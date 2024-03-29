#ifndef P2PFS_METADATA_CHILDS_H
#define P2PFS_METADATA_CHILDS_H

#include <boost/serialization/access.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <map>
#include <iostream>

#include "df_util/util_objects.h"


class metadata_childs {
private:
    friend class boost::serialization::access;

public:
    std::map<std::string, std::pair<FileType::FileType, Status::Status>> childs;

public:
    template<class Archive> void serialize(Archive& ar, const unsigned int version);
    metadata_childs() = default;
    metadata_childs(const metadata_childs& met);
    void add_child(std::string path, bool is_dir);
    void remove_child(std::string path, bool is_dir);
    void reset_add_remove_log();
    void reset_status();
    bool is_empty();
    void print_childs();
};

template<class Archive>
void metadata_childs::serialize(Archive &ar, const unsigned int version) {
    ar&this->childs;
}

#endif //P2PFS_METADATA_CHILDS_H
