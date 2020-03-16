//
// Created by danielsf97 on 2/26/20.
//

#ifndef P2PFS_METADATA_H
#define P2PFS_METADATA_H

#include <boost/serialization/access.hpp>
//#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "fuse/fuse_common/fuse35.h"

class metadata {
private:
    friend class boost::serialization::access;

public:
    struct stat stbuf;
    std::set<std::string> childs;

//    std::map<std::string, kv_store_key<std::string>> childs;

public:
    template<class Archive> void serialize(Archive& ar, const unsigned int version);
    metadata(struct stat& stbuf);
    metadata() = default;
    static std::string serialize_to_string(metadata& met);
    static metadata deserialize_from_string(std::string& string_serial);
    static void initialize_metadata(struct stat* stbuf, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    void add_child(std::string path);
//    void add_child(std::string path, kv_store_key<std::string> key);
};

template<class Archive>
void metadata::serialize(Archive &ar, const unsigned int version) {
    ar&this->stbuf.st_dev;
    ar&this->stbuf.st_ino;
    ar&this->stbuf.st_nlink;
    ar&this->stbuf.st_mode;
    ar&this->stbuf.st_uid;
    ar&this->stbuf.st_gid;
    ar&this->stbuf.__pad0;
    ar&this->stbuf.st_rdev;
    ar&this->stbuf.st_size;
    ar&this->stbuf.st_blksize;
    ar&this->stbuf.st_blocks;
    ar&this->stbuf.st_atim.tv_sec;
    ar&this->stbuf.st_atim.tv_nsec;
    ar&this->stbuf.st_mtim.tv_sec;
    ar&this->stbuf.st_mtim.tv_nsec;
    ar&this->stbuf.st_ctim.tv_sec;
    ar&this->stbuf.st_ctim.tv_nsec;
    ar&this->stbuf.__glibc_reserved[0];
    ar&this->stbuf.__glibc_reserved[1];
    ar&this->stbuf.__glibc_reserved[2];
    ar&this->childs;
}

#endif //P2PFS_METADATA_H