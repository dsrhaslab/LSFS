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

#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

/*
 * Notes on LevelDB
 * -> Put overwrite if key is present
 * -> Puts in leveldb are async, but there is a way to sync if its necessary
 * */

class kv_store_leveldb: public kv_store<std::string>{
private:
    leveldb::DB* db;
    leveldb::DB* db_merge_log;

    std::atomic<long> record_count = 0;
    std::atomic<long> record_count_cycle = 0;
    inline const static long record_refresh_rate = 10;
    inline const static int anti_entropy_max_keys = 20;

private:
    std::unique_ptr<long> get_client_id_from_key_version(const std::string& key, long version);
    void refresh_nr_keys_count();

public:
    ~kv_store_leveldb();
    int restart_database() override;
    void send_keys_gt(std::vector<std::string> &off_keys, tcp_client_server_connection::tcp_client_connection &connection,
                      void (*action)(tcp_client_server_connection::tcp_client_connection &, const std::string &, long, long, bool, const char*, size_t)) override;
    kv_store_leveldb(std::string(*f)(const std::string&, const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at);
    int init(void*, long id) override ;
    void close() override ;
    std::string db_name() const override;
    std::vector<std::string> get_last_keys_limit4() override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<std::string>> get_keys() override;
    bool put(const std::string& key, long version, long client_id, const std::string& bytes, bool is_merge) override; // use string.c_str() to convert string to const char*
    bool put_with_merge(const std::string& key, long version, long client_id, const std::string& bytes) override;
    std::unique_ptr<std::string> get(kv_store_key<std::string>& key) override;
    std::unique_ptr<std::string> remove(const kv_store_key<std::string>& key) override;
    std::unique_ptr<std::string> get_latest(const std::string& key, kv_store_key_version* kv_version) override;
    std::unique_ptr<long> get_latest_version(const std::string& key) override;
    std::unique_ptr<std::string> get_anti_entropy(const kv_store_key<std::string>& key, bool* is_merge) override;
    void remove_from_set_existent_keys(std::unordered_set<kv_store_key<std::string>>& keys) override;
    void print_store() override;
};

kv_store_leveldb::kv_store_leveldb(std::string (*f)(const std::string&,const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at) {
    this->merge_function = f;
    this->seen_log_garbage_at = seen_log_garbage_at;
    this->request_log_garbage_at = request_log_garbage_at;
    this->anti_entropy_log_garbage_at = anti_entropy_log_garbage_at;
}

kv_store_leveldb::~kv_store_leveldb() {
    delete db;
}

void kv_store_leveldb::close() {
    spdlog::debug("closing connection");

    delete this->db;
    std::cout << "Deleted db Database" << std::endl;
    delete this->db_merge_log;
    std::cout << "Deleted db_merge Database" << std::endl;
}

std::string kv_store_leveldb::db_name() const {
    return "levelDB";
}

int kv_store_leveldb::restart_database() {
    std::cout << "Restarting Database" << std::endl;
    this->close();
    return this->init((void *) this->path.c_str(), this->id);
}

int kv_store_leveldb::init(void* path, long id){
    std::cout << "Database Init" << std::endl;
    this->id = id;
    this->path = std::string((char*) path);

    std::filesystem::create_directories(this->path);

    leveldb::Options options;
    options.create_if_missing = true;
    std::string db_name = this->path + std::to_string(id);
    std::string db_merge_log_name = db_name + "_merge";

    std::cout << "Opening Database" << std::endl;

    leveldb::Status status = leveldb::DB::Open(options, db_name, &db);

    if (!status.ok())
    {
        fprintf(stderr,
                "Unable to open/create test database %s\n",
                db_name.c_str());
        return -1;
    }

    std::cout << "Opened db Database" << std::endl;

    status = leveldb::DB::Open(options, db_merge_log_name, &db_merge_log);

    std::cout << "Opened merge Database" << std::endl;

    if (!status.ok())
    {
        fprintf(stderr,
                "Unable to open/create test database %s\n",
                db_name.c_str());
        return -1;
    }

    std::cout << "All done" << std::endl;

    return 0;
}

std::vector<std::string> kv_store_leveldb::get_last_keys_limit4(){

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    std::set<std::string> temp;
    for (it->SeekToLast(); it->Valid() && temp.size() < 4; it->Prev()) {
        std::string comp_key = it->key().ToString();
        std::string key;
        long version;
        long client_id;
        int res = split_composite_key(comp_key, &key, &version, &client_id);
        if(res == 0){
            temp.emplace(key);
        }
    }

    std::vector<std::string> res;
    for (auto it = temp.begin(); it != temp.end(); ) {
        res.push_back(std::move(temp.extract(it++).value()));
    }
    return res;
}

//TODO este método tem de ser alterado está a criar um mapa com todas as chaves na base de dados
void kv_store_leveldb::update_partition(int p, int np) {

    if(np != this->nr_slices){
        std::cout << "UPDATE_PARTITION " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        this->clear_seen_log();
//        std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
//
//        std::map<kv_store_key<std::string>, bool> keys;
//
//        leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
//        for (it->SeekToFirst(); it->Valid(); it->Next()) {
//            std::string comp_key = it->key().ToString();
//            std::string key;
//            long version;
//            long client_id;
//            int res = split_composite_key(comp_key, &key, &version, &client_id);
//            if(res == 0){
//                kv_store_key<std::string> key_to_insert({key, kv_store_key_version(version, client_id)});
//                keys.insert(std::make_pair(std::move(key_to_insert), false));
//            }
//        }
//
//        if(!it->status().ok()){
//            delete it;
//            throw LevelDBException();
//        }
//
//        delete it;
//
//
//        for(const auto& seen_pair: this->seen){
//            if(keys.find(seen_pair.first) == keys.end()) { // a chave não existe
//                this->seen.insert_or_assign(seen_pair.first, false);
//            }
//        }

    }
}

std::unordered_set<kv_store_key<std::string>> kv_store_leveldb::get_keys() {

    long cycle = record_count_cycle++;
    if(cycle % record_refresh_rate == 0){
        cycle = 0;
        this->refresh_nr_keys_count();
    }

    long max_random_key_start = this->record_count - anti_entropy_max_keys;

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<long> distr(0, max_random_key_start);
    long key_start = distr(eng);

    std::unordered_set<kv_store_key<std::string>> keys;

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next(), --key_start) {}

    for(int i = 0; i < anti_entropy_max_keys && it->Valid(); i++, it->Next()){
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

void kv_store_leveldb::remove_from_set_existent_keys(std::unordered_set<kv_store_key<std::string>>& keys){
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());

    std::string prefix;
    for (auto it_keys = keys.begin(); it_keys != keys.end();) {
        prefix.clear();
        prefix.append(it_keys->key).append("#").append(std::to_string(it_keys->key_version.version)).append("#").append(std::to_string(it_keys->key_version.client_id));
        it->Seek(prefix);
        if(it->Valid() && it->key().ToString() == prefix){
            // se possuimos a chave
            it_keys = keys.erase(it_keys);
        }else if(this->get_slice_for_key(it_keys->key) != this->slice){
            // se a chave não pertence a esta store
            it_keys = keys.erase(it_keys);
        }else{
            ++it_keys;
        }
    }

    delete it;
}

void kv_store_leveldb::refresh_nr_keys_count(){
    long count = 0;

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        count++;
    }

    if(!it->status().ok()){
        delete it;
        return; //DO NOTHING
    }

    delete it;
    record_count = count;
}

std::unique_ptr<long> kv_store_leveldb::get_client_id_from_key_version(const std::string& key, long version) {
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

        if(res == 0){
//            long a = 23456789;
//            long b = 0;
//            std::cout << "version" << a << " client_id:" << b << std::endl;
            auto temp_version = kv_store_key_version(current_version, current_client_id);
            if(temp_version >= current_max_version){
                current_max_version = temp_version;
                exists = true;
            }
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

bool kv_store_leveldb::put(const std::string& key, long version, long client_id, const std::string& bytes, bool is_merge) {

    this->seen_it(key, version, client_id);
    int k_slice = this->get_slice_for_key(key);

    if(this->slice == k_slice){
        leveldb::WriteOptions writeOptions;
        std::string comp_key;
        comp_key.reserve(100);
        comp_key.append(key).append("#").append(std::to_string(version)).append("#").append(std::to_string(client_id));
        db->Put(writeOptions, comp_key, bytes);

        if(is_merge) {
            db_merge_log->Put(writeOptions, comp_key, std::to_string(is_merge));
        }
        this->record_count++;

//        if(!is_merge){
//            //if is not a merge operation should only respond that put was sucessfully if the
//            //version commited is actualy the current version
//
//            try{
//                std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
//                if(max_client_id == nullptr){
//                    return false;
//                }else{
//                    return  *max_client_id == client_id;
//                }
//            }catch(LevelDBException& e){
//                this->unseen_it(key, version, client_id);
//                throw LevelDBException();
//            }
//        }

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

std::unique_ptr<std::string> kv_store_leveldb::get(kv_store_key<std::string>& key) {

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
    std::string comp_key;
    comp_key.reserve(50);
    comp_key.append(key.key).append("#").append(std::to_string(key.key_version.version)).append("#").append(std::to_string(key.key_version.client_id));
    leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

    if (s.ok()){
        return std::make_unique<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}

std::unique_ptr<std::string> kv_store_leveldb::get_anti_entropy(const kv_store_key<std::string>& key, bool* is_merge) {
    std::string value;
    std::string comp_key;
    comp_key.reserve(50);
    comp_key.append(key.key).append("#").append(std::to_string(key.key_version.version)).append("#").append(std::to_string(key.key_version.client_id));
    leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
    *is_merge = s.ok();

    return this->get(const_cast<kv_store_key<std::string> &>(key));
}

std::unique_ptr<std::string> kv_store_leveldb::get_latest(const std::string& key, kv_store_key_version* kv_version) {

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
//        std::cout << "version: " << current_version << " client_id: " << current_client_id << std::endl;
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

std::unique_ptr<long> kv_store_leveldb::get_latest_version(const std::string& key) {
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

std::unique_ptr<std::string> kv_store_leveldb::remove(const kv_store_key<std::string>& key) {

    std::string value;
    std::string comp_key;
    comp_key.reserve(50);
    comp_key.append(key.key).append("#").append(std::to_string(key.key_version.version)).append("#").append(std::to_string(key.key_version.client_id));
    leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

    if (s.ok()){
        s = db->Delete(leveldb::WriteOptions(), comp_key);
        if(!s.ok())throw LevelDBException();
        this->record_count--; //removed record from database
        s = db_merge_log->Delete(leveldb::WriteOptions(), comp_key);
        if(!s.ok()) throw LevelDBException();

        return std::make_unique<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}

bool kv_store_leveldb::put_with_merge(const std::string& key, long version, long client_id, const std::string& bytes) {
    try{
//        std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
//
//        // quer exista ou não uma entrada para a mesma versão de outro cliente
//        // faz-se put na mesma da versão deste cliente para ter registo que a versão
//        // de tal cliente foi processada e por motivos de anti-entropia (não requerer
//        // versões-client_id já processados)
//        // É estritamente necessário o *max_client_id != client_id porque poderiamos estar a
//        // dar overwrite a uma versão merged.
//        if(max_client_id == nullptr || *max_client_id != client_id){
//            put(key, version, client_id, bytes, true);
//        }
//
//        if(max_client_id != nullptr){
//            if(*max_client_id != client_id){
//                kv_store_key<std::string> kv_key = {key, kv_store_key_version(version, *max_client_id)};
//                std::shared_ptr<std::string> data = get(kv_key);
//                if(data == nullptr){
//                    // caso ocorresse algum erro
//                    return false;
//                }
//                return put(key, version, std::max(*max_client_id, client_id), this->merge_function(*data, bytes), true);
//            }
//        }



        // quer exista ou não uma entrada para a mesma versão-client_id de outro cliente
        // faz-se put na mesma da versão deste cliente para ter registo que a versão
        // de tal cliente foi processada e por motivos de anti-entropia (não requerer
        // versões-client_id já processados)
        int res = put(key, version, client_id, bytes, true);
        if(res == false){
            return false;
        }

        kv_store_key_version kv_version;
        //We only need to merge with latest version
        std::unique_ptr<std::string> data = get_latest(key, &kv_version);
        if(data != nullptr){
            if(kv_version.version > version){
                //if there is a version bigger than mine, use its client_id
                this->record_count--; //put of merge_version shoudnt increment record count, its just an overwrite
                return put(key, version, client_id, this->merge_function(*data, bytes), true);
            }else if(kv_version.client_id != client_id){
                //if there is no version bigger than mine, use max client_id for merge
                this->record_count--; //put of merge_version shoudnt increment record count, its just an overwrite
                return put(key, version, std::max(kv_version.client_id, client_id), this->merge_function(*data, bytes), true);
            }

            //No need for merge, my version-client_id is bigger than the others
            return true;
        }else{
            // error on get
            throw LevelDBException();
        }
    }catch(LevelDBException& e){
        return false;
    }
}

void kv_store_leveldb::send_keys_gt(std::vector<std::string> &off_keys, tcp_client_server_connection::tcp_client_connection &connection,
                                    void (*send)(tcp_client_server_connection::tcp_client_connection &, const std::string &, long, long, bool, const char*, size_t)) {

    leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
    bool found_offset_key = false;
    if(off_keys.empty()){
        it->SeekToFirst();
    }else {
        for (auto key_it = off_keys.begin(); key_it != off_keys.end() && !found_offset_key; ++key_it) {
            std::string prefix = *key_it + "#";
            it->Seek(prefix);
            if (it->Valid() && it->key().starts_with(prefix)) {
                found_offset_key = true;
                break;
            } else {
                it->SeekToFirst();
            }
        }
    }

    for (; it->Valid(); it->Next()) {
        std::string comp_key = it->key().ToString();
        std::string current_key;
        std::string value;
        long current_version;
        long current_client_id;
        split_composite_key(comp_key, &current_key, &current_version, &current_client_id);
        leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
        bool is_merge = s.ok();
        send(connection, current_key, current_version, current_client_id, is_merge, it->value().data(), it->value().size());
    }

    if (!it->status().ok()) {
        delete it;
        throw LevelDBException();
    } else {
        delete it;
    }
}

#endif //P2PFS_KV_STORE_LEVELDB_H

