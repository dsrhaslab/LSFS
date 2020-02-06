//
// Created by danielsf97 on 1/9/20.
//

#ifndef P2PFS_KV_STORE_KEY_H
#define P2PFS_KV_STORE_KEY_H

struct kv_store_key {
    long key;
    long version;

    inline bool operator==(const kv_store_key& other) const
    {
        if (this->key == other.key && this->version == other.version) return true;
        else return false;
    }

    inline bool operator<(const kv_store_key& other) const
    {
        if(this->key == other.key){
            return (this->version < other.version);
        }else{
            return (this->key < other.key);
        }
    }
};

namespace std {

    template <>
    struct hash<kv_store_key>
    {
        std::size_t operator()(const kv_store_key& k) const
        {
            using std::size_t;
            using std::hash;
            using std::string;

            // Compute individual hash values for first,
            // second and third and combine them using XOR
            // and bit shifting:

            return ((hash<long>()(k.key)
                     ^ (hash<long>()(k.version) << 1)) >> 1);
        }
    };
}


#endif //P2PFS_KV_STORE_KEY_H
