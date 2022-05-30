//
// Created by danielsf97 on 3/4/20.
//

#ifndef P2PFS_KV_STORE_LEVELDB_H
#define P2PFS_KV_STORE_LEVELDB_H

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
    leveldb::DB* db_deleted;

    std::atomic<long> record_count = 0;
    std::atomic<long> record_count_cycle = 0;
    std::atomic<long> deleted_record_count = 0;
    std::atomic<long> deleted_record_count_cycle = 0;
    inline const static long record_refresh_rate = 10;
    inline const static int anti_entropy_max_keys = 20;
    inline const static int anti_entropy_num_keys_percentage = 80;
    inline const static int anti_entropy_num_deleted_keys_percentage = 20;


private:
    void refresh_nr_keys_count();
    void refresh_nr_deleted_keys_count();
    int open(leveldb::Options& options, std::string db_name, std::string& db_name_path, leveldb::DB** db);
    void print_db(leveldb::DB* database, long id, std::string filename);
    std::unique_ptr<std::string> get_db(kv_store_key<std::string>& key, leveldb::DB* database);
    std::unique_ptr<std::vector<kv_store_key_version>> get_latest_version_db(const std::string& key, leveldb::DB* database);


public:
    ~kv_store_leveldb();
    int restart_database() override;
    void send_keys_gt(std::vector<std::string> &off_keys, std::vector<std::string> &off_deleted_keys, tcp_client_server_connection::tcp_client_connection &connection,
                      void (*action)(tcp_client_server_connection::tcp_client_connection &, const std::string &, std::map<long, long>& , bool, bool, const char*, size_t)) override;
    kv_store_leveldb(std::string(*f)(const std::string&, const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at);
    int init(void*, long id) override ;
    void close() override ;
    std::string db_name() const override;
    std::vector<std::string> get_last_keys_limit4() override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<std::string>> get_keys() override;
    bool put(const std::string& key, kv_store_key_version version, const std::string& bytes, bool is_merge) override;
    bool put_with_merge(const std::string& key, kv_store_key_version version, const std::string& bytes) override;
    bool anti_entropy_put(const std::string& key, kv_store_key_version version, const std::string& value, bool is_merge) override;
    std::unique_ptr<std::string> get(kv_store_key<std::string>& key) override;
    std::unique_ptr<std::string> get_deleted(kv_store_key<std::string>& key) override;
    bool put_deleted(const std::string& key, kv_store_key_version version, const std::string& value) override;
    bool anti_entropy_remove(const std::string& key, kv_store_key_version version, const std::string& value) override;
    bool remove(const std::string& key, kv_store_key_version version) override;
    std::unique_ptr<std::vector<kv_store_key_version>> get_latest_version(const std::string& key) override;
    std::unique_ptr<std::vector<kv_store_key_version>> get_latest_deleted_version(const std::string& key) override;
    std::unique_ptr<std::vector<kv_store_key_version>> get_latest_data_version(const std::string& key, std::vector<std::unique_ptr<std::string>>& last_data) override;

    std::unique_ptr<std::string> get_anti_entropy(const kv_store_key<std::string>& key, bool* is_merge) override;
    void remove_from_set_existent_keys(std::unordered_set<kv_store_key<std::string>>& keys) override;
    void remove_from_set_existent_deleted_keys(std::unordered_set<kv_store_key<std::string>>& deleted_keys) override;
    void print_store(long id) override;
    bool check_if_deleted(const std::string& key, kv_store_key_version version) override;
    bool check_if_put_merged(const std::string& key, kv_store_key_version version) override;
    bool check_if_put_merged(const std::string& comp_key) override;
    std::unordered_set<kv_store_key<std::string>> get_deleted_keys() override;
};

kv_store_leveldb::kv_store_leveldb(std::string (*f)(const std::string&,const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at) {
    this->merge_function = f;
    this->seen_log_garbage_at = seen_log_garbage_at;
    this->request_log_garbage_at = request_log_garbage_at;
    this->anti_entropy_log_garbage_at = anti_entropy_log_garbage_at;
}

kv_store_leveldb::~kv_store_leveldb() {
    delete db;
    delete db_deleted;
}

void kv_store_leveldb::close() {
    spdlog::debug("closing connection");

    delete this->db;
    delete this->db_merge_log;
    delete this->db_deleted;
}

std::string kv_store_leveldb::db_name() const {
    return "levelDB";
}

int kv_store_leveldb::restart_database() {
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
    std::string db_delete_name = db_name + "_deleted";

    int res = 0;

    res = open(options, "DB", db_name, &db);
    if(res == -1) return -1;
    res = open(options, "DB_MERGE", db_merge_log_name, &db_merge_log);
    if(res == -1) return -1;
    res = open(options, "DB_DELETED", db_delete_name, &db_deleted);

    std::cout << "All done" << std::endl;

    return res;
}

int kv_store_leveldb::open(leveldb::Options& options, std::string db_name, std::string& db_name_path, leveldb::DB** db){
    
    std::cout << "Opening " << db_name  <<" Database" << std::endl;

    leveldb::Status status = leveldb::DB::Open(options, db_name_path, db);

    if (!status.ok())
    {
        fprintf(stderr,
                "Unable to open/create test database db %s\n",
                db_name.c_str());
        return -1;
    }

    return 0;
}


std::vector<std::string> kv_store_leveldb::get_last_keys_limit4(){

    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    std::set<std::string> temp;
    for (it->SeekToLast(); it->Valid() && temp.size() < 4; it->Prev()) {
        std::string comp_key = it->key().ToString();
        std::string key;
        int res = split_composite_key(comp_key, &key);
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

void kv_store_leveldb::update_partition(int p, int np) {
    if(np != this->nr_slices){
        std::cout << "update_partition " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        this->clear_seen_log();
    }
}

std::unordered_set<kv_store_key<std::string>> kv_store_leveldb::get_keys() {

    long cycle = record_count_cycle++;
    if(cycle % record_refresh_rate == 0){
        cycle = 0;
        this->refresh_nr_keys_count();
    }
    //Calculate percentage of keys to send
    long anti_entropy_num_keys = (anti_entropy_max_keys * anti_entropy_num_keys_percentage);

    long max_random_key_start = this->record_count - anti_entropy_num_keys;

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<long> distr(0, max_random_key_start);
    long key_start = distr(eng);

    std::unordered_set<kv_store_key<std::string>> keys;
    
    //Itero pela db ate chegar a key random escolhida para começar
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next(), --key_start) {}
    
    //A partir dessa iterator(key), guardar anti_entropy_max_keys para enviar
    for(int i = 0; i < anti_entropy_num_keys && it->Valid(); i++, it->Next()){
        std::string comp_key = it->key().ToString();
        
        bool is_merge = check_if_put_merged(comp_key);

        std::string key;
        std::map<long, long> vector;
        int res = split_composite_total(comp_key, &key, &vector);
        if(res == 0){
            keys.insert({key, kv_store_key_version(vector), false, is_merge});
        }
    }

    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }

    delete it;
    return std::move(keys);
}


std::unordered_set<kv_store_key<std::string>> kv_store_leveldb::get_deleted_keys() {

    long cycle = deleted_record_count_cycle++;
    if(cycle % record_refresh_rate == 0){
        cycle = 0;
        this->refresh_nr_deleted_keys_count();
    }
    //Calculate percentage of keys to send
    long anti_entropy_num_deleted_keys = (anti_entropy_max_keys * anti_entropy_num_deleted_keys_percentage);

    long max_random_key_start = this->deleted_record_count - anti_entropy_num_deleted_keys;

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<long> distr(0, max_random_key_start);
    long key_start = distr(eng);

    std::unordered_set<kv_store_key<std::string>> deleted_keys;
    
    //Itero pela db ate chegar a key random escolhida para começar
    leveldb::Iterator* it = db_deleted->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next(), --key_start) {}
    
    //A partir dessa iterator(key), guardar anti_entropy_max_keys para enviar
    for(int i = 0; i < anti_entropy_num_deleted_keys && it->Valid(); i++, it->Next()){
        std::string comp_key = it->key().ToString();
        std::string key;
        std::map<long, long> vector;
        int res = split_composite_total(comp_key, &key, &vector);
        if(res == 0){
            deleted_keys.insert({key, kv_store_key_version(vector), true});
        }
    }

    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }

    delete it;
    return std::move(deleted_keys);
}


void kv_store_leveldb::remove_from_set_existent_keys(std::unordered_set<kv_store_key<std::string>>& keys){
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    leveldb::Iterator* it_del = db_deleted->NewIterator(leveldb::ReadOptions());

    std::string prefix;
    for (auto it_keys = keys.begin(); it_keys != keys.end();) {
        prefix.clear();
        prefix = compose_key_toString(it_keys->key, it_keys->key_version);
        it->Seek(prefix);
        it_del->Seek(prefix);
        if(it->Valid() && it->key().ToString() == prefix){
            // if we hold the key
            it_keys = keys.erase(it_keys);
        }else{
            //if we do not hold the key, but can be deleted, have to check
            it_del->Seek(prefix);
            if (it_del->Valid() && it_del->key().ToString() == prefix){
                //if the key was deleted, do not need to request
                it_keys = keys.erase(it_keys);
            }else if(this->get_slice_for_key(it_keys->key) != this->slice){
                // if the key does not belong to this slice
                it_keys = keys.erase(it_keys);
            }else{
                ++it_keys;
            }
        }
    }
        

    delete it;
    delete it_del;
}


void kv_store_leveldb::remove_from_set_existent_deleted_keys(std::unordered_set<kv_store_key<std::string>>& deleted_keys){
    leveldb::Iterator* it = db_deleted->NewIterator(leveldb::ReadOptions());

    std::string prefix;
    for (auto it_keys = deleted_keys.begin(); it_keys != deleted_keys.end();) {
        prefix.clear();
        prefix = compose_key_toString(it_keys->key, it_keys->key_version);
        it->Seek(prefix);
        if(it->Valid() && it->key().ToString() == prefix){
            // if we hold the key
            it_keys = deleted_keys.erase(it_keys);
        }else if(this->get_slice_for_key(it_keys->key) != this->slice){
            // if the key does not belong to this slice
            it_keys = deleted_keys.erase(it_keys);
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
        return;
    }

    delete it;
    record_count = count;
}


void kv_store_leveldb::refresh_nr_deleted_keys_count(){
    long count = 0;

    leveldb::Iterator* it = db_deleted->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        count++;
    }

    if(!it->status().ok()){
        delete it;
        return;
    }

    delete it;
    deleted_record_count = count;
}

std::unique_ptr<std::vector<kv_store_key_version>> kv_store_leveldb::get_latest_version(const std::string& key) {
    return get_latest_version_db(key, db);
}


std::unique_ptr<std::vector<kv_store_key_version>> kv_store_leveldb::get_latest_deleted_version(const std::string& key) {
    return get_latest_version_db(key, db_deleted);
}

std::unique_ptr<std::vector<kv_store_key_version>> kv_store_leveldb::get_latest_version_db(const std::string& key, leveldb::DB* database) {
    leveldb::Iterator* it = database->NewIterator(leveldb::ReadOptions());
    std::string prefix = key + "#";
    bool exists = false;

    std::vector<kv_store_key_version> current_max_versions;
    int i = 0;
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next(), i++) {
        std::string comp_key = it->key().ToString();
        std::string current_key;
        std::map<long, long> current_vector;
        int res = split_composite_total(comp_key, &current_key, &current_vector);
        if(res == 0){
            if(i == 0){
                current_max_versions.emplace_back(kv_store_key_version(current_vector));
                exists = true;
            } else {
                auto temp_version = kv_store_key_version(current_vector);
                int cout_concurrent = 0;
                for(int j = 0; j < current_max_versions.size(); j++){
                    kVersionComp vcomp = comp_version(temp_version, current_max_versions.at(j));
                    if(vcomp == kVersionComp::Bigger){
                        current_max_versions.at(j) = temp_version;
                        break;
                    }
                    else if(vcomp == kVersionComp::Concurrent)
                        cout_concurrent++;
                }
                if(cout_concurrent == current_max_versions.size())
                    current_max_versions.emplace_back(temp_version);
            }
        }
    }
        
    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }else if(exists){
        delete it;
        return std::make_unique<std::vector<kv_store_key_version>>(current_max_versions);
    }
    delete it;
    return nullptr;
}

std::unique_ptr<std::vector<kv_store_key_version>> kv_store_leveldb::get_latest_data_version(const std::string& key, std::vector<std::unique_ptr<std::string>>& last_data){
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    std::string prefix = key + "#";
    bool exists = false;

    std::vector<kv_store_key_version> current_max_versions;
    int i = 0;
    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next(), i++) {
        std::string comp_key = it->key().ToString();
        std::string value = it->value().ToString();
        std::string current_key;
        std::map<long, long> current_vector;
        int res = split_composite_total(comp_key, &current_key, &current_vector);
        if(res == 0){
            if(i == 0){
                current_max_versions.emplace_back(kv_store_key_version(current_vector));
                last_data.emplace_back(std::make_unique<std::string>(std::move(value)));
                exists = true;
            } else {
                auto temp_version = kv_store_key_version(current_vector);
                int cout_concurrent = 0;
                for(int j = 0; j < current_max_versions.size(); j++){
                    kVersionComp vcomp = comp_version(temp_version, current_max_versions.at(j));
                    if(vcomp == kVersionComp::Bigger){
                        current_max_versions.at(j) = temp_version;
                        last_data.at(j) = std::make_unique<std::string>(std::move(value));
                        break;
                    }
                    else if(vcomp == kVersionComp::Concurrent)
                        cout_concurrent++;
                }
                if(cout_concurrent == current_max_versions.size()){
                    current_max_versions.emplace_back(temp_version);
                    last_data.emplace_back(std::make_unique<std::string>(std::move(value)));
                }
            }
        }        
    }
        
    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }else if(exists){
        delete it;
        return std::make_unique<std::vector<kv_store_key_version>>(current_max_versions);
    }
    delete it;
    return nullptr;
}

bool kv_store_leveldb::put(const std::string& key, kv_store_key_version version, const std::string& bytes, bool is_merge) {
    kv_store_key<std::string> key_comp = {key, version, false, is_merge};
    this->seen_it(key_comp);
    int k_slice = this->get_slice_for_key(key);

    if(this->slice == k_slice){
        leveldb::WriteOptions writeOptions;
        std::string comp_key;
        //comp_key.reserve(100);
        comp_key = compose_key_toString(key, version);
        db->Put(writeOptions, comp_key, bytes);
        if(is_merge) {
            db_merge_log->Put(writeOptions, comp_key, std::to_string(is_merge));
        }
        this->record_count++;
        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}


void kv_store_leveldb::print_store(long id){

    std::cout << "\nPrinting DB" << std::endl;

    print_db(db, id, "db_");
    
    std::cout << "\nPrinting DB_DELETED" << std::endl;
    
    print_db(db_deleted, id, "deleted_db_");
    
    std::cout << std::endl;
}



void kv_store_leveldb::print_db(leveldb::DB* database, long id, std::string filename){

    leveldb::Iterator* it = database->NewIterator(leveldb::ReadOptions());
    std::string filename2 = filename + to_string(id);
    std::ofstream db_file(filename2);

    db_file << "###########################################################"<< "\n";
    db_file << "###################### LSFS DATABASE ######################"<< "\n";
    db_file << "###########################################################"<< "\n";
    
    for (it->SeekToFirst(); it->Valid(); it->Next())
    {
        std::string comp_key = it->key().ToString();
        db_file << " Complete Key " << comp_key << "\n";
    /*
        std::string key;
        std::map<long, long> vector;
        int res = split_composite_total(comp_key, &key, &vector);

        if(res == 0){
            db_file << "  Key: " << key << "#" << "\n";
            
            kv_store_key<std::string> get_key = {key, kv_store_key_version(vector)};
            std::unique_ptr<std::string> data = get_db(get_key, database);

            if(data != nullptr) db_file << "  Data: " << *data << "\n";
            else db_file << "  Data: **NULLPTR**"<< "\n";
        }
        */
    }
    db_file.close();

    if (!it->status().ok())
    {
        std::cerr << "An error was found during the scan" << std::endl;
        std::cerr << it->status().ToString() << std::endl;
    }
}

std::unique_ptr<std::string> kv_store_leveldb::get_db(kv_store_key<std::string>& key, leveldb::DB* database) {
    
    //It shoud never happen this if, only safeguard 
    if(key.key_version.vv.empty())
        std::cout << "ERROR - Empty vector " << std::endl;
    
    std::string value;
    std::string comp_key;
    comp_key = compose_key_toString(key.key, key.key_version);
    
    leveldb::Status s = database->Get(leveldb::ReadOptions(), comp_key, &value);

    if (s.ok()){
        return std::make_unique<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}


std::unique_ptr<std::string> kv_store_leveldb::get(kv_store_key<std::string>& key) {
    return get_db(key, db);
}

std::unique_ptr<std::string> kv_store_leveldb::get_deleted(kv_store_key<std::string>& key) {
    return get_db(key, db_deleted);
}

std::unique_ptr<std::string> kv_store_leveldb::get_anti_entropy(const kv_store_key<std::string>& key, bool* is_merge) {
    if(key.is_deleted){
        std::cout << "################### Get Anti-entropy Message - Get Deleted" << std::endl;

        *is_merge = false;
        return this->get_deleted(const_cast<kv_store_key<std::string> &>(key));
    }else{
        std::string value;
        std::string comp_key;
        comp_key = compose_key_toString(key.key, key.key_version);
        leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
        *is_merge = s.ok();

        return this->get(const_cast<kv_store_key<std::string> &>(key));
    }
}


bool kv_store_leveldb::put_deleted(const std::string& key, kv_store_key_version version, const std::string& value){
    
    int k_slice = this->get_slice_for_key(key);

    if(this->slice == k_slice){
        
        std::string comp_key;
        comp_key = compose_key_toString(key, version);
        leveldb::Status s = db_deleted->Put(leveldb::WriteOptions(), comp_key, value);
        if(!s.ok()) throw LevelDBException();
        this->deleted_record_count++;

        return true;
    }
    return false;
}



bool kv_store_leveldb::anti_entropy_remove(const std::string& key, kv_store_key_version version, const std::string& value){
        
    if(!remove(key, version))
        return put_deleted(key, version, value);

    return true;
}


bool kv_store_leveldb::anti_entropy_put(const std::string& key, kv_store_key_version version, const std::string& value, bool is_merge){
    kv_store_key<std::string> key_comp = {key, version, false, is_merge};
    this->seen_it(key_comp);
    int k_slice = this->get_slice_for_key(key);   

    if(this->slice == k_slice){
        std::string value;
        std::string comp_key;
        comp_key = compose_key_toString(key, version);
        leveldb::Status s = db_deleted->Get(leveldb::ReadOptions(), comp_key, &value);
        //if it is not found in the deleted records
        if (!s.ok()){
            std::string comp_key;
            comp_key = compose_key_toString(key, version);
            db->Put(leveldb::WriteOptions(), comp_key, value);
            if(is_merge) {
                db_merge_log->Put(leveldb::WriteOptions(), comp_key, std::to_string(is_merge));
            }
            this->record_count++;
            return true;
        }
    }
    return false;
    
}


bool kv_store_leveldb::remove(const std::string& key, kv_store_key_version version) {
    kv_store_key<std::string> key_comp = {key, version, true};
    this->seen_it_deleted(key_comp);
    int k_slice = this->get_slice_for_key(key);

    if(this->slice == k_slice){
        std::string value;
        std::string comp_key;
        comp_key = compose_key_toString(key, version);
        std::cout << "A chave é: " << comp_key << std::endl;
        leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

        if (s.ok()){
            std::cout << "Tenho a chave" << std::endl;

            s = db->Delete(leveldb::WriteOptions(), comp_key);
            if(!s.ok())throw LevelDBException();
            this->record_count--; //removed record from database
            
            std::string value2;
            s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value2);
            if(s.ok()){
                s = db_merge_log->Delete(leveldb::WriteOptions(), comp_key);
                if(!s.ok()) throw LevelDBException();
            }

            s = db_deleted->Put(leveldb::WriteOptions(), comp_key, value);
            if(!s.ok()) throw LevelDBException();
            this->deleted_record_count++;

            return true;
        }
    }
    return false;
}


bool kv_store_leveldb::check_if_deleted(const std::string& key, kv_store_key_version version){
    leveldb::Iterator *it = db_deleted->NewIterator(leveldb::ReadOptions());
    std::string prefix = key + "#";
    bool exists = false;

    for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
        std::string comp_key = compose_key_toString(key, version);
        if(comp_key == it->key().ToString()){
            return true;
        }
    }
    return false;
}


bool kv_store_leveldb::check_if_put_merged(const std::string& key, kv_store_key_version version){
    std::string comp_key = compose_key_toString(key, version);
    std::string value;
    leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
    bool is_merge = s.ok();
    return is_merge;
}

bool kv_store_leveldb::check_if_put_merged(const std::string& comp_key){
    std::string value;
    leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
    bool is_merge = s.ok();
    return is_merge;
}


bool kv_store_leveldb::put_with_merge(const std::string& key, kv_store_key_version version, const std::string& bytes) {
    try{

        //This vector should only have one kv_store_key_version
        //if it has more, something was wrong 

        std::unique_ptr<std::vector<kv_store_key_version>> last_vkv = get_latest_version(key);
        std::vector<std::unique_ptr<std::string>> last_vdata;

        for(auto &kv: *last_vkv){
            kv_store_key<std::string> key_v = {key, kv};
            last_vdata.emplace_back(get(key_v));
        }

        kv_store_key_version last_kv;
        std::unique_ptr<std::string> last_data;
        
        //if no key was found in the system, just insert it with merge = true
        if( last_vkv == nullptr || last_vkv->size() <= 0){
            return put(key, version, bytes, true);
        }
        // if the vector has more than one version, just merge everything in the vector
        // 
        else if(last_vkv->size() > 1){
            last_kv = merge_vkv(*last_vkv);
            last_data = std::move(last_vdata.front());
            int i = 0;
            for(auto const & ptr_d : last_vdata){
                if(i > 0)
                    last_data = std::make_unique<std::string>(this->merge_function(*last_data, *ptr_d));
                i++;
            }
        }
        //retrieve first and unique elem
        else {
            last_kv = last_vkv->front();
        }

        std::cout << "Checking if version and last version are concurrent" << std::endl;
                
        // They should always be concurrent
        if(comp_version(last_kv, version) == kVersionComp::Concurrent){
            this->record_count--; //put of merge_version shoudnt increment record count, its just an overwrite
            int res = put(key, merge_kv(last_kv, version), this->merge_function(*last_data, bytes), true);
            if(res){
                //Ensure received version stays logged in the database. (for anti-entropy reasons)
                return put(key, version, bytes, true);
            }
            else {
                return false;
            }
        }
        // if not just insert with merge = true
        else {
            std::cout << "Just inserting the key with updated metadata" << std::endl;
            return put(key, version, bytes, true);
        }

    }
    catch(LevelDBException& e){
        return false;
    }
}

void kv_store_leveldb::send_keys_gt(std::vector<std::string> &off_keys, std::vector<std::string> &off_deleted_keys, tcp_client_server_connection::tcp_client_connection &connection,
                                    void (*send)(tcp_client_server_connection::tcp_client_connection &, const std::string &, std::map<long, long>&, bool, bool, const char*, size_t)) {

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
        std::map<long, long> current_vector;
        split_composite_total(comp_key, &current_key, &current_vector);
        leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
        bool is_merge = s.ok();
        try {
            send(connection, current_key, current_vector, false, is_merge, it->value().data(), it->value().size());
        }catch(std::exception& e){
            std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
        }
    }

    if (!it->status().ok()) {
        delete it;
        throw LevelDBException();
    } else {
        delete it;
    }


    it = db_deleted->NewIterator(leveldb::ReadOptions());
    found_offset_key = false;
    if(off_deleted_keys.empty()){
        it->SeekToFirst();
    }else {
        for (auto key_it = off_deleted_keys.begin(); key_it != off_deleted_keys.end() && !found_offset_key; ++key_it) {
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
        std::map<long, long> current_vector;
        split_composite_total(comp_key, &current_key, &current_vector);
        try {
            send(connection, current_key, current_vector, true, false, it->value().data(), it->value().size());
        }catch(std::exception& e){
            std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
        }
    }

    if (!it->status().ok()) {
        delete it;
        throw LevelDBException();
    } else {
        delete it;
    }
}

#endif //P2PFS_KV_STORE_LEVELDB_H

