#ifndef P2PFS_METADATA_ATTR_H
#define P2PFS_METADATA_ATTR_H

#include <boost/serialization/access.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <sys/stat.h>
#include <sys/types.h>

#include "df_util/util_objects.h"
#include "lsfs/fuse_common/macros.h"   


class metadata_attr {
private:
    friend class boost::serialization::access;

public:
    struct stat stbuf;

public:
    template<class Archive> void serialize(Archive& ar, const unsigned int version);
    metadata_attr(struct stat& stbuf);
    metadata_attr() = default;
    metadata_attr(const metadata_attr& met);
    static void initialize_metadata(struct stat* stbuf, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
};

template<class Archive>
void metadata_attr::serialize(Archive &ar, const unsigned int version) {
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
}

#endif //P2PFS_METADATA_ATTR_H
