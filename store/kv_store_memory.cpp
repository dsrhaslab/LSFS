//
// Created by danielsf97 on 1/9/20.
//

#include "kv_store_memory.h"
#include <climits>
#include <iostream>
#include <cstring>

//TODO Realizar Sincronização

//store n pode ser const char*

int kv_store_memory::get_slice_for_key(long key) {
    std::cout << "GET_SLICE_FOR_KEY" << std::endl;

    if(nr_slices == 1) return 1;

    long max = LONG_MAX;
    long min = LONG_MIN;
    long step = (max / this->nr_slices) * 2;
    long current = min;
    int slice = 1;

    long next_current = current + step;
    while (key > next_current){
        current = next_current;
        next_current = current + step;
        if(current > 0 && next_current < 0) break; //in the case of overflow
        slice = slice + 1;
    }

    if(slice > this->nr_slices){
        slice = this->nr_slices - 1;
    }

    return slice; //[1, nr_slices]
}

void kv_store_memory::update_partition(int p, int np) {
    std::cout << "UPDATE_PARTITION" << std::endl;

    if(np != this->nr_slices){
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

std::unordered_set<kv_store_key> kv_store_memory::get_keys() {
    std::cout << "GET_KEYS" << std::endl;

    std::unordered_set<kv_store_key> keys;
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    for(const auto& store_pair: this->store){
        keys.insert(store_pair.first);
    }
    return std::move(keys);
}

bool kv_store_memory::have_seen(long key, long version) {
    std::cout << "HAVE_SEEN" << std::endl;

    kv_store_key key_to_check({key, version});

    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    auto it = this->seen.find(key_to_check);
    if(it == this->seen.end()){ //a chave não existe no mapa seen
        return false;
    }else{
        return it->second;
    }
}

void kv_store_memory::seen_it(long key, long version) {
    std::cout << "SEEN_IT" << std::endl;

    kv_store_key key_to_insert({key, version});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    this->seen.insert_or_assign(std::move(key_to_insert), true);
}

bool kv_store_memory::put(long key, long version, const char *bytes) {
    std::cout << "PUT" << std::endl;

    kv_store_key key_to_insert({key, version});
    this->seen_it(key, version);
    int k_slice = this->get_slice_for_key(key);

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    if(this->slice == k_slice){
        std::cout << "ITS my SLICE -> i'm going to put " << bytes << std::endl;

        auto data_size = strlen(bytes);
        char *buffer = new char[data_size + 1];
        strncpy(buffer, bytes, data_size + 1); //buffer[len] = 0 std::make_shared<const char*>(buffer)
        this->store.insert_or_assign(std::move(key_to_insert), std::shared_ptr<const char[]>(buffer, [](const char* p){delete[] p;}));
        this->print_store();
        return true;
    }else{
        std::cout << "NOT MY SLICE" << std::endl;
        //Object received but does not belong to this store.
        return false;
    }
}

void kv_store_memory::print_store(){
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::cout << "================= MY STORE =============" << std::endl;
    for(auto& it : this->store)
        std::cout << it.first.key << ": " << it.second << std::endl;
    std::cout << "========================================" << std::endl;

}

std::shared_ptr<const char []> kv_store_memory::get(kv_store_key key) {
    std::cout << "GET" << std::endl;
    this->print_store();

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    const auto& it = this->store.find(key);
    if(it == this->store.end()){// a chave não existe
        return nullptr;
    }else{
        this->print_store();
        std::cout << "Retriving " << it->second << std::endl;
        return it->second;
    }
}

std::shared_ptr<const char []> kv_store_memory::remove(kv_store_key key) {
    std::cout << "REMOVE" << std::endl;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);

    const auto& it = this->store.find(key);
    if(it == this->store.end()){// a chave não existe
        return nullptr;
    }else{
        std::shared_ptr<const char []> res = it->second;
        this->store.erase(it);
        return res;
    }
}

int kv_store_memory::get_slice() {
    std::cout << "GET_SLICE" << std::endl;

    return this->slice;
}

void kv_store_memory::set_slice(int slice) {
    std::cout << "SET_SLICE" << std::endl;

    this->slice = slice;
}

int kv_store_memory::get_nr_slices() {
    std::cout << "GET_NR_SLICES" << std::endl;

    return this->nr_slices;
}

void kv_store_memory::set_nr_slices(int nr_slices) {
    std::cout << "SET_NR_SLICES" << std::endl;

    this->nr_slices = nr_slices;
}

bool kv_store_memory::in_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    return !(this->request_log.find(req_id) == this->request_log.end());
}

void kv_store_memory::log_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    this->request_log.insert_or_assign(req_id, true);
}

bool kv_store_memory::in_anti_entropy_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    return !(this->anti_entropy_log.find(req_id) == this->anti_entropy_log.end());
}

void kv_store_memory::log_anti_entropy_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    this->anti_entropy_log.insert_or_assign(req_id, true);
}
