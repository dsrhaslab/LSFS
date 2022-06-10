//
// Created by danielsf97 on 2/26/20.
//

#ifndef P2PFS_METADATA_CHILDS_H
#define P2PFS_METADATA_CHILDS_H

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

namespace FileType{
    enum FileType {FILE = 0, DIRECTORY = 1};

    // This wrapper is used to serialize enum as a char type in order to reduce space
    struct enum_wrapper {
        unsigned char m_value;
        enum_wrapper(unsigned int value) :
                m_value(value)
        {
            assert(value <= 255);
        }

        template<class Archive>
        void serialize(Archive& ar, unsigned int version)
        {
            ar & m_value;
        }
    };
}

namespace boost {
    namespace serialization {

        template<class Archive>
        void serialize(Archive & ar, FileType::FileType & g, const unsigned int version)
        {
            ar & FileType::enum_wrapper(g);
        }

    } 
} 

class metadata_childs {
private:
    friend class boost::serialization::access;

public:
    struct stat stbuf;
    std::set<std::string> childs;
    std::set<std::pair<FileType::FileType , std::string>> added_childs;
    std::set<std::pair<FileType::FileType , std::string>> removed_childs;

public:
    template<class Archive> void serialize(Archive& ar, const unsigned int version);
    metadata_childs(struct stat& stbuf);
    metadata_childs() = default;
    static std::string serialize_to_string(metadata_childs& met);
    static metadata_childs deserialize_from_string(const std::string& string_serial);
    static void initialize_metadata(struct stat* stbuf, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid);
    void add_child(std::string path, bool is_dir);
    void remove_child(std::string path, bool is_dir);
    void reset_add_remove_log();
    bool is_empty();
    static void print_metadata(metadata_childs& met);
};

template<class Archive>
void metadata_childs::serialize(Archive &ar, const unsigned int version) {
    ar&this->childs;
    ar&this->added_childs;
    ar&this->removed_childs;
}

#endif //P2PFS_METADATA_CHILDS_H
