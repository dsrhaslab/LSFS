//
// Created by danielsf97 on 3/22/20.
//

#ifndef P2PFS_KV_STORE_KEY_VERSION_H
#define P2PFS_KV_STORE_KEY_VERSION_H

struct kv_store_key_version {

    long version;
    long client_id = -1;

    explicit kv_store_key_version(long version, long client_id = -1): version(version), client_id(client_id) {}

    kv_store_key_version():version(-1), client_id(-1){}

    kv_store_key_version(const kv_store_key_version& other){
        this->version = other.version;
        this->client_id = other.client_id;
    }

    inline bool operator==(const kv_store_key_version& other) const
    {
        return this->version == other.version && this->client_id == other.client_id;
    }

    inline bool operator<(const kv_store_key_version& other) const
    {
        if(this->version == other.version){
            return this->client_id < other.client_id;
        }else{
            return (this->version < other.version);
        }
    }

    inline bool operator>(const kv_store_key_version& other) const
    {
        if(this->version == other.version){
            return this->client_id > other.client_id;
        }else{
            return (this->version > other.version);
        }
    }

    inline bool operator>=(const kv_store_key_version& other) const
    {
        return *this == other || *this > other;
    }

    inline bool operator<=(const kv_store_key_version& other) const
    {
        return *this == other || *this < other;
    }
};


#endif //P2PFS_KV_STORE_KEY_VERSION_H
