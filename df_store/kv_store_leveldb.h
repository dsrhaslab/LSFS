//
// Created by danielsf97 on 3/4/20.
//

#ifndef P2PFS_KV_STORE_LEVELDB_H
#define P2PFS_KV_STORE_LEVELDB_H

#include <leveldb/db.h>
#include "leveldb/write_batch.h"
#include <unordered_map>
#include <unordered_set>
#include <random>

#include "exceptions/custom_exceptions.h"
#include "kv_store.h"
#include "kv_store_key.h"
#include "kv_store_value/kv_store_value.h"
#include "lsfs/fuse_lsfs/metadata/metadata.h"
#include "df_util/serialize.h"
#include "df_util/util.h"

namespace fs = std::filesystem;

/*
 * Notes on LevelDB
 * -> Put overwrite if key is present
 * -> Puts in leveldb are async, but there is a way to sync if its necessary
 * */

class kv_store_leveldb: public kv_store<std::string>{
private:
    leveldb::DB* db;
    leveldb::DB* db_deleted;
    leveldb::DB* db_tmp_anti_entropy;

    std::atomic<long> record_count = 0;
    std::atomic<long> record_count_cycle = 0;
    std::atomic<long> deleted_record_count = 0;
    std::atomic<long> deleted_record_count_cycle = 0;
    inline const static long record_refresh_rate = 10;
    inline const static int anti_entropy_num_keys_percentage = 80;
    inline const static int anti_entropy_num_deleted_keys_percentage = 20;
    int anti_entropy_max_keys_to_send_percentage;


private:
    void refresh_nr_keys_count();
    void refresh_nr_deleted_keys_count();
    
    int open(leveldb::Options& options, std::string db_name, std::string& db_name_path, leveldb::DB** db);
    bool remove_from_main_db(const kv_store_key<std::string>& key);
    std::unique_ptr<std::string> get_db(const std::string& key, leveldb::DB* database);
    bool get_latest_version_db(const std::string& key, std::vector<kv_store_version>& last_versions, leveldb::DB* database);
    bool verify_if_version_exists_db(const kv_store_key<std::string>& key, leveldb::DB* database);
    void print_db(leveldb::DB* database, long id, std::string filename);
    

public:
    kv_store_leveldb(std::string(*f)(const std::string&, const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at, int anti_entropy_max_keys_to_send_percentage);
    ~kv_store_leveldb();
    
    int init(void*, long id) override;
    void close() override;
    int restart_database() override;
    std::string db_name() const override;
    void update_partition(int p, int np) override;
    bool is_key_is_for_me(const kv_store_key<std::string>& key) override;

    bool put(const kv_store_key<std::string>& key, const std::string& bytes) override;
    bool put_metadata_child(const kv_store_key<std::string>& key, const std::string& child_path, bool is_create, bool is_dir) override;
    bool put_metadata_stat(const kv_store_key<std::string>& key, const std::string& bytes) override;
    bool put_with_merge(const kv_store_key<std::string>& key, const std::string& bytes) override;
    bool remove(const kv_store_key<std::string>& key) override;

    std::unique_ptr<std::string> get(const std::string& key) override;
    std::unique_ptr<std::string> get_deleted(const std::string& key) override;
    bool get_metadata_size(const std::string& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& data_v) override;
    bool get_metadata_stat(const std::string& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& data_v) override;
    bool get_latest_data_version(const std::string& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& last_data) override;
    bool get_latest_version(const std::string& key, std::vector<kv_store_version>& last_versions) override;
    bool get_latest_deleted_version(const std::string& key, std::vector<kv_store_version>& last_versions) override;
    std::unique_ptr<std::string> get_data(const kv_store_key<std::string>& key) override;
    bool verify_if_version_exists(const kv_store_key<std::string>& key) override;
    bool verify_if_version_exists_in_deleted(const kv_store_key<std::string>& key) override;

    std::unordered_map<kv_store_key<std::string>, size_t> get_keys() override;
    std::unordered_set<kv_store_key<std::string>> get_deleted_keys() override;

    void remove_from_set_existent_deleted_keys(std::unordered_set<kv_store_key<std::string>>& deleted_keys) override;
    void remove_from_map_existent_keys(std::unordered_map<kv_store_key<std::string>, size_t>& keys) override;

    bool put_tmp_anti_entropy(const std::string& base_path, const kv_store_key<std::string>& key, const std::string& bytes) override;
    bool get_tmp_key_entry_size(const std::string& base_path, const kv_store_key<std::string>& key, std::string* value) override;
    bool put_tmp_key_entry_size(const kv_store_key<std::string>& key, size_t size) override;
    bool check_if_have_all_blks_and_put_metadata(const std::string& base_path, const kv_store_key<std::string>& key, size_t blk_num) override;
    void delete_metadata_from_tmp_anti_entropy(const std::string& base_path, const kv_store_key<std::string>& key, size_t blk_num) override;
    bool get_incomplete_blks(const kv_store_key<std::string>& key, size_t new_size, std::vector<size_t>& tmp_blks_to_request) override;

    // void send_keys_gt(std::vector<std::string> &off_keys, std::vector<std::string> &off_deleted_keys, tcp_client_server_connection::tcp_client_connection &connection,
    //                   void (*action)(tcp_client_server_connection::tcp_client_connection &, const std::string &, std::map<long, long>&, long , bool, FileType::FileType, const char*, size_t)) override;

    void print_store(long id) override;
    bool clean_db() override;
 
};

kv_store_leveldb::kv_store_leveldb(std::string (*f)(const std::string&,const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at, int anti_entropy_max_keys_to_send_percentage) {
    this->merge_function = f;
    this->seen_log_garbage_at = seen_log_garbage_at;
    this->request_log_garbage_at = request_log_garbage_at;
    this->anti_entropy_log_garbage_at = anti_entropy_log_garbage_at;
    this->anti_entropy_max_keys_to_send_percentage = anti_entropy_max_keys_to_send_percentage;

}

kv_store_leveldb::~kv_store_leveldb() {
    delete db;
    delete db_deleted;
    delete db_tmp_anti_entropy;

}

void kv_store_leveldb::close() {
    spdlog::debug("closing connection");

    delete this->db;
    delete this->db_deleted;
    delete this->db_tmp_anti_entropy;
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
    std::string db_delete_name = db_name + "_deleted";
    std::string db_tmp_anti_entropy_name = db_name + "_tmp_anti_entropy";
    

    int res = 0;

    res = open(options, "DB", db_name, &db);
    if(res == -1) return -1;
    res = open(options, "DB_DELETED", db_delete_name, &db_deleted);
    if(res == -1) return -1;
    res = open(options, "TMP_ANTI_ENTROPY", db_tmp_anti_entropy_name, &db_tmp_anti_entropy);

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

/*
    Updates peer partition view.
*/
void kv_store_leveldb::update_partition(int p, int np) {
    if(np != this->nr_slices){
        std::cout << "Update Partition: " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        this->clear_seen_log();
        this->clear_seen_deleted_log();
    }
}

/*
    Checks if a given key belongs to this peer slice.

    Return true if it does, false otherwise.
*/
bool kv_store_leveldb::is_key_is_for_me(const kv_store_key<std::string>& key){
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice)
        return true;
    return false;
}

/*
    Refreshes number of keys count.
*/
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

/*
    Refreshes number of deleted keys count.
*/
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


/*
    Put file => key-value in database.

    If key already exists:
        - update (substitute) entry with bigger version;
        - add if all entries are concurrent.

    Return true on success, false otherwise.
*/
bool kv_store_leveldb::put(const kv_store_key<std::string>& key, const std::string& bytes) {
    this->seen_it(key);
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice){

        if(key.f_type == FileType::DIRECTORY){

            return put_with_merge(key, bytes);

        }else{
            kv_store_value value;
            kv_store_value_f value_f;
            kv_store_version_data vd = {key.version, bytes};  

            std::unique_ptr<std::string> value_obj = this->get(key.key);
            if(value_obj == nullptr){
                
                value_f.add(vd);

            }else{
                
                value = deserialize_from_string<kv_store_value>(*value_obj);

                if(value.f_type == FileType::FILE){

                    value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

                    int cout_concurrent = 0;
                    
                    for(int i = 0; i < value_f.vdata.size(); i++){
                        kVersionComp vcomp = comp_version(key.version, value_f.vdata.at(i).version);
                        if(vcomp == kVersionComp::Bigger){
                            value_f.vdata.at(i) = vd;
                            break;
                        }
                        else if(vcomp == kVersionComp::Concurrent)
                            cout_concurrent++;  
                    }

                    if(cout_concurrent == value_f.vdata.size())
                        value_f.add(vd);
                }else{
                    return false;
                }
            }

            std::string value_f_serialized = serialize_to_string<kv_store_value_f>(value_f);

            value.f_type = key.f_type;
            value.serialized_v_type = value_f_serialized;

            std::string value_serialized = serialize_to_string<kv_store_value>(value);

            leveldb::Status s = db->Put(leveldb::WriteOptions(), key.key, value_serialized);

            if(s.ok()){
                this->record_count++;
                return true;
            }
        }
    }
    
    return false;
}

/*
    Put directory => key-value in database .

    If key already exists:
        - merge if concurrent;
        - replace if higher version.
        
    Return true on success, false otherwise.
*/
bool kv_store_leveldb::put_with_merge(const kv_store_key<std::string>& key, const std::string& bytes) {
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice){
        kv_store_value value;
    
        kv_store_value_d value_d;
        kv_store_version_data vd = {key.version, bytes};  

        std::unique_ptr<std::string> value_obj = this->get(key.key);
        if(value_obj == nullptr){

            value_d.vdata = vd;

        }else{

            value = deserialize_from_string<kv_store_value>(*value_obj);

            if(value.f_type == FileType::DIRECTORY){

                value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

                kVersionComp vcomp = comp_version(key.version, value_d.vdata.version);
                if(vcomp == kVersionComp::Concurrent){
                    std::string merged_data;

                    if(key.version.client_id <= value_d.vdata.version.client_id){
                        merged_data = this->merge_function(bytes, value_d.vdata.data);
                    }else{
                        merged_data = this->merge_function(value_d.vdata.data, bytes);
                    }

                    value_d.vdata.version = merge_kv(value_d.vdata.version, key.version);
                    value_d.vdata.data = merged_data;
                    
                }else if(vcomp == kVersionComp::Bigger) {
                    
                    value_d.vdata = vd;
                }else{
                    return true; //Se for lower ou igual skip
                }
            }else{
                return false;
            }
        }

        std::string value_d_serialized = serialize_to_string<kv_store_value_d>(value_d);

        value.f_type = key.f_type;
        value.serialized_v_type = value_d_serialized;

        std::string value_serialized = serialize_to_string<kv_store_value>(value);

        leveldb::Status s = db->Put(leveldb::WriteOptions(), key.key, value_serialized);

        if(s.ok()){
            this->record_count++;
            return true;
        }
    }

    return false;
}

/*
    Tries to remove key from main database and inserts key in deleted database.
    
    Return true on success, false otherwise.
*/
bool kv_store_leveldb::remove(const kv_store_key<std::string>& key) {
    this->seen_it_deleted(key);
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice){
        
        //Opcional, se performance diminuir retirar
        remove_from_main_db(key);

        std::string bytes;
        kv_store_version_data vd = {key.version, bytes};

        std::unique_ptr<std::string> value_obj = this->get_deleted(key.key);

        kv_store_value value;
        std::string value_serialized;
        
        if(key.f_type == FileType::DIRECTORY){

            kv_store_value_d value_d;
            
            if(value_obj == nullptr){
                
                value_d.vdata = vd;
            
            }else{
                
                value = deserialize_from_string<kv_store_value>(*value_obj);

                value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);
                
                kVersionComp vcomp = comp_version(key.version, value_d.vdata.version);
                if(vcomp == kVersionComp::Bigger){
                    value_d.vdata.version = key.version;

                }
                else if(vcomp == kVersionComp::Concurrent)
                    value_d.vdata.version = merge_kv(value_d.vdata.version, key.version);
            }

            value_serialized = serialize_to_string<kv_store_value_d>(value_d);
            
        }else{
            kv_store_value_f value_f;

            if(value_obj == nullptr){
            
                value_f.add(vd);

            }else{
                
                value = deserialize_from_string<kv_store_value>(*value_obj);

                value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

                int cout_concurrent = 0;
                
                for(int i = 0; i < value_f.vdata.size(); i++){
                    kVersionComp vcomp = comp_version(key.version, value_f.vdata.at(i).version);
                    if(vcomp == kVersionComp::Bigger){
                        value_f.vdata.at(i) = vd;
                        break;
                    }
                    else if(vcomp == kVersionComp::Concurrent)
                        cout_concurrent++;  
                }

                if(cout_concurrent == value_f.vdata.size())
                    value_f.add(vd);
            }

            value_serialized = serialize_to_string<kv_store_value_f>(value_f);
        }

        value.f_type = key.f_type;
        value.serialized_v_type = value_serialized;

        std::string to_insert = serialize_to_string<kv_store_value>(value);

        leveldb::Status s = db_deleted->Put(leveldb::WriteOptions(), key.key, to_insert);

        if(s.ok()){
            this->deleted_record_count++;
            return true;
        }
    }

    return false;
}


/*
    Tries to remove key from main database.
    
    Return true on success, false otherwise.
*/
bool kv_store_leveldb::remove_from_main_db(const kv_store_key<std::string>& key) {
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice){
        std::unique_ptr<std::string> data = this->get(key.key);

        bool full_remove = false;

        if(data != nullptr){
            kv_store_value value;
            std::string value_serialized;

            if(key.f_type == FileType::DIRECTORY){

                kv_store_value_d value_d;
                
                value = deserialize_from_string<kv_store_value>(*data);

                value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);
                
                kVersionComp vcomp = comp_version(key.version, value_d.vdata.version);
                if(vcomp == kVersionComp::Lower || vcomp == kVersionComp::Equal ){
                    full_remove = true;
                }else{
                    return true;
                }

            }else{

                kv_store_value_f value_f;
                    
                value = deserialize_from_string<kv_store_value>(*data);

                value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);
                
                for(auto it = value_f.vdata.begin(); it != value_f.vdata.end();){
                    kVersionComp vcomp = comp_version(key.version, it->version);
                    if(vcomp == kVersionComp::Lower || vcomp == kVersionComp::Equal){
                        it = value_f.vdata.erase(it);
                    }else{
                        ++it;
                    }
                }

                if(value_f.vdata.empty()){
                    full_remove = true;
                }else{
                    value.serialized_v_type = serialize_to_string<kv_store_value_f>(value_f);

                    std::string to_insert = serialize_to_string<kv_store_value>(value);

                    leveldb::Status s = db->Put(leveldb::WriteOptions(), key.key, to_insert);

                    if(!s.ok()){
                        return true;
                    }
                }         
            }

            if(full_remove){
                leveldb::Status s = db->Delete(leveldb::WriteOptions(), key.key);
                
                if(!s.ok())throw LevelDBException();
                
                this->record_count--; //removed record from database
                
                return true;
            }
        }
    }

    return false;
}



/*
    Retrive from database value associated to a key.

    Returns nullptr if key not found, value otherwise
*/
std::unique_ptr<std::string> kv_store_leveldb::get_db(const std::string& key, leveldb::DB* database) {
        
    std::string value;
    
    leveldb::Status s = database->Get(leveldb::ReadOptions(), key, &value);

    if (s.ok()){
        return std::make_unique<std::string>(std::move(value));
    }else{
        return nullptr;
    }
}



//  Retrive from main database value associated to a key
std::unique_ptr<std::string> kv_store_leveldb::get(const std::string& key) {
    return get_db(key, db);
}

//  Retrive from delete database value associated to a key
std::unique_ptr<std::string> kv_store_leveldb::get_deleted(const std::string& key) {
    return get_db(key, db_deleted);
}


/*
    Verify if key-version exists

    Returns false if not found or if lower version is found, true otherwise
*/
bool kv_store_leveldb::verify_if_version_exists_db(const kv_store_key<std::string>& key, leveldb::DB* database){

    std::unique_ptr<std::string> value_obj = get_db(key.key, database);
    if(value_obj == nullptr){
        return false;
    }
    else{
        kv_store_value value;
        value = deserialize_from_string<kv_store_value>(*value_obj);

        if(value.f_type == FileType::DIRECTORY){
            kv_store_value_d value_d;
            value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

            kVersionComp compv = comp_version(key.version, value_d.vdata.version);
            if(compv == kVersionComp::Equal || compv == kVersionComp::Bigger)
                return true;
            
        }else{

            kv_store_value_f value_f;
            value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

            for(int i = 0; i < value_f.vdata.size(); i++){
                kVersionComp compv = comp_version(key.version, value_f.vdata.at(i).version);
                if(compv == kVersionComp::Equal || compv == kVersionComp::Bigger)
                    return true;
            }
        }

        return false;
    }
}

/*
    Verify if key-version exists in main db

    Returns false if not found or if lower version is found, true otherwise
*/
bool kv_store_leveldb::verify_if_version_exists(const kv_store_key<std::string>& key){
    return verify_if_version_exists_db(key, db);
}

/*
    Verify if key-version exists in deleted db

    Returns false if not found or if lower version is found, true otherwise
*/
bool kv_store_leveldb::verify_if_version_exists_in_deleted(const kv_store_key<std::string>& key){
    return verify_if_version_exists_db(key, db_deleted);
}

/*
    Retrieves from main db data given a key-version to look for.

    Returns nullptr if key or key-version not found, valid pointer otherwise. 
*/
std::unique_ptr<std::string> kv_store_leveldb::get_data(const kv_store_key<std::string>& key){

    std::unique_ptr<std::string> value_obj = get(key.key);
    if(value_obj == nullptr){
        return nullptr;
    }
    else{
        kv_store_value value;
        value = deserialize_from_string<kv_store_value>(*value_obj);

        if(value.f_type == FileType::DIRECTORY){
            kv_store_value_d value_d;

            value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

            if(value_d.vdata.version == key.version){
                return std::make_unique<std::string>(value_d.vdata.data);
            }
            
        }else{
            kv_store_value_f value_f;
            
            value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

            for(auto& val: value_f.vdata){
                if(val.version == key.version){
                    return std::make_unique<std::string>(val.data);
                }
            }
        }

        return nullptr;
    }
}


/*
    Retrieves from database latests versions and respective data for a key

    Returns true if successfull => inserted elements in last_versions and last_data.
    Returns false if key does not exists.
*/
bool kv_store_leveldb::get_latest_data_version(const std::string& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& last_data){
    kv_store_value_f value;
    std::vector<kv_store_version> v_version;

    std::unique_ptr<std::string> value_obj = get(key);
    if(value_obj == nullptr){
        return false;
    }
    else{
        kv_store_value value;
        value = deserialize_from_string<kv_store_value>(*value_obj);

        if(value.f_type == FileType::DIRECTORY){

            kv_store_value_d value_d;
            value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);
            last_versions.push_back(value_d.vdata.version);
            last_data.push_back(std::make_unique<std::string>(value_d.vdata.data));
        
        }else{
            kv_store_value_f value_f;
            value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

            for(auto& val: value_f.vdata){
                last_versions.push_back(val.version);
                last_data.push_back(std::make_unique<std::string>(val.data));
            }
        }

        return true;
    }
}


/*
    Retrieves from database latests versions for a key

    Returns true if successfull => inserted elements in last_versions.
    Returns false if key does not exists.
*/
bool kv_store_leveldb::get_latest_version_db(const std::string& key, std::vector<kv_store_version>& last_versions, leveldb::DB* database){
    std::vector<kv_store_version> v_version;

    std::unique_ptr<std::string> value_obj = get_db(key, database);
    if(value_obj == nullptr){
        return false;
    }
    else{
        kv_store_value value;
        value = deserialize_from_string<kv_store_value>(*value_obj);

        if(value.f_type == FileType::DIRECTORY){
            kv_store_value_d value_d;
            value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

            last_versions.push_back(value_d.vdata.version);
            
        }else{
            kv_store_value_f value_f;
            value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

            for(auto& val: value_f.vdata){
                last_versions.push_back(val.version);
            }
        }

        return true;
    }
}

/*
    Retrieves from main db latests versions for a key

    Returns true if successfull => inserted elements in last_versions.
    Returns false if key does not exists.
*/
bool kv_store_leveldb::get_latest_version(const std::string& key, std::vector<kv_store_version>& last_versions){
    return get_latest_version_db(key, last_versions, db);
}


/*
    Retrieves from deleted db latests versions for a key

    Returns true if successfull => inserted elements in last_versions.
    Returns false if key does not exists.
*/
bool kv_store_leveldb::get_latest_deleted_version(const std::string& key, std::vector<kv_store_version>& last_versions){
    return get_latest_version_db(key, last_versions, db_deleted);
}



/*
    Tries to insert directory metadata child in database.

    Returns true if successfull => inserted element child.
    Returns false if key is not for this peer, key does not exists or failed to insert in database.
*/
bool kv_store_leveldb::put_metadata_child(const kv_store_key<std::string>& key, const std::string& child_path, bool is_create, bool is_dir){
    this->seen_it(key);
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice){

        std::unique_ptr<std::string> value_obj = get(key.key);
        if(value_obj == nullptr){
            return false;
        }
        else{
            kv_store_value value;
            value = deserialize_from_string<kv_store_value>(*value_obj);
            
            if(value.f_type == FileType::DIRECTORY){
                
                kv_store_value_d value_d;
                value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

                kVersionComp comp_v = comp_version(key.version, value_d.vdata.version);
                if(comp_v == kVersionComp::Lower || comp_v == kVersionComp::Equal)
                    return true;
                else{
                    metadata met = deserialize_from_string<metadata>(value_d.vdata.data);

                    if(is_create) met.childs.add_child(child_path, is_dir);
                    else met.childs.remove_child(child_path, is_dir);
                    
                    std::string bytes = serialize_to_string<metadata>(met);

                    value_d.vdata.data = bytes;
                    value_d.vdata.version = merge_kv(value_d.vdata.version, key.version);

                    std::string updated_value = serialize_to_string<kv_store_value_d>(value_d);

                    value.serialized_v_type = updated_value;

                    std::string to_update = serialize_to_string<kv_store_value>(value);

                    leveldb::Status s = db->Put(leveldb::WriteOptions(), key.key, to_update);

                    if(s.ok())
                        return true;
                }
            }
        }
    }

    return false;
}

/*
    Tries to insert directory metadata stat in database.

    Returns true if successfull => inserted new stat or updated existent.
    Returns false if key is not for this peer or failed to insert in database.
*/
bool kv_store_leveldb::put_metadata_stat(const kv_store_key<std::string>& key, const std::string& bytes){
    this->seen_it(key);
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice){
        kv_store_value value;        
        kv_store_value_d value_d;

        metadata_attr met_attr = deserialize_from_string<metadata_attr>(bytes);
        metadata new_met(met_attr);

        std::unique_ptr<std::string> value_obj = get(key.key);
        if(value_obj == nullptr){
    
            std::string met_data = serialize_to_string<metadata>(new_met);

            value_d.vdata = {key.version, met_data};
            std::string value_d_serial = serialize_to_string<kv_store_value_d>(value_d);

            value.f_type = FileType::DIRECTORY;
            std::string value_serial = serialize_to_string<kv_store_value>(value);
            
           leveldb::Status s = db->Put(leveldb::WriteOptions(), key.key, value_serial);
            if(s.ok()){
                this->record_count++;
                return true;
            }
        }
        else{
            value = deserialize_from_string<kv_store_value>(*value_obj);

            value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

            kVersionComp comp_v = comp_version(key.version, value_d.vdata.version);
            if(comp_v == kVersionComp::Lower || comp_v == kVersionComp::Equal){
                return true;
            }else{
                metadata met = deserialize_from_string<metadata>(value_d.vdata.data);

                std::string new_metadata = metadata::merge_attr(new_met, met);

                value_d.vdata.data = new_metadata;
                value_d.vdata.version = merge_kv(value_d.vdata.version, key.version);

                std::string serial_value_d = serialize_to_string<kv_store_value_d>(value_d);

                value.serialized_v_type = serial_value_d;

                std::string updated_value = serialize_to_string<kv_store_value>(value);

                leveldb::Status s = db->Put(leveldb::WriteOptions(), key.key, updated_value);

                if(s.ok())
                    return true;
            }
        }
    }

    return false;
}


/*
    Retrives last versions and respective size of metadata.

    Returns true if successfull.
    Returns false if key does not exists.
*/
bool kv_store_leveldb::get_metadata_size(const std::string& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& data_v){
    bool got_latest = get_latest_data_version(key, last_versions, data_v);
    if(got_latest){
        for(int i = 0; i < data_v.size(); i++){
            size_t size = data_v[i]->size();
            data_v[i] = std::make_unique<std::string>(to_string(size));
        }
    }
    return got_latest;
}

/*
    Retrives last versions and respective stat of metadata.

    Returns true if successfull.
    Returns false if key does not exists.
*/
bool kv_store_leveldb::get_metadata_stat(const std::string& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& data_v){
    bool got_latest = get_latest_data_version(key, last_versions, data_v);
    if(got_latest){
        for(int i = 0; i < data_v.size(); i++){
            metadata met = deserialize_from_string<metadata>(*(data_v[i]));
            std::string value_stat = serialize_to_string<metadata_attr>(met.attr);
            
            data_v[i] = std::make_unique<std::string>(to_string(value_stat));
        }
    }

    return got_latest;
}


/*
    Prints all databases into a file
*/
void kv_store_leveldb::print_store(long id){

    std::cout << "\nPrinting DB" << std::endl;

    print_db(db, id, "db_");
    
    std::cout << "\nPrinting DB_DELETED" << std::endl;
    
    print_db(db_deleted, id, "deleted_db_");

    std::cout << "\nPrinting DB_TMP_ANTI_ENTROPY" << std::endl;
    
    print_db(db_tmp_anti_entropy, id, "tmp_anti_entropy_");
    
    std::cout << std::endl;
}


/*
    Prints database key-value. 
*/
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
    }
    db_file.close();

    if (!it->status().ok())
    {
        std::cerr << "An error was found during the scan" << std::endl;
        std::cerr << it->status().ToString() << std::endl;
    }
}



/*
    Retrives to unordered_map structure a percentage of keys from main database.
*/
std::unordered_map<kv_store_key<std::string>, size_t> kv_store_leveldb::get_keys() {

    long cycle = record_count_cycle++;
    if(cycle % record_refresh_rate == 0){
        cycle = 0;
        this->refresh_nr_keys_count();
    }
    //Calculate percentage of keys to send
    long anti_entropy_num_keys = this->record_count * ((anti_entropy_max_keys_to_send_percentage * anti_entropy_num_keys_percentage) / 100);

    long max_random_key_start = this->record_count - anti_entropy_num_keys;

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<long> distr(0, max_random_key_start);
    long key_start = distr(eng);

    std::unordered_map<kv_store_key<std::string>, size_t> keys;
    
    //Itero pela db ate chegar a key random escolhida para começar
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next(), --key_start) {}
    
    //A partir dessa iterator(key), guardar anti_entropy_max_keys para enviar
    for(int i = 0; i < anti_entropy_num_keys && it->Valid(); it->Next()){
        std::string key = it->key().ToString();

        kv_store_value value = deserialize_from_string<kv_store_value>(it->value().ToString());

        if(value.f_type == FileType::DIRECTORY){
            
            kv_store_value_d value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

            size_t v_size = value_d.vdata.data.size();

            kv_store_key<std::string> st_key = {key, value_d.vdata.version, value.f_type, false};

            keys.insert(std::make_pair(st_key, v_size));

            i++;
        
        }else{
            kv_store_value_f value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

            for(auto& dv: value_f.vdata){
                if(i < anti_entropy_num_keys){

                    size_t v_size = dv.data.size();
                    
                    kv_store_key<std::string> st_key = {key, dv.version, value.f_type, false};

                    keys.insert(std::make_pair(st_key, v_size));

                    i++;
                }else{
                    break;
                }
            }
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
    long anti_entropy_num_deleted_keys = this->deleted_record_count * ((anti_entropy_max_keys_to_send_percentage * anti_entropy_num_deleted_keys_percentage) / 100);

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
    for(int i = 0; i < anti_entropy_num_deleted_keys && it->Valid(); it->Next()){
        std::string key = it->key().ToString();
        
        kv_store_value value = deserialize_from_string<kv_store_value>(it->value().ToString());

        if(value.f_type == FileType::DIRECTORY){
            
            kv_store_value_d value_d = deserialize_from_string<kv_store_value_d>(value.serialized_v_type);

            size_t v_size = value_d.vdata.data.size();

            kv_store_key<std::string> st_key = {key, value_d.vdata.version, value.f_type, false};

            deleted_keys.insert(st_key);

            i++;
        
        }else{
            kv_store_value_f value_f = deserialize_from_string<kv_store_value_f>(value.serialized_v_type);

            for(auto& dv: value_f.vdata){
                if(i < anti_entropy_num_deleted_keys){

                    size_t v_size = dv.data.size();
                    
                    kv_store_key<std::string> st_key = {key, dv.version, value.f_type, false};

                    deleted_keys.insert(st_key);

                    i++;
                }else{
                    break;
                }
            }
        }
    }

    if(!it->status().ok()){
        delete it;
        throw LevelDBException();
    }

    delete it;
    return std::move(deleted_keys);
}



void kv_store_leveldb::remove_from_map_existent_keys(std::unordered_map<kv_store_key<std::string>, size_t>& keys){

    for (auto it_keys = keys.begin(); it_keys != keys.end();) {
        if(this->get_slice_for_key(it_keys->first.key) == this->slice){
    
            bool exists = verify_if_version_exists(it_keys->first);
            if(exists){
                // if we hold the key
                it_keys = keys.erase(it_keys);
            }else{
                //if we do not hold the key, but can be deleted, have to check
                bool exists_in_deleted = verify_if_version_exists_in_deleted(it_keys->first);
                if (exists_in_deleted){
                    //if the key was deleted, do not need to request
                    it_keys = keys.erase(it_keys);
                }else{
                    ++it_keys;
                }
            }
        }else{
            // if the key does not belong to this slice
            it_keys = keys.erase(it_keys);
        }
    }
}


void kv_store_leveldb::remove_from_set_existent_deleted_keys(std::unordered_set<kv_store_key<std::string>>& deleted_keys){
    
    for (auto it_keys = deleted_keys.begin(); it_keys != deleted_keys.end();) {
        if(this->get_slice_for_key(it_keys->key) == this->slice){
    
            bool exists = verify_if_version_exists(*it_keys);
            if(exists){
                // if we hold the key
                it_keys = deleted_keys.erase(it_keys);
            }else{
                //if we do not hold the key, but can be deleted, have to check
                bool exists_in_deleted = verify_if_version_exists_in_deleted(*it_keys);
                if (exists_in_deleted){
                    //if the key was deleted, do not need to request
                    it_keys = deleted_keys.erase(it_keys);
                }else{
                    ++it_keys;
                }
            }
        }else{
            // if the key does not belong to this slice
            it_keys = deleted_keys.erase(it_keys);
        }
    }
}


bool kv_store_leveldb::put_tmp_anti_entropy(const std::string& base_path, const kv_store_key<std::string>& key, const std::string& bytes) {
    int k_slice = this->get_slice_for_key(base_path);

    if(this->slice == k_slice){
        leveldb::WriteOptions writeOptions;
        std::string comp_key = compose_key_toString(base_path, key.version);

        std::string blk_num_str;
        int res_2 = get_blk_num(key.key, &blk_num_str);
        if(res_2 == 0){
            std::string key_blk = comp_key + ":" + blk_num_str;
            leveldb::Status s = db_tmp_anti_entropy->Put(writeOptions, key_blk, bytes);
            if(!s.ok()) throw LevelDBException();
            return true;
        }
    }
    
    return false;
}

//key is base path
bool kv_store_leveldb::put_tmp_key_entry_size(const kv_store_key<std::string>& key, size_t size) {
    int k_slice = this->get_slice_for_key(key.key);

    if(this->slice == k_slice){
        leveldb::WriteOptions writeOptions;
        std::string comp_key;
        comp_key = compose_key_toString(key.key, key.version);
        comp_key = comp_key + "#size";
        leveldb::Status s = db_tmp_anti_entropy->Put(writeOptions, comp_key, to_string(size));
        if(!s.ok()) throw LevelDBException();
        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

bool kv_store_leveldb::get_tmp_key_entry_size(const std::string& base_path, const kv_store_key<std::string>& key, std::string* value) {
    int k_slice = this->get_slice_for_key(base_path);

    if(this->slice == k_slice){
        std::string comp_key;
        comp_key = compose_key_toString(base_path, key.version);
        comp_key = comp_key + "#size";
        leveldb::Status s = db_tmp_anti_entropy->Get(leveldb::ReadOptions(), comp_key, value);
        if(!s.ok()) throw LevelDBException();
        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

//If have all blocks and metadata was inserted with success - true
//If do not have all blocks - false
//Kv_store_key is_deleted and is_merge need to be specified
//Key is base path
//merge = true
bool kv_store_leveldb::check_if_have_all_blks_and_put_metadata(const std::string& base_path, const kv_store_key<std::string>& key, size_t NR_BLKS) {
    int k_slice = this->get_slice_for_key(base_path);

    if(this->slice == k_slice){

        std::string met;
        
        for(int i = 1; i <= NR_BLKS; i++){
            std::string comp_key = compose_key_toString(base_path, key.version);
            std::string blk_path;
            blk_path.reserve(100);
            blk_path.append(comp_key).append(":").append(std::to_string(i));

            std::string value;
            
            leveldb::Status s = db_tmp_anti_entropy->Get(leveldb::ReadOptions(), blk_path, &value);
            if(!s.ok()) return false;
            met += value;
        }

        kv_store_key<std::string> k_merge = {base_path, key.version, key.f_type, key.is_deleted};
        if(!key.is_deleted){
            put(k_merge, met);
        }else{
            remove(k_merge);
        }
        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

bool kv_store_leveldb::get_incomplete_blks(const kv_store_key<std::string>& key, size_t new_size, std::vector<size_t>& tmp_blks_to_request) {

    std::string key_s = compose_key_toString(key.key, key.version);
    std::string key_s_size = key_s + "#size";

    std::string val;
    leveldb::Status s = db_tmp_anti_entropy->Get(leveldb::ReadOptions(), key_s, &val);
    if(!s.ok()) throw LevelDBException();

    size_t size = 0;
    if(!val.empty()){
        size = stol(val);
    }else{
        return false;
    }

    size_t NR_BLKS = (size / BLK_SIZE);
    if(size % BLK_SIZE > 0) NR_BLKS = NR_BLKS + 1;

    if(size != new_size){
        delete_metadata_from_tmp_anti_entropy(key.key, key, NR_BLKS);
        return false;
    }

    for(int i = 1; i <= NR_BLKS; i++){
        std::string blk_path;
        blk_path.reserve(100);
        blk_path.append(key_s).append(":").append(std::to_string(i));

        std::string value;

        leveldb::Status s = db_tmp_anti_entropy->Get(leveldb::ReadOptions(), key_s, &value);
        if(!s.ok()) throw LevelDBException();
        else if(s.IsNotFound()){
            tmp_blks_to_request.push_back(i);
        }
    }
    return true;
}




void kv_store_leveldb::delete_metadata_from_tmp_anti_entropy(const std::string& base_path, const kv_store_key<std::string>& key, size_t NR_BLKS) {
        
    std::string comp_key;
    comp_key = compose_key_toString(base_path, key.version);
    comp_key = comp_key + "#size";
    
    leveldb::Status s = db_tmp_anti_entropy->Delete(leveldb::WriteOptions(), comp_key);
    if(!s.ok()) throw LevelDBException();

    for(int i = 1; i <= NR_BLKS; i++){
        std::string blk_path;
        blk_path.reserve(100);
        blk_path.append(comp_key).append(":").append(std::to_string(i));
      
        db_tmp_anti_entropy->Delete(leveldb::WriteOptions(), blk_path);
    }   
}


// void kv_store_leveldb::send_keys_gt(tcp_client_server_connection::tcp_client_connection &connection,
//                                     void (*send)(tcp_client_server_connection::tcp_client_connection &, const std::string &, std::map<long, long>&, long, bool, FileType::FileType, const char*, size_t)) {

//     leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
//     bool found_offset_key = false;
//     if(off_keys.empty()){
//         it->SeekToFirst();
//     }else {
//         for (auto key_it = off_keys.begin(); key_it != off_keys.end() && !found_offset_key; ++key_it) {
//             std::string prefix = *key_it;
//             it->Seek(prefix);
//             if (it->Valid() && it->key().starts_with(prefix)) {
//                 found_offset_key = true;
//                 break;
//             } else {
//                 it->SeekToFirst();
//             }
//         }
//     }

//     for (it->SeekToFirst();; it->Valid(); it->Next()) {
//         std::string comp_key = it->key().ToString();
//         std::string current_key;
//         std::string value;
//         std::map<long, long> current_vector;
//         long cli_id;
//         split_composite_total(comp_key, &current_key, &current_vector, &cli_id);
//         leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
//         bool is_merge = s.ok();
//         try {
//             send(connection, current_key, it->value().data(), it->value().size());
//         }catch(std::exception& e){
//             std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
//         }
//     }

//     if (!it->status().ok()) {
//         delete it;
//         throw LevelDBException();
//     } else {
//         delete it;
//     }


//     it = db_deleted->NewIterator(leveldb::ReadOptions());
//     found_offset_key = false;
//     if(off_deleted_keys.empty()){
//         it->SeekToFirst();
//     }else {
//         for (auto key_it = off_deleted_keys.begin(); key_it != off_deleted_keys.end() && !found_offset_key; ++key_it) {
//             std::string prefix = *key_it + "#";
//             it->Seek(prefix);
//             if (it->Valid() && it->key().starts_with(prefix)) {
//                 found_offset_key = true;
//                 break;
//             } else {
//                 it->SeekToFirst();
//             }
//         }
//     }

//     for (; it->Valid(); it->Next()) {
//         std::string comp_key = it->key().ToString();
//         std::string current_key;
//         std::string value;
//         std::map<long, long> current_vector;
//         long cli_id;
//         split_composite_total(comp_key, &current_key, &current_vector, &cli_id);
//         try {
//             send(connection, current_key, current_vector, cli_id, true, false, it->value().data(), it->value().size());
//         }catch(std::exception& e){
//             std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
//         }
//     }

//     if (!it->status().ok()) {
//         delete it;
//         throw LevelDBException();
//     } else {
//         delete it;
//     }
// }


//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------



// std::vector<std::string> kv_store_leveldb::get_last_keys_limit4(){

//     leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
//     std::set<std::string> temp;
//     for (it->SeekToLast(); it->Valid() && temp.size() < 4; it->Prev()) {
//         std::string comp_key = it->key().ToString();
//         std::string key;
//         int res = split_composite_key(comp_key, &key);
//         if(res == 0){
//             temp.emplace(key);
//         }
//     }

//     std::vector<std::string> res;
//     for (auto it = temp.begin(); it != temp.end(); ) {
//         res.push_back(std::move(temp.extract(it++).value()));
//     }
//     return res;
// }

// void kv_store_leveldb::update_partition(int p, int np) {
//     if(np != this->nr_slices){
//         std::cout << "update_partition " << std::to_string(np) << std::endl;
//         this->nr_slices = np;
//         this->slice = p;
//         //clear memory to allow new keys to be stored
//         this->clear_seen_log();
//         this->clear_seen_deleted_log();
//     }
// }

// bool kv_store_leveldb::is_key_is_for_me(const kv_store_key<std::string>& key){
//     int k_slice = this->get_slice_for_key(key.key);

//     if(this->slice == k_slice)
//         return true;
//     return false;
// }

// //Return size difference of map 
// int add_latest_version_to_map(std::unordered_map<kv_store_key<std::string>, size_t>& keys, kv_store_key<std::string> key_2_add, size_t key_2_add_size){
//     std::vector<kv_store_key<std::string>> to_remove;
//     int i = 0;
//     bool to_add = false;
//     for(auto& it: keys){
//         if(comp_version(key_2_add.version, it.first.version) == kVersionComp::Bigger){
//             to_remove.push_back(it.first);
//             to_add = true;
//         }else if(comp_version(key_2_add.version, it.first.version) == kVersionComp::Concurrent){
//             to_add = true;
//         }
//     }

//     for(auto& key: to_remove){
//         keys.erase(key);
//         i--;
//     }
//     if(to_add){
//         keys.insert(std::make_pair(key_2_add, key_2_add_size));
//         i++;
//     } 
//     return i;
// }

// std::unordered_map<kv_store_key<std::string>, size_t> kv_store_leveldb::get_keys() {

//     long cycle = record_count_cycle++;
//     if(cycle % record_refresh_rate == 0){
//         cycle = 0;
//         this->refresh_nr_keys_count();
//     }
//     //Calculate percentage of keys to send
//     long anti_entropy_num_keys = this->record_count * ((anti_entropy_max_keys_to_send_percentage * anti_entropy_num_keys_percentage) / 100);

//     long max_random_key_start = this->record_count - anti_entropy_num_keys;

//     std::random_device rd;
//     std::mt19937 eng(rd());
//     std::uniform_int_distribution<long> distr(0, max_random_key_start);
//     long key_start = distr(eng);

//     std::unordered_map<kv_store_key<std::string>, size_t> keys;
    
//     //Itero pela db ate chegar a key random escolhida para começar
//     leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
//     for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next(), --key_start) {}
    
//     //A partir dessa iterator(key), guardar anti_entropy_max_keys para enviar
//     for(int i = 0; i < anti_entropy_num_keys && it->Valid(); it->Next()){
//         std::string comp_key = it->key().ToString();
        
//         size_t v_size = it->value().size();

//         std::string key;
//         std::map<long, long> vector;
//         long cli_id;
//         int res = split_composite_total(comp_key, &key, &vector, &cli_id);
//         if(res == 0){
            
//             kv_store_key<std::string> st_key = {key, kv_store_version(vector, cli_id), false};
//             if(anti_entropy_disseminate_latest_keys){
//                 int size_dif = add_latest_version_to_map(keys, st_key, v_size);
//                 i = i + size_dif;
//                 i--; //Para contrariar o i++
//             }else{
//                 keys.insert(std::make_pair(st_key, v_size));
//             }
//         }
//         i++;
//     }

//     if(!it->status().ok()){
//         delete it;
//         throw LevelDBException();
//     }

//     delete it;
//     return std::move(keys);
// }

// //Return size difference of map 
// int add_latest_deleted_version_to_set(std::unordered_set<kv_store_key<std::string>>& deleted_keys, kv_store_key<std::string> key_2_add){
//     std::vector<kv_store_key<std::string>> to_remove;
//     int i = 0;
//     bool to_add = false;
//     for(auto& it: deleted_keys){
//         if(comp_version(key_2_add.version, it.version) == kVersionComp::Bigger){
//             to_remove.push_back(it);
//             to_add = true;
//         }else if(comp_version(key_2_add.version, it.version) == kVersionComp::Concurrent){
//             to_add = true;
//         }
//     }

//     for(auto& key: to_remove){
//         deleted_keys.erase(key);
//         i--;
//     }
//     if(to_add){
//         deleted_keys.insert(key_2_add);
//         i++;
//     }
//     return i;
// }

// std::unordered_set<kv_store_key<std::string>> kv_store_leveldb::get_deleted_keys() {

//     long cycle = deleted_record_count_cycle++;
//     if(cycle % record_refresh_rate == 0){
//         cycle = 0;
//         this->refresh_nr_deleted_keys_count();
//     }
//     //Calculate percentage of keys to send
//     long anti_entropy_num_deleted_keys = this->deleted_record_count * ((anti_entropy_max_keys_to_send_percentage * anti_entropy_num_deleted_keys_percentage) / 100);

//     long max_random_key_start = this->deleted_record_count - anti_entropy_num_deleted_keys;

//     std::random_device rd;
//     std::mt19937 eng(rd());
//     std::uniform_int_distribution<long> distr(0, max_random_key_start);
//     long key_start = distr(eng);

//     std::unordered_set<kv_store_key<std::string>> deleted_keys;
    
//     //Itero pela db ate chegar a key random escolhida para começar
//     leveldb::Iterator* it = db_deleted->NewIterator(leveldb::ReadOptions());
//     for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next(), --key_start) {}
    
//     //A partir dessa iterator(key), guardar anti_entropy_max_keys para enviar
//     for(int i = 0; i < anti_entropy_num_deleted_keys && it->Valid(); it->Next()){
//         std::string comp_key = it->key().ToString();
//         std::string key;
//         std::map<long, long> vector;
//         long cli_id;
//         int res = split_composite_total(comp_key, &key, &vector, &cli_id);
//         if(res == 0){
//             kv_store_key<std::string> st_key = {key, kv_store_version(vector, cli_id), true};
            
//             if(anti_entropy_disseminate_latest_keys){
//                 int size_dif = add_latest_deleted_version_to_set(deleted_keys, st_key);
//                 i = i + size_dif;
//                 i--; //Para contrariar o i++
//             }else{
//                 deleted_keys.insert(st_key);
//             }

//         }
//         i++;
//     }

//     if(!it->status().ok()){
//         delete it;
//         throw LevelDBException();
//     }

//     delete it;
//     return std::move(deleted_keys);
// }


// void kv_store_leveldb::remove_from_map_existent_keys(std::unordered_map<kv_store_key<std::string>, size_t>& keys){
//     leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
//     leveldb::Iterator* it_del = db_deleted->NewIterator(leveldb::ReadOptions());

//     std::string prefix;
//     for (auto it_keys = keys.begin(); it_keys != keys.end();) {
//         prefix.clear();
//         prefix = compose_key_toString(it_keys->first.key, it_keys->first.version);
//         it->Seek(prefix);
//         it_del->Seek(prefix);
//         if(it->Valid() && it->key().ToString() == prefix){
//             // if we hold the key
//             it_keys = keys.erase(it_keys);
//         }else{
//             //if we do not hold the key, but can be deleted, have to check
//             it_del->Seek(prefix);
//             if (it_del->Valid() && it_del->key().ToString() == prefix){
//                 //if the key was deleted, do not need to request
//                 it_keys = keys.erase(it_keys);
//             }else if(this->get_slice_for_key(it_keys->first.key) != this->slice){
//                 // if the key does not belong to this slice
//                 it_keys = keys.erase(it_keys);
//             }else{
//                 ++it_keys;
//             }
//         }
//     }
        

//     delete it;
//     delete it_del;
// }


// void kv_store_leveldb::remove_from_set_existent_deleted_keys(std::unordered_set<kv_store_key<std::string>>& deleted_keys){
//     leveldb::Iterator* it = db_deleted->NewIterator(leveldb::ReadOptions());

//     std::string prefix;
//     for (auto it_keys = deleted_keys.begin(); it_keys != deleted_keys.end();) {
//         prefix.clear();
//         prefix = compose_key_toString(it_keys->key, it_keys->version);
//         it->Seek(prefix);
//         if(it->Valid() && it->key().ToString() == prefix){
//             // if we hold the key
//             it_keys = deleted_keys.erase(it_keys);
//         }else if(this->get_slice_for_key(it_keys->key) != this->slice){
//             // if the key does not belong to this slice
//             it_keys = deleted_keys.erase(it_keys);
//         }else{
//             ++it_keys;
//         }
//     }

//     delete it;
// }


// void kv_store_leveldb::refresh_nr_keys_count(){
//     long count = 0;

//     leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
//     for (it->SeekToFirst(); it->Valid(); it->Next()) {
//         count++;
//     }

//     if(!it->status().ok()){
//         delete it;
//         return;
//     }

//     delete it;
//     record_count = count;
// }


// void kv_store_leveldb::refresh_nr_deleted_keys_count(){
//     long count = 0;

//     leveldb::Iterator* it = db_deleted->NewIterator(leveldb::ReadOptions());
//     for (it->SeekToFirst(); it->Valid(); it->Next()) {
//         count++;
//     }

//     if(!it->status().ok()){
//         delete it;
//         return;
//     }

//     delete it;
//     deleted_record_count = count;
// }

// std::unique_ptr<std::vector<kv_store_version>> kv_store_leveldb::get_latest_version(const std::string& key) {
//     return get_latest_version_db(key, db);
// }


// std::unique_ptr<std::vector<kv_store_version>> kv_store_leveldb::get_latest_deleted_version(const std::string& key) {
//     return get_latest_version_db(key, db_deleted);
// }

// std::unique_ptr<std::vector<kv_store_version>> kv_store_leveldb::get_latest_version_db(const std::string& key, leveldb::DB* database) {
//     leveldb::Iterator* it = database->NewIterator(leveldb::ReadOptions());
//     std::string prefix = key + "#";
//     bool exists = false;

//     std::vector<kv_store_version> current_max_versions;
//     int i = 0;
//     for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next(), i++) {
//         std::string comp_key = it->key().ToString();
//         std::string current_key;
//         std::map<long, long> current_vector;
//         long cli_id;
//         int res = split_composite_total(comp_key, &current_key, &current_vector, &cli_id);
//         if(res == 0){
//             if(i == 0){
//                 current_max_versions.emplace_back(kv_store_version(current_vector, cli_id));
//                 exists = true;
//             } else {
//                 kv_store_version temp_version = kv_store_version(current_vector, cli_id);
//                 int cout_concurrent = 0;
//                 for(int j = 0; j < current_max_versions.size(); j++){
//                     kVersionComp vcomp = comp_version(temp_version, current_max_versions.at(j));
//                     if(vcomp == kVersionComp::Bigger){
//                         current_max_versions.at(j) = temp_version;
//                         break;
//                     }
//                     else if(vcomp == kVersionComp::Concurrent)
//                         cout_concurrent++;
//                 }
//                 if(cout_concurrent == current_max_versions.size())
//                     current_max_versions.emplace_back(temp_version);
//             }
//         }
//     }
        
//     if(!it->status().ok()){
//         delete it;
//         throw LevelDBException();
//     }else if(exists){
//         delete it;
//         return std::make_unique<std::vector<kv_store_version>>(current_max_versions);
//     }
//     delete it;
//     return nullptr;
// }

// std::unique_ptr<std::vector<kv_store_version>> kv_store_leveldb::get_latest_data_version(const std::string& key, std::vector<std::unique_ptr<std::string>>& last_data){
//     leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
//     std::string prefix = key + "#";
//     bool exists = false;

//     std::vector<kv_store_version> current_max_versions;
//     int i = 0;
//     for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next(), i++) {
//         std::string comp_key = it->key().ToString();
//         std::string value = it->value().ToString();
//         std::string current_key;
//         std::map<long, long> current_vector;
//         long cli_id;
//         int res = split_composite_total(comp_key, &current_key, &current_vector, &cli_id);
//         if(res == 0){
//             if(i == 0){
//                 current_max_versions.emplace_back(kv_store_version(current_vector, cli_id));
//                 last_data.emplace_back(std::make_unique<std::string>(std::move(value)));
//                 exists = true;
//             } else {
//                 kv_store_version temp_version = kv_store_version(current_vector, cli_id);
//                 int cout_concurrent = 0;
//                 for(int j = 0; j < current_max_versions.size(); j++){
//                     kVersionComp vcomp = comp_version(temp_version, current_max_versions.at(j));
//                     if(vcomp == kVersionComp::Bigger){
//                         current_max_versions.at(j) = temp_version;
//                         last_data.at(j) = std::make_unique<std::string>(std::move(value));
//                         break;
//                     }
//                     else if(vcomp == kVersionComp::Concurrent)
//                         cout_concurrent++;
//                 }
//                 if(cout_concurrent == current_max_versions.size()){
//                     current_max_versions.push_back(temp_version);
//                     last_data.emplace_back(std::make_unique<std::string>(std::move(value)));
//                 }
//             }
//         }        
//     }
        
//     if(!it->status().ok()){
//         delete it;
//         throw LevelDBException();
//     }else if(exists){
//         delete it;
//         return std::make_unique<std::vector<kv_store_version>>(current_max_versions);
//     }
//     delete it;
//     return nullptr;
// }


// bool kv_store_leveldb::put(const kv_store_key<std::string>& key, const std::string& bytes) {
//     this->seen_it(key);
//     int k_slice = this->get_slice_for_key(key.key);

//     if(this->slice == k_slice){
//         leveldb::WriteOptions writeOptions;
//         std::string comp_key;
//         //comp_key.reserve(100);
//         comp_key = compose_key_toString(key.key, key.version);
//         db->Put(writeOptions, comp_key, bytes);
//         if(key.is_merge) {
//             std::string k_c = key.key + "#";
//             db_merge_log->Put(writeOptions, k_c, std::to_string(key.is_merge));
//         }
//         this->record_count++;
//         return true;
//     }else{
//         //Object received but does not belong to this df_store.
//         return false;
//     }
// }


// bool kv_store_leveldb::put_metadata_child(const std::string& key, const kv_store_version& version, const kv_store_version& past_version, const std::string& child_path, bool is_create, bool is_dir){
//     kv_store_key<std::string> key_comp = {key, version, false};
//     this->seen_it(key_comp);
//     int k_slice = this->get_slice_for_key(key);

//     if(this->slice == k_slice){

//         std::string value;
//         std::string past_comp_key = compose_key_toString(key, past_version);
//         std::string new_comp_key = compose_key_toString(key, version);
        
//         leveldb::Status s = db->Get(leveldb::ReadOptions(), past_comp_key, &value);

//         if (s.ok()){
//             metadata met = metadata::deserialize_from_string(value);

//             if(is_create) met.childs.add_child(child_path, is_dir);
//             else met.childs.remove_child(child_path, is_dir);

//             std::string bytes = metadata::serialize_to_string(met);
          
//             leveldb::WriteBatch batch;
//             batch.Delete(past_comp_key);
//             batch.Put(new_comp_key, bytes);
//             db->Write(leveldb::WriteOptions(), &batch);
            
//         }
//     }else{
//         //Object received but does not belong to this df_store.
//         return false;
//     }

//     return true;
// }


// bool kv_store_leveldb::put_metadata_stat(const std::string& key, const kv_store_version& version, const kv_store_version& past_version, const std::string& bytes){
//     kv_store_key<std::string> key_comp = {key, version, false};
//     this->seen_it(key_comp);
//     int k_slice = this->get_slice_for_key(key);

//     if(this->slice == k_slice){
//         std::string value;
//         std::string past_comp_key = compose_key_toString(key, past_version);
//         std::string new_comp_key = compose_key_toString(key, version);

//         metadata_attr met_attr = metadata_attr::deserialize_from_string(bytes);
//         metadata new_met(met_attr);
        
//         leveldb::Status s = db->Get(leveldb::ReadOptions(), past_comp_key, &value);

//         if (s.ok()){
//             metadata met = metadata::deserialize_from_string(value);         
            
//             std::string data = metadata::merge_attr(new_met, met);
            
//             leveldb::WriteBatch batch;
//             batch.Delete(past_comp_key);
//             batch.Put(new_comp_key, data);
//             db->Write(leveldb::WriteOptions(), &batch);
            
//         }
//         else{
//             leveldb::WriteOptions writeOptions;
//             s = db->Put(writeOptions, new_comp_key, metadata::serialize_to_string(new_met));
//             if(s.ok()) this->record_count++;
//         }
        
//     }else{
//         //Object received but does not belong to this df_store.
//         return false;
//     }

//     return true;
// }



// std::unique_ptr<std::vector<kv_store_version>> kv_store_leveldb::get_metadata_size(const std::string& key, std::vector<std::unique_ptr<std::string>>& data_v){
//     std::unique_ptr<std::vector<kv_store_version>> version = get_latest_data_version(key, data_v);

//     for(int i = 0; i < data_v.size(); i++){
//         size_t size = data_v[i]->size();
//         data_v[i] = std::make_unique<std::string>(to_string(size));
//     }

//     return std::move(version);
// }

// std::unique_ptr<std::vector<kv_store_version>> kv_store_leveldb::get_metadata_stat(const std::string& key, std::vector<std::unique_ptr<std::string>>& data_v){
//     std::unique_ptr<std::vector<kv_store_version>> version = get_latest_data_version(key, data_v);

//     for(int i = 0; i < data_v.size(); i++){
//         metadata met = metadata::deserialize_from_string(*(data_v[i]));
//         std::string value_stat = metadata_attr::serialize_to_string(met.attr);
        
//         data_v[i] = std::make_unique<std::string>(to_string(value_stat));
//     }

    
//     return std::move(version);
// }


// void kv_store_leveldb::print_store(long id){

//     std::cout << "\nPrinting DB" << std::endl;

//     print_db(db, id, "db_");
    
//     std::cout << "\nPrinting DB_DELETED" << std::endl;
    
//     print_db(db_deleted, id, "deleted_db_");

//     std::cout << "\nPrinting DB_MERGE_LOG" << std::endl;
    
//     print_db(db_merge_log, id, "merge_db_");

//     std::cout << "\nPrinting DB_TMP_ANTI_ENTROPY" << std::endl;
    
//     print_db(db_tmp_anti_entropy, id, "tmp_anti_entropy_");
    
//     std::cout << std::endl;
// }



// void kv_store_leveldb::print_db(leveldb::DB* database, long id, std::string filename){

//     leveldb::Iterator* it = database->NewIterator(leveldb::ReadOptions());
//     std::string filename2 = filename + to_string(id);
//     std::ofstream db_file(filename2);

//     db_file << "###########################################################"<< "\n";
//     db_file << "###################### LSFS DATABASE ######################"<< "\n";
//     db_file << "###########################################################"<< "\n";
    
//     for (it->SeekToFirst(); it->Valid(); it->Next())
//     {
//         std::string comp_key = it->key().ToString();
//         db_file << " Complete Key " << comp_key << "\n";
        
//         if(filename.compare("db_") == 0 || filename.compare("deleted_db_") == 0){
//             std::string key;
//             std::map<long, long> vector;
//             long cli_id;
//             int res = split_composite_total(comp_key, &key, &vector, &cli_id);

//             if(res == 0){
//                 db_file << "  Key: " << key << "\n";
//                 db_file << "  Client_id: " << cli_id << "\n";
//                 kv_store_key<std::string> get_key = {key, kv_store_version(vector, cli_id)};
//                 std::unique_ptr<std::string> data = get_db(get_key, database);

//                 if(data != nullptr) db_file << "  Size: " << data->size() << "\n";
//                 else db_file << "  Data: **NULLPTR**"<< "\n";
//             }
//         }
        
//     }
//     db_file.close();

//     if (!it->status().ok())
//     {
//         std::cerr << "An error was found during the scan" << std::endl;
//         std::cerr << it->status().ToString() << std::endl;
//     }
// }

// std::unique_ptr<std::string> kv_store_leveldb::get_db(const kv_store_key<std::string>& key, leveldb::DB* database) {
    
//     //It shoud never happen this if, only safeguard 
//     if(key.version.vv.empty())
//         std::cout << "ERROR - Empty vector " << std::endl;
    
//     std::string value;
//     std::string comp_key;
//     comp_key = compose_key_toString(key.key, key.version);
    
//     leveldb::Status s = database->Get(leveldb::ReadOptions(), comp_key, &value);

//     if (s.ok()){
//         return std::make_unique<std::string>(std::move(value));
//     }else{
//         return nullptr;
//     }
// }


// std::unique_ptr<std::string> kv_store_leveldb::get(const kv_store_key<std::string>& key) {
//     return get_db(key, db);
// }

// std::unique_ptr<std::string> kv_store_leveldb::get_deleted(const kv_store_key<std::string>& key) {
//     return get_db(key, db_deleted);
// }

// std::unique_ptr<std::string> kv_store_leveldb::get_merge(const kv_store_key<std::string>& key) {
//     std::string value;
//     std::string k_c = key.key + "#";
    
//     leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), k_c, &value);

//     if (s.ok()){
//         return std::make_unique<std::string>(std::move(value));
//     }else{
//         return nullptr;
//     }
// }

// std::unique_ptr<std::string> kv_store_leveldb::get_anti_entropy(const kv_store_key<std::string>& key, bool* is_merge) {
//     if(key.is_deleted){
//         std::cout << "################### Get Anti-entropy Message - Get Deleted" << std::endl;

//         *is_merge = false;
//         return this->get_deleted(const_cast<kv_store_key<std::string> &>(key));
//     }else{
//         std::string value;
//         std::string k_c = key.key + "#";
//         leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), k_c, &value);
//         *is_merge = s.ok();

//         return this->get(const_cast<kv_store_key<std::string> &>(key));
//     }
// }


// bool kv_store_leveldb::put_deleted(const kv_store_key<std::string>& key, const std::string& value){
    
//     int k_slice = this->get_slice_for_key(key.key);

//     if(this->slice == k_slice){
        
//         std::string comp_key;
//         comp_key = compose_key_toString(key.key, key.version);
//         leveldb::Status s = db_deleted->Put(leveldb::WriteOptions(), comp_key, value);
//         if(!s.ok()) throw LevelDBException();
//         this->deleted_record_count++;

//         return true;
//     }
//     return false;
// }



// bool kv_store_leveldb::anti_entropy_remove(const kv_store_key<std::string>& key, const std::string& value){
        
//     if(!remove(key))
//         return put_deleted(key, value);

//     return true;
// }

// //Not used
// bool kv_store_leveldb::anti_entropy_put(const kv_store_key<std::string>& key, const std::string& value){
//     this->seen_it(key);
//     int k_slice = this->get_slice_for_key(key.key);   

//     if(this->slice == k_slice){
//         std::string value;
//         std::string comp_key;
//         comp_key = compose_key_toString(key.key, key.version);
//         leveldb::Status s = db_deleted->Get(leveldb::ReadOptions(), comp_key, &value);
//         //if it is not found in the deleted records
//         if (!s.ok()){
//             db->Put(leveldb::WriteOptions(), comp_key, value);
//             if(key.is_merge) {
//                 std::string k_c = key.key + "#";
//                 db_merge_log->Put(leveldb::WriteOptions(), k_c, std::to_string(key.is_merge));
//             }
//             this->record_count++;
//             return true;
//         }
//     }
//     return false;
    
// }

// //Kv_store_key is_deleted and is_merge need to be specified
// //Key is base path
// bool kv_store_leveldb::put_tmp_anti_entropy(const std::string& base_path, const kv_store_key<std::string>& key, const std::string& bytes) {
//     this->seen_it(key);
//     int k_slice = this->get_slice_for_key(base_path);

//     if(this->slice == k_slice){
//         leveldb::WriteOptions writeOptions;
//         std::string comp_key;
//         //comp_key.reserve(100);
//         comp_key = compose_key_toString(key.key, key.version);
//         leveldb::Status s = db_tmp_anti_entropy->Put(writeOptions, comp_key, bytes);
//         if(!s.ok()) throw LevelDBException();
//         return true;
//     }else{
//         //Object received but does not belong to this df_store.
//         return false;
//     }
// }

// //key is base path
// bool kv_store_leveldb::put_tmp_key_entry_size(const kv_store_key<std::string>& key, size_t size) {
//     int k_slice = this->get_slice_for_key(key.key);

//     if(this->slice == k_slice){
//         leveldb::WriteOptions writeOptions;
//         std::string comp_key;
//         comp_key = compose_key_toString(key.key, key.version);
//         comp_key = comp_key + "#size";
//         leveldb::Status s = db_tmp_anti_entropy->Put(writeOptions, comp_key, to_string(size));
//         if(!s.ok()) throw LevelDBException();
//         return true;
//     }else{
//         //Object received but does not belong to this df_store.
//         return false;
//     }
// }

// bool kv_store_leveldb::get_tmp_key_entry_size(const std::string& base_path, const kv_store_key<std::string>& key, std::string* value) {
//     int k_slice = this->get_slice_for_key(base_path);

//     if(this->slice == k_slice){
//         std::string comp_key;
//         comp_key = compose_key_toString(base_path, key.version);
//         comp_key = comp_key + "#size";
//         leveldb::Status s = db_tmp_anti_entropy->Get(leveldb::ReadOptions(), comp_key, value);
//         if(!s.ok()) throw LevelDBException();
//         return true;
//     }else{
//         //Object received but does not belong to this df_store.
//         return false;
//     }
// }

// //If have all blocks and metadata was inserted with success - true
// //If do not have all blocks - false
// //Kv_store_key is_deleted and is_merge need to be specified
// //Key is base path
// //merge = true
// bool kv_store_leveldb::check_if_have_all_blks_and_put_metadata(const std::string& base_path, const kv_store_key<std::string>& key, size_t blk_num) {
//     int k_slice = this->get_slice_for_key(base_path);

//     if(this->slice == k_slice){

//         std::string met;
        
//         for(int i = 1; i <= blk_num; i++){
//             std::string blk_path;
//             blk_path.reserve(100);
//             blk_path.append(base_path).append(":").append(std::to_string(i));

//             std::string value;
//             std::string comp_key;
//             comp_key = compose_key_toString(blk_path, key.version);
            
//             leveldb::Status s = db_tmp_anti_entropy->Get(leveldb::ReadOptions(), comp_key, &value);
//             if(!s.ok()) return false;
//             met += value;
//         }

//         //have all blks
//         std::string c_key;
//         c_key = compose_key_toString(base_path, key.version);

//         if(!key.is_deleted){
//                 kv_store_key<std::string> k_merge = {base_path, key.version, key.is_deleted, true};
//                 put_with_merge(k_merge, met);
//         }else{
//             leveldb::Status s = db_deleted->Put(leveldb::WriteOptions(), c_key, met);
//             if(!s.ok()) throw LevelDBException();
//             this->deleted_record_count++;
//         }
//         return true;
//     }else{
//         //Object received but does not belong to this df_store.
//         return false;
//     }
// }

// bool kv_store_leveldb::get_incomplete_blks(const kv_store_key<std::string>& key, std::vector<size_t>& tmp_blks_to_request) {
//     leveldb::Iterator* it = db_tmp_anti_entropy->NewIterator(leveldb::ReadOptions());
//     std::string prefix = compose_key_toString(key.key, key.version);
//     bool res = false;
//     int i = 0;
//     size_t size = 0;
//     std::vector<size_t> received_blks;

//     for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next(), i++) {
//         std::string comp_key = it->key().ToString();
//         std::string value = it->value().ToString();

//         if(comp_key.find("#size") != std::string::npos){
//             size = stol(value);
//         }else{
//             std::string current_key;
//             std::map<long, long> current_vector;
//             long cli_id;
//             int res1 = split_composite_total(comp_key, &current_key, &current_vector, &cli_id);
//             if(res1 == 0){
//                 std::string blk_num_str;
//                 int res_2 = get_blk_num(current_key, &blk_num_str);
//                 if(res_2 == 0)
//                     received_blks.push_back(stol(blk_num_str));
//             }
//         }
//     }
//     if(size > 0) res = true;
//     for(size_t j = 1; j <= size; j++){
//         if(std::find(received_blks.begin(), received_blks.end(), j) == received_blks.end()){
//             tmp_blks_to_request.push_back(j);
//         }
//     }
        
//     if(!it->status().ok()){
//         delete it;
//         throw LevelDBException();
//     }
//     delete it;
//     return res;
// }




// void kv_store_leveldb::delete_metadata_from_tmp_anti_entropy(const std::string& base_path, const kv_store_key<std::string>& key, size_t blk_num) {
        
//     std::string comp_key;
//     comp_key = compose_key_toString(base_path, key.version);
//     comp_key = comp_key + "#size";
    
//     leveldb::Status s = db_tmp_anti_entropy->Delete(leveldb::WriteOptions(), comp_key);
//     if(!s.ok()) throw LevelDBException();

//     for(int i = 1; i <= blk_num; i++){
//         std::string blk_path;
//         blk_path.reserve(100);
//         blk_path.append(base_path).append(":").append(std::to_string(i));

//         comp_key = compose_key_toString(blk_path, key.version);
        
//         db_tmp_anti_entropy->Delete(leveldb::WriteOptions(), comp_key);
       
//     }   
// }

// //Delete must be true
// bool kv_store_leveldb::remove(const kv_store_key<std::string>& key) {
//     kv_store_key<std::string> key_comp = {key.key, key.version, true};
//     this->seen_it_deleted(key_comp);
//     int k_slice = this->get_slice_for_key(key.key);

//     if(this->slice == k_slice){
//         std::string value;
//         std::string comp_key;
//         comp_key = compose_key_toString(key.key, key.version);
        
//         leveldb::Status s = db->Get(leveldb::ReadOptions(), comp_key, &value);

//         if (s.ok()){
            
//             s = db->Delete(leveldb::WriteOptions(), comp_key);
//             if(!s.ok())throw LevelDBException();
//             this->record_count--; //removed record from database
            
//             std::string value2;
//             std::string k_c = key.key + "#";
//             s = db_merge_log->Get(leveldb::ReadOptions(), k_c, &value2);
//             if(s.ok()){
//                 s = db_merge_log->Delete(leveldb::WriteOptions(), k_c);
//                 if(!s.ok()) throw LevelDBException();
//             }

//             s = db_deleted->Put(leveldb::WriteOptions(), comp_key, value);
//             if(!s.ok()) throw LevelDBException();
//             this->deleted_record_count++;

//             return true;
//         }
//     }
//     return false;
// }


// bool kv_store_leveldb::check_if_deleted(const kv_store_key<std::string>& key){
//     leveldb::Iterator *it = db_deleted->NewIterator(leveldb::ReadOptions());
//     std::string prefix = key.key + "#";
//     bool exists = false;

//     for (it->Seek(prefix); it->Valid() && it->key().starts_with(prefix); it->Next()) {
//         std::string comp_key = compose_key_toString(key.key, key.version);
//         if(comp_key == it->key().ToString()){
//             return true;
//         }
//     }
//     return false;
// }


// bool kv_store_leveldb::check_if_put_merged(const std::string& key){
//     std::string value;
//     std::string k_c = key + "#";
//     leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), k_c, &value);
//     bool is_merge = s.ok();
//     return is_merge;
// }


// bool kv_store_leveldb::put_with_merge(const kv_store_key<std::string>& key, const std::string& bytes) {
//     try{

//         std::unique_ptr<std::vector<kv_store_version>> last_vkv = get_latest_version(key.key);
//         std::vector<std::unique_ptr<std::string>> last_vdata;
        
//         //if no key was found in the system, just insert it with merge = true
//         if( last_vkv == nullptr || last_vkv->size() <= 0){
//             return put(key, bytes);
//         }

//         for(auto &kv: *last_vkv){
//             kv_store_key<std::string> kv_m = {key.key, kv};
//             last_vdata.emplace_back(get(kv_m));
//         }

//         kv_store_version last_kv;
//         std::unique_ptr<std::string> last_data;


//         // if the vector has more than one version, just merge everything in the vector
//         // 
//         if(last_vkv->size() > 1){
//             last_kv = merge_vkv(*last_vkv);
//             last_data = std::move(last_vdata.front());
//             int i = 0;
//             for(auto const & ptr_d : last_vdata){
//                 if(i > 0){
//                     if(last_vkv->at(i).client_id == last_kv.client_id){
//                         last_data = std::make_unique<std::string>(this->merge_function(*ptr_d, *last_data));
//                     }else{
//                         last_data = std::make_unique<std::string>(this->merge_function(*last_data, *ptr_d));
//                     }
//                 }
//                 i++;
//             }
//         }
//         //retrieve first and unique elem
//         else {
//             last_kv = last_vkv->front();
//             last_data = std::move(last_vdata.front());
//         }
        
//         // They should always be concurrent
//         if(comp_version(last_kv, key.version) == kVersionComp::Concurrent){
//             this->record_count--; //put of merge_version shoudnt increment record count, its just an overwrite
//             kv_store_key<std::string> k_to_add = {key.key, merge_kv(last_kv, key.version), false, true};
//             std::string merged_data;
//             if(key.version.client_id  == k_to_add.version.client_id){
//                 merged_data = this->merge_function(bytes, *last_data);
//             }else{
//                 merged_data = this->merge_function(*last_data, bytes);
//             }
//             int res = put(k_to_add, merged_data);
//             if(res){
//                 //Ensure received version stays logged in the database. (for anti-entropy reasons)
//                 return put(key, bytes);
//             }
//             else {
//                 return false;
//             }
//         }
//         // if not just insert with merge = true
//         else {
//             return put(key, bytes);
//         }

//     }
//     catch(LevelDBException& e){
//         return false;
//     }
// }

// void kv_store_leveldb::send_keys_gt(std::vector<std::string> &off_keys, std::vector<std::string> &off_deleted_keys, tcp_client_server_connection::tcp_client_connection &connection,
//                                     void (*send)(tcp_client_server_connection::tcp_client_connection &, const std::string &, std::map<long, long>&, long, bool, bool, const char*, size_t)) {

//     leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
//     bool found_offset_key = false;
//     if(off_keys.empty()){
//         it->SeekToFirst();
//     }else {
//         for (auto key_it = off_keys.begin(); key_it != off_keys.end() && !found_offset_key; ++key_it) {
//             std::string prefix = *key_it + "#";
//             it->Seek(prefix);
//             if (it->Valid() && it->key().starts_with(prefix)) {
//                 found_offset_key = true;
//                 break;
//             } else {
//                 it->SeekToFirst();
//             }
//         }
//     }

//     for (; it->Valid(); it->Next()) {
//         std::string comp_key = it->key().ToString();
//         std::string current_key;
//         std::string value;
//         std::map<long, long> current_vector;
//         long cli_id;
//         split_composite_total(comp_key, &current_key, &current_vector, &cli_id);
//         leveldb::Status s = db_merge_log->Get(leveldb::ReadOptions(), comp_key, &value);
//         bool is_merge = s.ok();
//         try {
//             send(connection, current_key, current_vector, cli_id, false, is_merge, it->value().data(), it->value().size());
//         }catch(std::exception& e){
//             std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
//         }
//     }

//     if (!it->status().ok()) {
//         delete it;
//         throw LevelDBException();
//     } else {
//         delete it;
//     }


//     it = db_deleted->NewIterator(leveldb::ReadOptions());
//     found_offset_key = false;
//     if(off_deleted_keys.empty()){
//         it->SeekToFirst();
//     }else {
//         for (auto key_it = off_deleted_keys.begin(); key_it != off_deleted_keys.end() && !found_offset_key; ++key_it) {
//             std::string prefix = *key_it + "#";
//             it->Seek(prefix);
//             if (it->Valid() && it->key().starts_with(prefix)) {
//                 found_offset_key = true;
//                 break;
//             } else {
//                 it->SeekToFirst();
//             }
//         }
//     }

//     for (; it->Valid(); it->Next()) {
//         std::string comp_key = it->key().ToString();
//         std::string current_key;
//         std::string value;
//         std::map<long, long> current_vector;
//         long cli_id;
//         split_composite_total(comp_key, &current_key, &current_vector, &cli_id);
//         try {
//             send(connection, current_key, current_vector, cli_id, true, false, it->value().data(), it->value().size());
//         }catch(std::exception& e){
//             std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
//         }
//     }

//     if (!it->status().ok()) {
//         delete it;
//         throw LevelDBException();
//     } else {
//         delete it;
//     }
// }


/*
    Cleans all databases internally. 
    Does not delete database files.

    Return true if managed to delete all keys.
*/
bool kv_store_leveldb::clean_db(){
    bool res = true;
    
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        leveldb::Status s = db->Delete(leveldb::WriteOptions(), it->key());
        if(!s.ok())
            res = false;
    }
    it = db_deleted->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        leveldb::Status s = db_deleted->Delete(leveldb::WriteOptions(), it->key());
        if(!s.ok())
            res = false;
    }
    it = db_tmp_anti_entropy->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        leveldb::Status s = db_tmp_anti_entropy->Delete(leveldb::WriteOptions(), it->key());
        if(!s.ok())
            res = false;
    }
    return res;
}


#endif //P2PFS_KV_STORE_LEVELDB_H

