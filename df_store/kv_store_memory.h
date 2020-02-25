//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_MEMORY_H
#define P2PFS_KV_STORE_MEMORY_H

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

template <typename T>
class kv_store_memory: public kv_store<T>{
private:
    std::unordered_map<kv_store_key<T>, std::shared_ptr<std::string>> store;
    std::atomic<int> slice = 1; //[1, nr_slices]
    std::atomic<int> nr_slices = 1;
    std::unordered_map<kv_store_key<T>, bool> seen;
    std::unordered_map<std::string, bool> request_log;
    std::unordered_map<std::string, bool> anti_entropy_log;
    std::recursive_mutex seen_mutex;
    std::recursive_mutex store_mutex;
    std::recursive_mutex req_log_mutex;
    std::recursive_mutex anti_entropy_log_mutex;

public:
    int init(void*, long id) override ;
    void close() override;
    int get_slice_for_key(T key) override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<T>> get_keys() override;
    bool have_seen(T key, long version) override;
    void seen_it(T key, long version) override;
    bool put(T key, long version, std::string bytes) override; // use string.c_str() to convert string to const char*
    std::shared_ptr<std::string> get(kv_store_key<T> key) override;
    std::shared_ptr<std::string> remove(kv_store_key<T> key) override;
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

template <typename T>
int kv_store_memory<T>::init(void*, long id) { return 0;}

template <typename T>
void kv_store_memory<T>::close() {}

template <typename T>
int kv_store_memory<T>::get_slice_for_key(T key) {
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

//    long max = LONG_MAX;
//    long min = LONG_MIN;
//    long step = (max / this->nr_slices) * 2;
//    long current = min;
//    int slice = 1;
//
//    long next_current = current + step;
//    while (key > next_current){
//        current = next_current;
//        next_current = current + step;
//        if(current > 0 && next_current < 0) break; //in the case of overflow
//        slice = slice + 1;
//    }
//
//    if(slice > this->nr_slices){
//        slice = this->nr_slices - 1;
//    }
//
//    return slice; //[1, nr_slices]
}

template <typename T>
void kv_store_memory<T>::update_partition(int p, int np) {
    if(np != this->nr_slices){
        std::cout << "UPDATE_PARTITION " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->seen_mutex, this->store_mutex);
        for(const auto& seen_pair: this->seen){
            if(this->store.find(seen_pair.first) == this->store.end()) { // a chave não existe
                this->seen.insert_or_assign(seen_pair.first, false);
            }
        }
    }
}

template <typename T>
std::unordered_set<kv_store_key<T>> kv_store_memory<T>::get_keys() {
    std::unordered_set<kv_store_key<T>> keys;
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    for(const auto& store_pair: this->store){
        keys.insert(store_pair.first);
    }
    return std::move(keys);
}

template <typename T>
bool kv_store_memory<T>::have_seen(T key, long version) {
    kv_store_key<T> key_to_check({key, version});

    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    auto it = this->seen.find(key_to_check);
    if(it == this->seen.end()){ //a chave não existe no mapa seen
        return false;
    }else{
        return it->second;
    }
}

template <typename T>
void kv_store_memory<T>::seen_it(T key, long version) {
    kv_store_key<T> key_to_insert({key, version});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    this->seen.insert_or_assign(std::move(key_to_insert), true);
}

template <typename T>
bool kv_store_memory<T>::put(T key, long version, std::string bytes) {
    kv_store_key<T> key_to_insert({key, version});
    this->seen_it(key, version);
    int k_slice = this->get_slice_for_key(key);

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    if(this->slice == k_slice){
        auto data_size = bytes.size();
        this->store.insert_or_assign(std::move(key_to_insert), std::make_shared<std::string>(bytes));
        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

template <typename T>
void kv_store_memory<T>::print_store(){
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::cout << "================= MY STORE =============" << std::endl;
    for(auto& it : this->store)
        std::cout << it.first.key << ": " << it.second << std::endl;
    std::cout << "========================================" << std::endl;

}

template <typename T>
std::shared_ptr<std::string> kv_store_memory<T>::get(kv_store_key<T> key) {
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    const auto& it = this->store.find(key);
    if(it == this->store.end()){// a chave não existe
        return nullptr;
    }else{
        return it->second;
    }
}

template <typename T>
std::shared_ptr<std::string> kv_store_memory<T>::remove(kv_store_key<T> key) {
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);

    const auto& it = this->store.find(key);
    if(it == this->store.end()){// a chave não existe
        return nullptr;
    }else{
        std::shared_ptr<std::string> res = it->second;
        this->store.erase(it);
        return res;
    }
}

template <typename T>
int kv_store_memory<T>::get_slice() {
    return this->slice;
}

template <typename T>
void kv_store_memory<T>::set_slice(int slice) {
    this->slice = slice;
}

template <typename T>
int kv_store_memory<T>::get_nr_slices() {
    return this->nr_slices;
}

template <typename T>
void kv_store_memory<T>::set_nr_slices(int nr_slices) {
    this->nr_slices = nr_slices;
}

template <typename T>
bool kv_store_memory<T>::in_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    return !(this->request_log.find(req_id) == this->request_log.end());
}

template <typename T>
void kv_store_memory<T>::log_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    this->request_log.insert_or_assign(req_id, true);
}

template <typename T>
bool kv_store_memory<T>::in_anti_entropy_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    return !(this->anti_entropy_log.find(req_id) == this->anti_entropy_log.end());
}

template <typename T>
void kv_store_memory<T>::log_anti_entropy_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    this->anti_entropy_log.insert_or_assign(req_id, true);
}

#endif //P2PFS_KV_STORE_MEMORY_H
