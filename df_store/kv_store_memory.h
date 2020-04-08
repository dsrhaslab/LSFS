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
    std::unordered_map<T, bool> store_merge_log;
    std::recursive_mutex store_mutex;

private:
    std::unique_ptr<long> get_client_id_from_key_version(std::string key, long version);

public:
    kv_store_memory(std::string (*f)(std::string&, std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at);
    int init(void*, long id) override ;
    std::string db_name() const override;
    void close() override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<T>> get_keys() override;
    bool put(std::string key, long version, long client_id, std::string bytes, bool is_merge = false) override; // use string.c_str() to convert string to const char*
    bool put_with_merge(std::string key, long version, long client_id, std::string bytes);
    std::shared_ptr<std::string> get(kv_store_key<std::string>& key) override;
    std::shared_ptr<std::string> remove(kv_store_key<T> key) override;
    void print_store() override;
    std::shared_ptr<std::string> get_latest(std::string key, kv_store_key_version* kv_version) override;
    std::shared_ptr<std::string> get_anti_entropy(kv_store_key<std::string> key, bool* is_merge) override;
    std::unique_ptr<long> get_latest_version(std::string key) override;
};

template <typename T>
kv_store_memory<T>::kv_store_memory(std::string (*f)(std::string&, std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at) {
    this->merge_function = f;
    this->seen_log_garbage_at = seen_log_garbage_at;
    this->request_log_garbage_at = request_log_garbage_at;
    this->anti_entropy_log_garbage_at = anti_entropy_log_garbage_at;
}

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
std::unique_ptr<long> kv_store_memory<T>::get_client_id_from_key_version(std::string key, long version){
    std::unique_ptr<long> res(nullptr);
    kv_store_key_version max_version = kv_store_key_version(version);
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    bool exists = false;
    for(const auto& store_pair: this->store){
        if(store_pair.first.key == key && store_pair.first.key_version.version == version && store_pair.first.key_version >= max_version){
            max_version = store_pair.first.key_version;
            exists = true;
        }
    }

    if(exists){
        res = std::make_unique<long>(max_version.client_id);
    }

    return res;
}

template <typename T>
bool kv_store_memory<T>::put(std::string key, long version, long client_id, std::string bytes, bool is_merge) {
    kv_store_key<T> key_to_insert({key, kv_store_key_version(version, client_id)});
    this->seen_it(key, version, client_id);
    int k_slice = this->get_slice_for_key(key);

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    if(this->slice == k_slice){
        auto data_size = bytes.size();
        this->store.insert_or_assign(std::move(key_to_insert), std::make_shared<std::string>(bytes));
        this->store_merge_log.emplace(key, is_merge);

        if(!is_merge){
            //if is not a merge operation should only respond that put was sucessfully if the
            //version commited is actualy the current version

            std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
            return *max_client_id == client_id;
        }

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
        std::cout << it.first.key << ": " << it.first.key_version.version << std::endl;
    std::cout << "========================================" << std::endl;

}

template <typename T>
std::shared_ptr<std::string> kv_store_memory<T>::get(kv_store_key<std::string>& key) {
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);

    if(key.key_version.client_id == -1){
        std::unique_ptr<long> current_max_client_id = get_client_id_from_key_version(key.key, key.key_version.version);
        if(current_max_client_id == nullptr){
            return nullptr;
        }
        key.key_version.client_id = *current_max_client_id;
    }

    const auto& it = this->store.find(key);
    if(it == this->store.end()){// a chave não existe
        return nullptr;
    }else{
        return it->second;
    }
}

template <typename T>
std::shared_ptr<std::string> kv_store_memory<T>::get_anti_entropy(kv_store_key<std::string> key, bool* is_merge) {

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    auto it = this->store_merge_log.find(key.key);
    if(it != this->store_merge_log.end()){
        *is_merge = it->second;
        return this->get(key);
    }

    return nullptr;
}

template <typename T>
std::shared_ptr<std::string> kv_store_memory<T>::get_latest(std::string key, kv_store_key_version* kv_version){
    std::shared_ptr<std::string> res = nullptr;
    auto max_version = kv_store_key_version(LONG_MIN);
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);

    for(const auto& store_pair: this->store){
        if(store_pair.first.key == key && store_pair.first.key_version >= max_version){
            max_version = store_pair.first.key_version;
            res = store_pair.second;
        }
    }
    *kv_version = max_version;

    return res;
}

template <typename T>
std::unique_ptr<long> kv_store_memory<T>::get_latest_version(std::string key){
    std::unique_ptr<long> res = nullptr;
    auto max_version = kv_store_key_version(LONG_MIN);

    bool exists = false;
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    for(const auto& store_pair: this->store){
        if(store_pair.first.key == key && store_pair.first.key_version >= max_version){
            max_version = store_pair.first.key_version;
            exists = true;
        }
    }

    if (exists){
        res = std::make_unique<long>(max_version.version);
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

        auto max_version = get_latest_version(key.key);
        if(max_version == nullptr){
            // se a chave que eliminei foi a última
            auto it_merge = this->store_merge_log.find(key.key);
            if(it_merge != this->store_merge_log.end()){
                this->store_merge_log.erase(it_merge);
            }
        }

        return res;
    }
}

template<typename T>
bool kv_store_memory<T>::put_with_merge(std::string key, long version, long client_id, std::string bytes) {

//    std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
//    if(max_client_id == nullptr){
//        //no conflict
//        return put(key, version, client_id, bytes);
//    }else{
//        if(*max_client_id != client_id){
//            kv_store_key<std::string> kv_key = {key, kv_store_key_version(version, *max_client_id)};
//            std::shared_ptr<std::string> data = get(kv_key);
//            if(data == nullptr){
//                // caso ocorresse algum erro
//                return false;
//            }
//            return put(key, version, std::max(*max_client_id, client_id), this->merge_function(*data, bytes));
//        }
//    }
//
//    return true;


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
    std::shared_ptr<std::string> data = get_latest(key, &kv_version);
    if(data != nullptr){
        if(kv_version.version > version){
            //if there is a version bigger than mine, use its client_id
            return put(key, version, client_id, this->merge_function(*data, bytes), true);
        }else if(kv_version.client_id != client_id){
            //if there is no version bigger than mine, use max client_id for merge
            return put(key, version, std::max(kv_version.client_id, client_id), this->merge_function(*data, bytes), true);
        }

        //No need for merge, my version-client_id is bigger than the others
        return true;
    }else{
        // error on get
        return false;
    }
}

#endif //P2PFS_KV_STORE_MEMORY_H
