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
    long refresh_nr_keys_count(leveldb::DB* database);
    void refresh_nr_main_keys_count();
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

    void send_keys_gt(tcp_client_server_connection::tcp_client_connection &connection,
                      void (*action)(tcp_client_server_connection::tcp_client_connection &, const std::string &, std::map<long, long>&, long , bool, FileType::FileType, const char*, size_t)) override;

    void print_store(long id) override;
    bool clean_db() override;
    bool put_dummy(const std::string& key, const std::string& bytes) override;
 
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
    Refreshes number of keys count, given a database.
*/
long kv_store_leveldb::refresh_nr_keys_count(leveldb::DB* database){
    long count = 0;

    leveldb::Iterator* it = database->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        kv_store_value value = deserialize_from_string<kv_store_value>(it->value().ToString());
        if(value.f_type == FileType::DIRECTORY){
            count++;
        }else{
            count += value.vdata.size();
        }
    }

    if(!it->status().ok()){
        delete it;
        return -1;
    }

    delete it;
    return count;
}

/*
    Refreshes number of main db keys count.
*/
void kv_store_leveldb::refresh_nr_main_keys_count(){
    long count = refresh_nr_keys_count(db);
    if(count >= 0)
        record_count = count;
}

/*
    Refreshes number of deleted db keys count.
*/
void kv_store_leveldb::refresh_nr_deleted_keys_count(){
    long count = refresh_nr_keys_count(db_deleted);
    if(count >= 0)
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
            kv_store_version_data vd = {key.version, bytes};  

            std::unique_ptr<std::string> value_obj = this->get(key.key);
            if(value_obj == nullptr){
                
                value.add(vd);

            }else{
                
                value = deserialize_from_string<kv_store_value>(*value_obj);

                if(value.f_type == FileType::FILE){

                    int cout_concurrent = 0;
                    
                    for(int i = 0; i < value.vdata.size(); i++){
                        kVersionComp vcomp = comp_version(key.version, value.vdata.at(i).version);
                        if(vcomp == kVersionComp::Bigger){
                            value.vdata.at(i) = vd;
                            break;
                        }
                        else if(vcomp == kVersionComp::Concurrent)
                            cout_concurrent++;  
                    }

                    if(cout_concurrent == value.vdata.size())
                        value.add(vd);
                }else{
                    return false;
                }
            }

            value.f_type = key.f_type;

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

        kv_store_version_data vd = {key.version, bytes};  

        std::unique_ptr<std::string> value_obj = this->get(key.key);
        if(value_obj == nullptr){

            value.add(vd);

        }else{

            value = deserialize_from_string<kv_store_value>(*value_obj);

            if(value.f_type == FileType::DIRECTORY){

                kVersionComp vcomp = comp_version(key.version, value.vdata.at(0).version);
                if(vcomp == kVersionComp::Concurrent){
                    std::string merged_data;

                    if(key.version.client_id <= value.vdata.at(0).version.client_id){
                        merged_data = this->merge_function(bytes, value.vdata.at(0).data);
                    }else{
                        merged_data = this->merge_function(value.vdata.at(0).data, bytes);
                    }

                    value.vdata.at(0).version = merge_kv(value.vdata.at(0).version, key.version);
                    value.vdata.at(0).data = merged_data;
                    
                }else if(vcomp == kVersionComp::Bigger) {
                    
                    value.vdata.at(0) = vd;
                }else{
                    return true; //Se for lower ou igual skip
                }
            }else{
                return false;
            }
        }

        value.f_type = key.f_type;

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
            
            if(value_obj == nullptr){
                
                value.add(vd);
            
            }else{
                
                value = deserialize_from_string<kv_store_value>(*value_obj);
                
                kVersionComp vcomp = comp_version(key.version, value.vdata.at(0).version);
                if(vcomp == kVersionComp::Bigger){
                    value.vdata.at(0).version = key.version;

                }
                else if(vcomp == kVersionComp::Concurrent)
                    value.vdata.at(0).version = merge_kv(value.vdata.at(0).version, key.version);
            }
            
        }else{
            if(value_obj == nullptr){
            
                value.add(vd);

            }else{
                
                value = deserialize_from_string<kv_store_value>(*value_obj);

                int cout_concurrent = 0;
                
                for(int i = 0; i < value.vdata.size(); i++){
                    kVersionComp vcomp = comp_version(key.version, value.vdata.at(i).version);
                    if(vcomp == kVersionComp::Bigger){
                        value.vdata.at(i) = vd;
                        break;
                    }
                    else if(vcomp == kVersionComp::Concurrent)
                        cout_concurrent++;  
                }

                if(cout_concurrent == value.vdata.size())
                    value.add(vd);
            }
        }

        value.f_type = key.f_type;

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

            if(key.f_type == FileType::DIRECTORY){
                
                value = deserialize_from_string<kv_store_value>(*data);
                
                kVersionComp vcomp = comp_version(key.version, value.vdata.at(0).version);
                if(vcomp == kVersionComp::Lower || vcomp == kVersionComp::Equal ){
                    full_remove = true;
                }else{
                    return true;
                }

            }else{
                    
                value = deserialize_from_string<kv_store_value>(*data);
                
                for(auto it = value.vdata.begin(); it != value.vdata.end();){
                    kVersionComp vcomp = comp_version(key.version, it->version);
                    if(vcomp == kVersionComp::Lower || vcomp == kVersionComp::Equal){
                        it = value.vdata.erase(it);
                    }else{
                        ++it;
                    }
                }

                if(value.vdata.empty()){
                    full_remove = true;
                }else{

                    std::string to_insert = serialize_to_string<kv_store_value>(value);

                    leveldb::Status s = db->Put(leveldb::WriteOptions(), key.key, to_insert);

                    if(s.ok()){
                        return true;
                    }
                }         
            }

            if(full_remove){
                leveldb::Status s = db->Delete(leveldb::WriteOptions(), key.key);
                
                if(s.ok()){
                    this->record_count--; //removed record from database
                    return true;
                }
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

            kVersionComp compv = comp_version(key.version, value.vdata.at(0).version);
            if(compv == kVersionComp::Equal || compv == kVersionComp::Bigger)
                return true;
            
        }else{

            for(int i = 0; i < value.vdata.size(); i++){
                kVersionComp compv = comp_version(key.version, value.vdata.at(i).version);
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

            if(value.vdata.at(0).version == key.version){
                return std::make_unique<std::string>(value.vdata.at(0).data);
            }
            
        }else{

            for(auto& val: value.vdata){
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
    std::vector<kv_store_version> v_version;

    std::unique_ptr<std::string> value_obj = get(key);
    if(value_obj == nullptr){
        return false;
    }
    else{
        kv_store_value value;
        value = deserialize_from_string<kv_store_value>(*value_obj);

        if(value.f_type == FileType::DIRECTORY){

            last_versions.push_back(value.vdata.at(0).version);
            last_data.push_back(std::make_unique<std::string>(value.vdata.at(0).data));
        
        }else{

            for(auto& val: value.vdata){
                last_versions.push_back(val.version);
                last_data.push_back(std::make_unique<std::string>(val.data));
            }
        }

        return true;
    }
    return true;
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

            last_versions.push_back(value.vdata.at(0).version);
            
        }else{

            for(auto& val: value.vdata){
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
                
                kVersionComp comp_v = comp_version(key.version, value.vdata.at(0).version);
                if(comp_v == kVersionComp::Lower || comp_v == kVersionComp::Equal)
                    return true;
                else{
                    metadata met = deserialize_from_string<metadata>(value.vdata.at(0).data);

                    if(is_create) met.childs.add_child(child_path, is_dir);
                    else met.childs.remove_child(child_path, is_dir);
                    
                    std::string bytes = serialize_to_string<metadata>(met);

                    value.vdata.at(0).data = bytes;
                    value.vdata.at(0).version = merge_kv(value.vdata.at(0).version, key.version);

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

        metadata_attr met_attr = deserialize_from_string<metadata_attr>(bytes);
        metadata new_met(met_attr);

        std::unique_ptr<std::string> value_obj = get(key.key);
        if(value_obj == nullptr){
    
            std::string met_data = serialize_to_string<metadata>(new_met);
            
            kv_store_version_data vd = {key.version, met_data};
            
            value.add(vd);
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

            kVersionComp comp_v = comp_version(key.version, value.vdata.at(0).version);
            if(comp_v == kVersionComp::Lower || comp_v == kVersionComp::Equal){
                return true;
            }else{
                metadata met = deserialize_from_string<metadata>(value.vdata.at(0).data);

                std::string new_metadata = metadata::merge_attr(new_met, met);

                value.vdata.at(0).data = new_metadata;
                value.vdata.at(0).version = merge_kv(value.vdata.at(0).version, key.version);

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
        db_file << " Size: " << it->value().size() << "\n";
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
        this->refresh_nr_main_keys_count();
    }
    //Calculate percentage of keys to send
    long anti_entropy_num_keys = (this->record_count * ((anti_entropy_max_keys_to_send_percentage * anti_entropy_num_keys_percentage) / 100)) / 100;

    long max_random_key_start = this->record_count - anti_entropy_num_keys;

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<long> distr(0, max_random_key_start);
    long key_start = distr(eng);

    std::unordered_map<kv_store_key<std::string>, size_t> keys;
    
    //Itero pela db ate chegar a key random escolhida para começar
    //Can pass the exact key but doesnt matter.
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next()) {
        kv_store_value value;
        value = deserialize_from_string<kv_store_value>(it->value().ToString());
        if(value.f_type == FileType::DIRECTORY){
            --key_start;
        }else{
            key_start -= value.vdata.size();
        }
    }
    
    //A partir dessa iterator(key), guardar anti_entropy_max_keys para enviar
    for(int i = 0; i < anti_entropy_num_keys && it->Valid(); it->Next()){
        std::string key = it->key().ToString();

        kv_store_value value = deserialize_from_string<kv_store_value>(it->value().ToString());

        if(value.f_type == FileType::DIRECTORY){
            
            size_t v_size = value.vdata.at(0).data.size();

            kv_store_key<std::string> st_key = {key, value.vdata.at(0).version, value.f_type, false};

            keys.insert(std::make_pair(st_key, v_size));

            i++;
        
        }else{

            for(auto& dv: value.vdata){
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
    long anti_entropy_num_deleted_keys = (this->deleted_record_count * ((anti_entropy_max_keys_to_send_percentage * anti_entropy_num_deleted_keys_percentage) / 100)) / 100;

    long max_random_key_start = this->deleted_record_count - anti_entropy_num_deleted_keys;

    std::random_device rd;
    std::mt19937 eng(rd());
    std::uniform_int_distribution<long> distr(0, max_random_key_start);
    long key_start = distr(eng);

    std::unordered_set<kv_store_key<std::string>> deleted_keys;
    
    //Itero pela db ate chegar a key random escolhida para começar
    leveldb::Iterator* it = db_deleted->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid() && key_start > 0; it->Next(), --key_start) {
        kv_store_value value;
        value = deserialize_from_string<kv_store_value>(it->value().ToString());
        if(value.f_type == FileType::DIRECTORY){
            --key_start;
        }else{
            key_start -= value.vdata.size();
        }
    }
    
    //A partir dessa iterator(key), guardar anti_entropy_max_keys para enviar
    for(int i = 0; i < anti_entropy_num_deleted_keys && it->Valid(); it->Next()){
        std::string key = it->key().ToString();
        
        kv_store_value value = deserialize_from_string<kv_store_value>(it->value().ToString());

        if(value.f_type == FileType::DIRECTORY){
            
            kv_store_key<std::string> st_key = {key, value.vdata.at(0).version, value.f_type, false};

            deleted_keys.insert(st_key);

            i++;
        
        }else{
        
            for(auto& dv: value.vdata){
                if(i < anti_entropy_num_deleted_keys){
                    
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
            if(s.ok())
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
        if(s.ok())
            return true;
    }
    return false;
}

bool kv_store_leveldb::get_tmp_key_entry_size(const std::string& base_path, const kv_store_key<std::string>& key, std::string* value) {
    int k_slice = this->get_slice_for_key(base_path);

    if(this->slice == k_slice){
        std::string comp_key;
        comp_key = compose_key_toString(base_path, key.version);
        comp_key = comp_key + "#size";
        leveldb::Status s = db_tmp_anti_entropy->Get(leveldb::ReadOptions(), comp_key, value);
        if(s.ok())
            return true;
    }
    return false;
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
            if(s.IsNotFound()) return false;
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
    if(s.IsNotFound()) 
        return false;
    else if(!s.ok()) throw LevelDBException();

    size_t size = 0;
    if(!val.empty()){
        size = stol(val);
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
        if(s.IsNotFound()){
            tmp_blks_to_request.push_back(i);
        }else if(!s.ok()) throw LevelDBException();
    }
    return true;
}




void kv_store_leveldb::delete_metadata_from_tmp_anti_entropy(const std::string& base_path, const kv_store_key<std::string>& key, size_t NR_BLKS) {
        
    std::string comp_key = compose_key_toString(base_path, key.version);
    std::string comp_key_size = comp_key + "#size";
    
    leveldb::Status s = db_tmp_anti_entropy->Delete(leveldb::WriteOptions(), comp_key_size);
    if(!s.ok()) throw LevelDBException();

    for(int i = 1; i <= NR_BLKS; i++){
        std::string blk_path;
        blk_path.reserve(100);
        blk_path.append(comp_key).append(":").append(std::to_string(i));
      
        db_tmp_anti_entropy->Delete(leveldb::WriteOptions(), blk_path);
    }   
}


void kv_store_leveldb::send_keys_gt(tcp_client_server_connection::tcp_client_connection &connection,
                                    void (*send)(tcp_client_server_connection::tcp_client_connection &, const std::string &, std::map<long, long>&, long, bool, FileType::FileType, const char*, size_t)) {

    leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
    
    bool is_deleted = false;

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        std::string value_obj = it->value().ToString();

        kv_store_value value = deserialize_from_string<kv_store_value>(value_obj);
        if(value.f_type == FileType::DIRECTORY){
            std::map<long, long>& vv = value.vdata.at(0).version.vv;
            long client_id = value.vdata.at(0).version.client_id;
            try{
                send(connection, key, vv, client_id, is_deleted, value.f_type, value.vdata.at(0).data.c_str(), value.vdata.at(0).data.size());
            }catch(std::exception& e){
                std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
            }
        }else{
            for(auto version_data: value.vdata){
                std::map<long, long>& vv = version_data.version.vv;
                long client_id = version_data.version.client_id;
                try{
                    send(connection, key, vv, client_id, is_deleted, value.f_type, version_data.data.c_str(), version_data.data.size());
                }catch(std::exception& e){
                    std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
                }
            }
        }
    }

    if (!it->status().ok()) {
        delete it;
        throw LevelDBException();
    } else {
        delete it;
    }


    leveldb::Iterator *it_del = db_deleted->NewIterator(leveldb::ReadOptions());

    is_deleted = true;
    
    for (it_del->SeekToFirst(); it_del->Valid(); it_del->Next()) {
        std::string key = it_del->key().ToString();
        std::string value_obj = it_del->value().ToString();

        kv_store_value value = deserialize_from_string<kv_store_value>(value_obj);
        if(value.f_type == FileType::DIRECTORY){
            std::map<long, long>& vv = value.vdata.at(0).version.vv;
            long client_id = value.vdata.at(0).version.client_id;
            try{
                send(connection, key, vv, client_id, is_deleted, value.f_type, value.vdata.at(0).data.c_str(), value.vdata.at(0).data.size());
            }catch(std::exception& e){
                std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
            }
        }else{
            for(auto version_data: value.vdata){
                std::map<long, long>& vv = version_data.version.vv;
                long client_id = version_data.version.client_id;
                try{
                    send(connection, key, vv, client_id, is_deleted, value.f_type, version_data.data.c_str(), version_data.data.size());
                }catch(std::exception& e){
                    std::cerr << "Exception: " << e.what()  << " " << strerror(errno) << std::endl;
                }
            }
        }
    }

    if (!it->status().ok()) {
        delete it;
        throw LevelDBException();
    } else {
        delete it;
    }
}


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

bool kv_store_leveldb::put_dummy(const std::string& key, const std::string& bytes) {
   
    leveldb::Status s = db->Put(leveldb::WriteOptions(), key, bytes);

    if(s.ok()){
        this->record_count++;
        return true;
    }
    return false;
}



#endif //P2PFS_KV_STORE_LEVELDB_H

