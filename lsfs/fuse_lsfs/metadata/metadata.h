//
// Created by danielsf97 on 2/26/20.
//

#ifndef P2PFS_METADATA_H
#define P2PFS_METADATA_H

#include <boost/serialization/access.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "lsfs/fuse_common/fuse35.h"
#include "metadata_attr.h"
#include "metadata_childs.h"

class metadata {
private:
    friend class boost::serialization::access;

public:
    metadata_attr attr;
    metadata_childs childs;

public:
    template<class Archive> void serialize(Archive& ar, const unsigned int version);
    metadata(metadata_attr attr);
    metadata(const metadata& met);
    metadata() = default;
    static std::string serialize_to_string(metadata& met);
    static metadata deserialize_from_string(const std::string& string_serial);
    static std::string merge_metadata(metadata met1, metadata met2);
};

template<class Archive>
void metadata::serialize(Archive &ar, const unsigned int version) {
    ar&this->attr;
    ar&this->childs;
}

#endif //P2PFS_METADATA_H
