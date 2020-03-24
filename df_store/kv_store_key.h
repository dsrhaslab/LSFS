//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_KEY_H
#define P2PFS_KV_STORE_KEY_H

#include "kv_store_key_version.h"

template <typename T>
struct kv_store_key {
    T key;
    kv_store_key_version key_version;

    inline bool operator==(const kv_store_key& other) const
    {
        return this->key == other.key && this->key_version == other.key_version;
    }

    inline bool operator<(const kv_store_key& other) const
    {
        if(this->key == other.key){
            return (this->key_version < other.key_version);
        }else{
            return (this->key < other.key);
        }
    }

    inline bool operator>(const kv_store_key& other) const
    {
        if(this->key == other.key){
            return (this->key_version > other.key_version);
        }else{
            return (this->key > other.key);
        }
    }
};

namespace std {

    template <typename T>
    struct hash<kv_store_key<T>>
    {
        std::size_t operator()(const kv_store_key<T>& k) const
        {
            using std::size_t;
            using std::hash;
            using std::string;

            // Compute individual hash values for first,
            // second and third and combine them using XOR
            // and bit shifting:

            return ((hash<T>()(k.key)
                     ^ (hash<long>()(k.key_version.version) << 1)) >> 1);
        }
    };
}


#endif //P2PFS_KV_STORE_KEY_H
