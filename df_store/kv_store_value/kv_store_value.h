#ifndef P2PFS_KV_STORE_VALUE_H
#define P2PFS_KV_STORE_VALUE_H

#include <boost/serialization/vector.hpp>

#include "kv_store_version_data.h"


/*
    General wrapper structure that stores key value.
    File and Dir in same structure even though Dir doesn't need the vector, only one element.
*/
struct kv_store_value{
    FileType::FileType f_type;
    std::vector<kv_store_version_data>  vdata;


    void add(kv_store_version_data& version_d){
        this->vdata.push_back(version_d);
    }

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar&this->f_type;
        ar&this->vdata;
    }

};

#endif //P2PFS_KV_STORE_VALUE_H