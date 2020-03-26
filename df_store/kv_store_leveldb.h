//
// Created by danielsf97 on 3/4/20.
//

#ifndef P2PFS_KV_STORE_LEVELDB_H
#define P2PFS_KV_STORE_LEVELDB_H

#include <wiredtiger.h>
#include <leveldb/db.h>
#include "exceptions/custom_exceptions.h"
#include "df_util/util.h"
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

private:
    std::unique_ptr<long> get_client_id_from_key_version(std::string key, long version);

public:
    ~kv_store_leveldb();
    explicit kv_store_leveldb(std::string(*f)(std::string&, std::string&));
    int init(void*, long id) override ;
    void close() override ;
    std::string db_name() const override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<std::string>> get_keys() override;
    bool put(std::string key, long version, long client_id, std::string bytes) override; // use string.c_str() to convert string to const char*
    bool put_with_merge(std::string key, long version, long client_id, std::string bytes);
    std::shared_ptr<std::string> get(kv_store_key<std::string>& key) override;
    std::shared_ptr<std::string> remove(kv_store_key<std::string> key) override;
    std::shared_ptr<std::string> get_latest(std::string key, kv_store_key_version* kv_version) override;
    std::unique_ptr<long> get_latest_version(std::string key) override;
    void print_store() override;
};

kv_store_leveldb::kv_store_leveldb(std::string (*f)(std::string&, std::string&)) {
    this->merge_function = f;
}

kv_store_leveldb::~kv_store_leveldb() {
    delete db;
}

void kv_store_leveldb::close() {
    std::cout << "closing connection" << std::endl;
    delete db;
}

std::string kv_store_leveldb::db_name() const {
    return "levelDB";
}

int kv_store_leveldb::init(void* path, long id){
    this->id = id;
    this->path = std::string((char*) path);

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
            long client_id;
            int res = split_composite_key(comp_key, &key, &version, &client_id);
            if(res == 0){
                kv_store_key<std::string> key_to_insert({key, kv_store_key_version(version, client_id)});
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
        long client_id;
        int res = split_composite_key(comp_key, &key, &version, &client_id);
        if(res == 0){
            keys.insert({key, kv_store_key_version(version, client_id)});
        }
    }

    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }

    delete it;
    return std::move(keys);
}

std::unique_ptr<long> kv_store_leveldb::get_client_id_from_key_version(std::string key, long version) {
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    std::string prefix = key + "#" + std::to_string(version) + "#";
    bool exists = false;

    auto current_max_version = kv_store_key_version(version);
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
        std::string comp_key = it->key().ToString();
        std::string current_key;
        long current_version;
        long current_client_id;
        int res = split_composite_key(comp_key, &current_key, &current_version, &current_client_id);
        std::cout << "version: " << current_version << " client_id: " << current_client_id << std::endl;
        auto temp_version = kv_store_key_version(current_version, current_client_id);
        if(res == 0 && temp_version >= current_max_version){
            current_max_version = temp_version;
            exists = true;
        }
    }

    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }else if(exists){
        delete it;
        return std::make_unique<long>(current_max_version.client_id);
    }

    delete it;
    return nullptr;
}

//puts in leveldb are async, but there is a way to sync if its necessary
bool kv_store_leveldb::put(std::string key, long version, long client_id, std::string bytes) {

    this->seen_it(key, version, client_id);
    int k_slice = this->get_slice_for_key(key);

    if(this->slice == k_slice){
        leveldb::WriteOptions writeOptions;
        std::string comp_key = key + "#" + std::to_string(version) + "#" + std::to_string(client_id);
        db->Put(writeOptions, comp_key, bytes);

        try{
            std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
            if(max_client_id == nullptr){
                return false;
            }else{
                return *max_client_id == client_id;
            }
        }catch(LevelDBException& e){
            return false;
        }
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
        long client_id;
        int res = split_composite_key(comp_key, &key, &version, &client_id);
        if(res == 0){
            std::cout << key << ": " << version << "#" << client_id << std::endl;
        }
    }

    if (!it->status().ok())
    {
        std::cerr << "An error was found during the scan" << std::endl;
        std::cerr << it->status().ToString() << std::endl;
    }
}

std::shared_ptr<std::string> kv_store_leveldb::get(kv_store_key<std::string>& key) {

    if(key.key_version.client_id == -1){
        std::unique_ptr<long> current_max_client_id(nullptr);
        try{
            current_max_client_id = get_client_id_from_key_version(key.key, key.key_version.version);
        }catch(LevelDBException& e){}

        if(current_max_client_id == nullptr){
            return nullptr;
        }

        key.key_version.client_id = *current_max_client_id;
    }

    std::string value;
    std::string comp_key = key.key + "#" + std::to_string(key.key_version.version) + "#" + std::to_string(key.key_version.client_id);
    leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

    if (s.ok()){
        return std::make_shared<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}

std::shared_ptr<std::string> kv_store_leveldb::get_latest(std::string key, kv_store_key_version* kv_version) {

    leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
    std::string prefix = key + "#";
    bool exists = false;

    auto current_max_version = kv_store_key_version(LONG_MIN);
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
        std::string comp_key = it->key().ToString();
        std::string current_key;
        long current_version;
        long current_client_id;
        int res = split_composite_key(comp_key, &current_key, &current_version, &current_client_id);
        std::cout << "version: " << current_version << " client_id: " << current_client_id << std::endl;
        auto temp_version = kv_store_key_version(current_version, current_client_id);
        if (res == 0 && temp_version >= current_max_version) {
            current_max_version = temp_version;
            exists = true;
        }
    }

    if (!it->status().ok()) {
        delete it;
        throw LevelDBException();
    } else if (exists) {
        delete it;
        *kv_version = current_max_version;
        kv_store_key<std::string> get_key = {key, current_max_version};
        return get(get_key);
    }

    delete it;
    return nullptr;
}

std::unique_ptr<long> kv_store_leveldb::get_latest_version(std::string key) {
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());

    std::string prefix = key + "#";
    bool exists = false;
    long current_max_version = LONG_MIN;
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
        std::string comp_key = it->key().ToString();
        std::string current_key;
        long current_version;
        long current_client_id;
        int res = split_composite_key(comp_key, &current_key, &current_version, &current_client_id);
        if(res == 0 && current_version >= current_max_version){
            current_max_version = current_version;
            exists = true;
        }
    }

    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }else if(exists){
        delete it;
        return std::make_unique<long>(current_max_version);
    }

    delete it;
    return nullptr;
}

std::shared_ptr<std::string> kv_store_leveldb::remove(kv_store_key<std::string> key) {

    std::string value;
    std::string comp_key = key.key + "#" + std::to_string(key.key_version.version) + "#" + std::to_string(key.key_version.client_id);
    leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

    if (s.ok()){
        s = db->Delete(leveldb::WriteOptions(), comp_key);
        if(!s.ok()) throw LevelDBException();

        return std::make_shared<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}

bool kv_store_leveldb::put_with_merge(std::string key, long version, long client_id, std::string bytes) {
    try{
        std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
        if(max_client_id == nullptr){
            //no conflict
            return put(key, version, client_id, bytes);
        }else{
            if(*max_client_id != client_id){
                kv_store_key<std::string> kv_key = {key, kv_store_key_version(version, *max_client_id)};
                std::shared_ptr<std::string> data = get(kv_key);
                if(data == nullptr){
                    // caso ocorresse algum erro
                    return false;
                }
                return put(key, version, std::max(*max_client_id, client_id), this->merge_function(*data, bytes));
            }
        }

        return true;
    }catch(LevelDBException& e){
        return false;
    }
}

#endif //P2PFS_KV_STORE_LEVELDB_H

