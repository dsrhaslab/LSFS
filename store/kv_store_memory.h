//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_MEMORY_H
#define P2PFS_KV_STORE_MEMORY_H


#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include "kv_store_key.h"
#include "kv_store.h"

class kv_store_memory: public kv_store{
private:
    std::unordered_map<kv_store_key, const char*> store;
    std::atomic<int> slice = 0;
    std::atomic<int> nr_slices = 0;
    std::unordered_map<kv_store_key, bool> seen;
    std::recursive_mutex seen_mutex;
    std::recursive_mutex store_mutex;

public:
    int get_slice_for_key(long key) override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key> get_keys() override;
    bool have_seen(long key, long version) override;
    void seen_it(long key, long version) override;
    bool put(long key, long version, const char* bytes) override; // use string.c_str() to convert string to const char*
    const char* get(kv_store_key key) override;
    const char* remove(kv_store_key key) override;
    int get_slice() override;
    void set_slice(int slice) override;
    int get_nr_slices() override;
    void set_nr_slices(int nr_slices) override;
};


#endif //P2PFS_KV_STORE_MEMORY_H
