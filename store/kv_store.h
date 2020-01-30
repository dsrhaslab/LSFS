//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_H
#define P2PFS_KV_STORE_H

#include <unordered_set>
#include "kv_store_key.h"

class kv_store {

public:

    virtual int get_slice_for_key(long key) = 0;
    virtual void update_partition(int p, int np) = 0;
    virtual std::unordered_set<kv_store_key> get_keys() = 0;
    virtual bool have_seen(long key, long version) = 0;
    virtual void seen_it(long key, long version) = 0;
    virtual bool put(long key, long version, const char* bytes) = 0; // use string.c_str() to convert string to const char*
    virtual std::shared_ptr<const char []> get(kv_store_key key) = 0;
    virtual std::shared_ptr<const char []> remove(kv_store_key key) = 0;
    virtual int get_slice() = 0;
    virtual void set_slice(int slice) = 0;
    virtual int get_nr_slices() = 0;
    virtual void set_nr_slices(int nr_slices) = 0;
    virtual bool in_log(std::string req_id) = 0;
    virtual void log_req(std::string req_id) = 0;
    virtual void print_store() = 0;
};

#endif //P2PFS_KV_STORE_H
