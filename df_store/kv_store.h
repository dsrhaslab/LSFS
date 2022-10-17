//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_H
#define P2PFS_KV_STORE_H

#include <unordered_set>
#include "kv_store_key.h"
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"
#include "iostream"

template <typename T>
class kv_store {

protected:

    long id;
    std::string path;
    std::atomic<int> slice = 1; //[1, nr_slices]
    std::atomic<int> nr_slices = 1;
    std::unordered_map<kv_store_key<T>, bool> seen;
    std::unordered_map<kv_store_key<T>, bool> seen_deleted;
    std::unordered_map<std::string, bool> request_log;
    std::unordered_map<std::string, bool> anti_entropy_log;
    std::recursive_mutex seen_mutex;
    std::recursive_mutex seen_deleted_mutex;
    std::recursive_mutex req_log_mutex;
    std::recursive_mutex anti_entropy_log_mutex;
    std::string(*merge_function) (const std::string& bytes, const std::string& new_bytes);
    std::atomic<long> seen_count = 0;
    std::atomic<long> seen_deleted_count = 0;
    std::atomic<long> req_count = 0;
    std::atomic<long> anti_entropy_count = 0;
    long seen_log_garbage_at;
    long request_log_garbage_at;
    long anti_entropy_log_garbage_at;

public:
    virtual int init(void*, long id) = 0;
    virtual void close() = 0;
    virtual int restart_database() = 0;
    virtual std::string db_name() const = 0;
    virtual void update_partition(int p, int np) = 0;
    virtual bool is_key_is_for_me(const kv_store_key<T>& key) = 0;
    
    // virtual void send_keys_gt(std::vector<std::string>& off_keys, std::vector<std::string> &off_deleted_keys, tcp_client_server_connection::tcp_client_connection& connection,
    //                           void(*action)(tcp_client_server_connection::tcp_client_connection& connection, const std::string&, std::map<long, long>&, long, bool, bool, const char* data, size_t data_size)) = 0;
        
    virtual bool put(const kv_store_key<T>& key, const std::string& bytes) = 0;
    virtual bool put_metadata_child(const kv_store_key<T>& key, const std::string& child_path, bool is_create, bool is_dir) = 0;
    virtual bool put_metadata_stat(const kv_store_key<T>& key, const std::string& bytes) = 0;
    virtual bool put_with_merge(const kv_store_key<T>& key, const std::string& bytes) = 0;
    virtual bool remove(const kv_store_key<T>& key) = 0;
    
    virtual std::unique_ptr<std::string> get(const T& key) = 0;
    virtual std::unique_ptr<std::string> get_deleted(const T& key) = 0;
    virtual bool get_metadata_size(const T& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& data_v) = 0;
    virtual bool get_metadata_stat(const T& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& data_v) = 0;
    virtual bool get_latest_data_version(const T& key, std::vector<kv_store_version>& last_versions, std::vector<std::unique_ptr<std::string>>& last_data) = 0;
    virtual bool get_latest_version(const T& key, std::vector<kv_store_version>& last_versions) = 0;
    virtual bool get_latest_deleted_version(const T& key, std::vector<kv_store_version>& last_versions) = 0;
    virtual std::unique_ptr<std::string> get_data(const kv_store_key<T>& key) = 0;
    virtual bool verify_if_version_exists(const kv_store_key<T>& key) = 0;
    virtual bool verify_if_version_exists_in_deleted(const kv_store_key<T>& key) = 0;

    virtual std::unordered_map<kv_store_key<T>, size_t> get_keys() = 0;
    virtual std::unordered_set<kv_store_key<T>> get_deleted_keys() = 0;

    virtual void remove_from_set_existent_deleted_keys(std::unordered_set<kv_store_key<T>>& deleted_keys) = 0;
    virtual void remove_from_map_existent_keys(std::unordered_map<kv_store_key<T>, size_t>& keys) = 0;

    virtual bool put_tmp_anti_entropy(const std::string& base_path, const kv_store_key<T>& key, const std::string& bytes) = 0;
    virtual bool get_tmp_key_entry_size(const std::string& base_path, const kv_store_key<T>& key, std::string* value) = 0;
    virtual bool put_tmp_key_entry_size(const kv_store_key<T>& key, size_t size) = 0;
    virtual bool check_if_have_all_blks_and_put_metadata(const std::string& base_path, const kv_store_key<T>& key, size_t blk_num) = 0;
    virtual void delete_metadata_from_tmp_anti_entropy(const std::string& base_path, const kv_store_key<T>& key, size_t blk_num) = 0;
    virtual bool get_incomplete_blks(const kv_store_key<T>& key, size_t new_size, std::vector<size_t>& tmp_blks_to_request) = 0;
 
    virtual void print_store(long id) = 0;
    virtual bool clean_db() = 0;


    int get_slice_for_key(const T& key);
    void clear_seen_log();
    bool have_seen(const kv_store_key<T>& key);
    void seen_it(const kv_store_key<T>& key);
    void unseen_it(const kv_store_key<T>& key);
    void clear_seen_deleted_log();
    bool have_seen_deleted(const kv_store_key<T>& key);
    void seen_it_deleted(const kv_store_key<T>& key);
    int get_slice();
    void set_slice(int slice);
    int get_nr_slices();
    void set_nr_slices(int nr_slices);
    bool in_log(const std::string& req_id);
    void log_req(const std::string& req_id);
    void clear_request_log();
    bool in_anti_entropy_log(const std::string& req_id);
    void log_anti_entropy_req(const std::string& req_id);
    void clear_anti_entropy_log();
};

template <typename T>
int kv_store<T>::get_slice_for_key(const T& key) {
    if(nr_slices == 1) return 1;

    size_t max = SIZE_MAX;
    size_t min = 0;
    size_t target = std::hash<T>()(key);
    size_t step = (max / this->nr_slices);

    size_t current = min;
    int slice = 1;
    size_t next_current = current + step;

    while (target > next_current){
        current = next_current;
        next_current = current + step;
        if(current > 0 && next_current < 0) break; //in case of overflow
        slice = slice + 1;
    }

    if(slice > this->nr_slices){
        slice = this->nr_slices - 1;
    }

    return slice;
}



template <typename T>
void kv_store<T>::clear_seen_log() {
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    this->seen.clear();
    seen_count = 0;
}

template <typename T>
bool kv_store<T>::have_seen(const kv_store_key<T>& key) {

    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    auto it = this->seen.find(key);
    if(it == this->seen.end()){ // key does not exist in seen map
        return false;
    }else{
        return it->second;
    }
}

template <typename T>
void kv_store<T>::seen_it(const kv_store_key<T>& key) {
    seen_count +=1 ;
    kv_store_key<T> key_to_insert({key.key, kv_store_version(key.version), key.f_type, key.is_deleted});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    if(seen_count % seen_log_garbage_at == 0){
        this->clear_seen_log();
    }
    this->seen.insert_or_assign(std::move(key_to_insert), true);
}

template <typename T>
void kv_store<T>::clear_seen_deleted_log() {
    std::scoped_lock<std::recursive_mutex> lk(this->seen_deleted_mutex);
    this->seen_deleted.clear();
    seen_deleted_count = 0;
}

template <typename T>
bool kv_store<T>::have_seen_deleted(const kv_store_key<T>& key) {

    std::scoped_lock<std::recursive_mutex> lk(this->seen_deleted_mutex);
    auto it = this->seen_deleted.find(key);
    if(it == this->seen_deleted.end()){ // key does not exist in seen map
        return false;
    }else{
        return it->second;
    }
}

template <typename T>
void kv_store<T>::seen_it_deleted(const kv_store_key<T>& key) {
    seen_deleted_count +=1 ;
    kv_store_key<T> key_to_insert({key.key, kv_store_version(key.version), key.f_type, key.is_deleted});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_deleted_mutex);
    if(seen_deleted_count % seen_log_garbage_at == 0){
        this->clear_seen_deleted_log();
    }
    this->seen_deleted.insert_or_assign(std::move(key_to_insert), true);
}

template <typename T>
void kv_store<T>::unseen_it(const kv_store_key<T>& key) {
    kv_store_key<T> key_to_unseen({key, kv_store_version(key.version), key.f_type, key.is_deleted});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    this->seen.insert_or_assign(std::move(key_to_unseen), false);
}

template <typename T>
int kv_store<T>::get_slice() {
    return this->slice;
}

template <typename T>
void kv_store<T>::set_slice(int slice) {
    this->slice = slice;
}

template <typename T>
int kv_store<T>::get_nr_slices() {
    return this->nr_slices;
}

template <typename T>
void kv_store<T>::set_nr_slices(int nr_slices) {
    this->nr_slices = nr_slices;
}

template <typename T>
void kv_store<T>::clear_request_log() {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    this->request_log.clear();
    req_count = 0;
}

template <typename T>
bool kv_store<T>::in_log(const std::string& req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    return !(this->request_log.find(req_id) == this->request_log.end());
}

template <typename T>
void kv_store<T>::log_req(const std::string& req_id) {
    req_count +=1 ;
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    if(req_count % request_log_garbage_at == 0){
        this->clear_request_log();
    }
    this->request_log.insert_or_assign(req_id, true);
}

template <typename T>
bool kv_store<T>::in_anti_entropy_log(const std::string& req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    return !(this->anti_entropy_log.find(req_id) == this->anti_entropy_log.end());
}

template <typename T>
void kv_store<T>::log_anti_entropy_req(const std::string& req_id) {
    anti_entropy_count += 1;
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    if(anti_entropy_count % anti_entropy_log_garbage_at == 0){
        this->clear_anti_entropy_log();
    }
    this->anti_entropy_log.insert_or_assign(req_id, true);
}

template <typename T>
void kv_store<T>::clear_anti_entropy_log() {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    this->anti_entropy_log.clear();
    anti_entropy_count = 0;
}

#endif //P2PFS_KV_STORE_H
