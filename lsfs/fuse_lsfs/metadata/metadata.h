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

#include "metadata_attr.h"
#include "metadata_childs.h"
#include "df_util/serialize.h"


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
    static std::string merge_attr(metadata met1, metadata met2);
};

template<class Archive>
void metadata::serialize(Archive &ar, const unsigned int version) {
    ar&this->attr;
    ar&this->childs;
}

#endif //P2PFS_METADATA_H
