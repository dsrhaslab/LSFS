//
// Created by danielsf97 on 3/4/20.
//

#ifndef P2PFS_KV_STORE_LEVELDB_H
#define P2PFS_KV_STORE_LEVELDB_H

#include <wiredtiger.h>
#include <leveldb/db.h>
#include "../exceptions/custom_exceptions.h"
#include "../df_util/util.h"
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <memory>
#include "kv_store_key.h"
#include "kv_store.h"
#include <climits>
#include <iostream>
#include <cstring>
#include <functional>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
namespace fs = std::filesystem;

class kv_store_leveldb: public kv_store<std::string>{
private:
    leveldb::DB* db;
    long id;
    std::string path;
    std::atomic<int> slice = 1; //[1, nr_slices]
    std::atomic<int> nr_slices = 1;
    std::unordered_map<kv_store_key<std::string>, bool> seen;
    std::unordered_map<std::string, bool> request_log;
    std::unordered_map<std::string, bool> anti_entropy_log;
    std::recursive_mutex seen_mutex;
    std::recursive_mutex req_log_mutex;
    std::recursive_mutex anti_entropy_log_mutex;

public:
    ~kv_store_leveldb();
    int init(void*, long id) override ;
    void close() override ;
    int get_slice_for_key(std::string key) override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<std::string>> get_keys() override;
    bool have_seen(std::string key, long version) override;
    void seen_it(std::string key, long version) override;
    bool put(std::string key, long version, std::string bytes) override; // use string.c_str() to convert string to const char*
    std::shared_ptr<std::string> get(kv_store_key<std::string> key) override;
    std::shared_ptr<std::string> remove(kv_store_key<std::string> key) override;
    int get_slice() override;
    void set_slice(int slice) override;
    int get_nr_slices() override;
    void set_nr_slices(int nr_slices) override;
    bool in_log(std::string req_id);
    void log_req(std::string req_id);
    bool in_anti_entropy_log(std::string req_id);
    void log_anti_entropy_req(std::string req_id);
    void print_store();
};

kv_store_leveldb::~kv_store_leveldb() {
    delete db;
}

void kv_store_leveldb::close() {
    std::cout << "closing connection" << std::endl;
    delete db;
}

int kv_store_leveldb::init(void* path, long id){
    this->id = id;
    this->path = std::string((char*) path);
//    std::string database_path = (char*) path +  std::to_string(id);
//    try{
//        std::filesystem::create_directories(database_path.c_str());
//    }catch (std::exception& e) {
//        std::cout << e.what() << std::endl;
//    }

    leveldb::Options options;
    options.create_if_missing = true;
    std::string db_name = this->path + std::to_string(id);

    leveldb::Status status = leveldb::DB::Open(options, db_name, &db);

    if (!status.ok())
    {
        fprintf(stderr,
                "Unable to open/create test database %s\n",
                db_name.c_str());
        return -1;
    }

    return 0;
}

int kv_store_leveldb::get_slice_for_key(std::string key) {
    if(nr_slices == 1) return 1;

    size_t max = SIZE_MAX;
    size_t min = 0;
    size_t target = std::hash<std::string>()(key);
    size_t step = (max / this->nr_slices);

    size_t current = min;
    int slice = 1;
    size_t next_current = current + step;

    while (target > next_current){
        current = next_current;
        next_current = current + step;
        if(current > 0 && next_current < 0) break; //in the case of overflow
        slice = slice + 1;
    }

    if(slice > this->nr_slices){
        slice = this->nr_slices - 1;
    }

    return slice;
}

void kv_store_leveldb::update_partition(int p, int np) {

    if(np != this->nr_slices){
        std::cout << "UPDATE_PARTITION " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);

        std::map<kv_store_key<std::string>, bool> keys;

        leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            std::string comp_key = it->key().ToString();
            std::string key;
            long version;
            int res = split_composite_key(comp_key, &key, &version);
            if(res == 0){
                kv_store_key<std::string> key_to_insert({key, version});
                keys.insert(std::make_pair(std::move(key_to_insert), false));
            }
        }

        if(!it->status().ok()){
            delete it;
            throw LevelDBException();
        }

        delete it;


        for(const auto& seen_pair: this->seen){
            if(keys.find(seen_pair.first) == keys.end()) { // a chave não existe
                this->seen.insert_or_assign(seen_pair.first, false);
            }
        }

    }
}

std::unordered_set<kv_store_key<std::string>> kv_store_leveldb::get_keys() {

    std::unordered_set<kv_store_key<std::string>> keys;

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string comp_key = it->key().ToString();
        std::string key;
        long version;
        int res = split_composite_key(comp_key, &key, &version);
        if(res == 0){
            keys.insert({key, version});
        }
    }

    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }

    delete it;
    return std::move(keys);
}

bool kv_store_leveldb::have_seen(std::string key, long version) {
    kv_store_key<std::string> key_to_check({key, version});

    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    auto it = this->seen.find(key_to_check);
    if(it == this->seen.end()){ //a chave não existe no mapa seen
        return false;
    }else{
        return it->second;
    }
}

void kv_store_leveldb::seen_it(std::string key, long version) {
    kv_store_key<std::string> key_to_insert({key, version});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    this->seen.insert_or_assign(std::move(key_to_insert), true);
}

//puts in leveldb are async, but there is a way to sync if its necessary
bool kv_store_leveldb::put(std::string key, long version, std::string bytes) {

    this->seen_it(key, version);
    int k_slice = this->get_slice_for_key(key);

    if(this->slice == k_slice){
        leveldb::WriteOptions writeOptions;
        std::string comp_key = key + ":" + std::to_string(version);
        db->Put(writeOptions, comp_key, bytes);
        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

void kv_store_leveldb::print_store(){

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());

    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::string comp_key = it->key().ToString();
        std::string key;
        long version;
        int res = split_composite_key(comp_key, &key, &version);
        if(res == 0){
            std::cout << key << ": " << version << std::endl;
        }
    }

    if (!it->status().ok())
    {
        std::cerr << "An error was found during the scan" << std::endl;
        std::cerr << it->status().ToString() << std::endl;
    }
}

std::shared_ptr<std::string> kv_store_leveldb::get(kv_store_key<std::string> key) {
    std::string value;
    std::string comp_key = key.key + ":" + std::to_string(key.version);
    leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

    if (s.ok()){
        return std::make_shared<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}

std::shared_ptr<std::string> kv_store_leveldb::remove(kv_store_key<std::string> key) {

    std::string value;
    std::string comp_key = key.key + ":" + std::to_string(key.version);
    leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

    if (s.ok()){
        s = db->Delete(leveldb::WriteOptions(), comp_key);
        if(!s.ok()) throw LevelDBException();

        return std::make_shared<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}

int kv_store_leveldb::get_slice() {
    return this->slice;
}

void kv_store_leveldb::set_slice(int slice) {
    this->slice = slice;
}

int kv_store_leveldb::get_nr_slices() {
    return this->nr_slices;
}

void kv_store_leveldb::set_nr_slices(int nr_slices) {
    this->nr_slices = nr_slices;
}

bool kv_store_leveldb::in_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    return !(this->request_log.find(req_id) == this->request_log.end());
}

void kv_store_leveldb::log_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    this->request_log.insert_or_assign(req_id, true);
}

bool kv_store_leveldb::in_anti_entropy_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    return !(this->anti_entropy_log.find(req_id) == this->anti_entropy_log.end());
}

void kv_store_leveldb::log_anti_entropy_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    this->anti_entropy_log.insert_or_assign(req_id, true);
}

#endif //P2PFS_KV_STORE_LEVELDB_H

