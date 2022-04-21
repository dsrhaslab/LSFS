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
    bool is_deleted = false;
    bool is_merge = false;
    
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

/*
std::ostream& operator<<(std::ostream& os, const kv_store_key<std::string>& other)
{
        return os << other.key << "#" << other.key_version;
}*/

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

            return (hash<T>()(k.key));
        }
    };
}


#endif //P2PFS_KV_STORE_KEY_H
