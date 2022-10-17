#ifndef P2PFS_KV_STORE_VALUE_H
#define P2PFS_KV_STORE_VALUE_H

#include <boost/serialization/vector.hpp>

#include "kv_store_version_data.h"


/*
    General wrapper structure that stores key value.
    Specifies FileType to be able to deserialize string to respective structure.
*/
struct kv_store_value{
    FileType::FileType f_type;
    std::string serialized_v_type;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar&this->f_type;
        ar&this->serialized_v_type;
    }

};


/*
    Structure to store File type versions_data
*/
struct kv_store_value_f {

    std::vector<kv_store_version_data>  vdata;


    void add(kv_store_version_data& version_d){
        this->vdata.push_back(version_d);
    }

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar&this->vdata;
    }

};


/*
    Structure to store Directory type version_data
*/
struct kv_store_value_d {

    kv_store_version_data  vdata;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar&this->vdata;
    }
};



#endif //P2PFS_KV_STORE_VALUE_H