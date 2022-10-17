#ifndef P2PFS_KV_STORE_VERSION_DATA_H
#define P2PFS_KV_STORE_VERSION_DATA_H


#include "kv_store_version.h"
#include "df_util/util_objects.h"

#include <boost/serialization/string.hpp>

struct kv_store_version_data {

    kv_store_version version;
    std::string data;

private:
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar&this->version;
        ar&this->data;
    }
    
};



#endif //P2PFS_KV_STORE_VERSION_DATA_H