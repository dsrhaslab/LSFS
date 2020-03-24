//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_H
#define P2PFS_KV_STORE_H

#include <unordered_set>
#include "kv_store_key.h"

template <typename T>
class kv_store {

protected:

    long id;
    std::string path;
    std::atomic<int> slice = 1; //[1, nr_slices]
    std::atomic<int> nr_slices = 1;
    std::unordered_map<kv_store_key<T>, bool> seen;
    std::unordered_map<std::string, bool> request_log;
    std::unordered_map<std::string, bool> anti_entropy_log;
    std::recursive_mutex seen_mutex;
    std::recursive_mutex req_log_mutex;
    std::recursive_mutex anti_entropy_log_mutex;

public:

    virtual int init(void*, long id) = 0;
    virtual std::string db_name() const = 0;
    virtual void close() = 0;
    virtual void update_partition(int p, int np) = 0;
    virtual std::unordered_set<kv_store_key<T>> get_keys() = 0;
    virtual bool put(T key, long version, long client_id, std::string bytes) = 0; // use string.c_str() to convert string to const char*
    virtual std::shared_ptr<std::string> get(kv_store_key<T>& key) = 0;
    virtual std::shared_ptr<std::string> remove(kv_store_key<T> key) = 0;
    virtual std::shared_ptr<std::string> get_latest(T key, kv_store_key_version* version) = 0;
    virtual std::unique_ptr<long> get_latest_version(T key) = 0;
    virtual void print_store() = 0;


    int get_slice_for_key(T key);
    bool have_seen(T key, long version, long client_id);
    void seen_it(T key, long version, long client_id);
    int get_slice();
    void set_slice(int slice);
    int get_nr_slices();
    void set_nr_slices(int nr_slices);
    bool in_log(std::string req_id);
    void log_req(std::string req_id);
    bool in_anti_entropy_log(std::string req_id);
    void log_anti_entropy_req(std::string req_id);
};

template <typename T>
int kv_store<T>::get_slice_for_key(T key) {
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
        if(current > 0 && next_current < 0) break; //in the case of overflow
        slice = slice + 1;
    }

    if(slice > this->nr_slices){
        slice = this->nr_slices - 1;
    }

    return slice;
}

template <typename T>
bool kv_store<T>::have_seen(T key, long version, long client_id) {
    kv_store_key<T> key_to_check({key, kv_store_key_version(version, client_id)});

    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    auto it = this->seen.find(key_to_check);
    if(it == this->seen.end()){ //a chave não existe no mapa seen
        return false;
    }else{
        return it->second;
    }
}

template <typename T>
void kv_store<T>::seen_it(T key, long version, long client_id) {
    kv_store_key<T> key_to_insert({key, kv_store_key_version(version, client_id)});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    this->seen.insert_or_assign(std::move(key_to_insert), true);
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
bool kv_store<T>::in_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    return !(this->request_log.find(req_id) == this->request_log.end());
}

template <typename T>
void kv_store<T>::log_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    this->request_log.insert_or_assign(req_id, true);
}

template <typename T>
bool kv_store<T>::in_anti_entropy_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    return !(this->anti_entropy_log.find(req_id) == this->anti_entropy_log.end());
}

template <typename T>
void kv_store<T>::log_anti_entropy_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    this->anti_entropy_log.insert_or_assign(req_id, true);
}

#endif //P2PFS_KV_STORE_H
