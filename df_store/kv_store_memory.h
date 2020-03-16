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
    std::recursive_mutex store_mutex;

public:
    int init(void*, long id) override ;
    std::string db_name() const override;
    void close() override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<T>> get_keys() override;
    bool put(T key, long version, std::string bytes) override; // use string.c_str() to convert string to const char*
    std::shared_ptr<std::string> get(kv_store_key<T> key) override;
    std::shared_ptr<std::string> remove(kv_store_key<T> key) override;
    void print_store() override;
    std::shared_ptr<std::string> get_latest(T key, long *version) override;
    std::unique_ptr<long> get_latest_version(std::string key) override;
};

template <typename T>
int kv_store_memory<T>::init(void*, long id) { return 0;}

template <typename T>
void kv_store_memory<T>::close() {}

template<typename T>
std::string kv_store_memory<T>::db_name() const {
    return "kv_store_memory_db";
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
std::shared_ptr<std::string> kv_store_memory<T>::get_latest(T key, long* version){
    std::shared_ptr<std::string> res = nullptr;
    long max_version = LONG_MIN;
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    for(const auto& store_pair: this->store){
        if(store_pair.first.key == key && store_pair.first.version >= max_version){
            max_version = store_pair.first.version;
            res = store_pair.second;
        }
    }
    *version = max_version;
    return res;
}

template <typename T>
std::unique_ptr<long> kv_store_memory<T>::get_latest_version(std::string key){
    std::unique_ptr<long> res = nullptr;
    long max_version = LONG_MIN;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    for(const auto& store_pair: this->store){
        if(store_pair.first.key == key && store_pair.first.version >= max_version){
            max_version = store_pair.first.version;
            res = std::make_unique<long>(max_version);
        }
    }

    return res;
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

#endif //P2PFS_KV_STORE_MEMORY_H
