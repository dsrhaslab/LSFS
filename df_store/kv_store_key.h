//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_KEY_H
#define P2PFS_KV_STORE_KEY_H

#include "kv_store_value/kv_store_version.h"
#include "df_util/util_objects.h"


template <typename T>
struct kv_store_key {
    T key;
    kv_store_version version;
    FileType::FileType f_type = FileType::FILE;
    bool is_deleted = false;
    
    
    inline bool operator==(const kv_store_key& other) const
    {
        return this->key == other.key && this->version == other.version
                && this->is_deleted == other.is_deleted && this->f_type == other.f_type;
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

            return (hash<T>()(k.key));
        }
    };
}


#endif //P2PFS_KV_STORE_KEY_H
