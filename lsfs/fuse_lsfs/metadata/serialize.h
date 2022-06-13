//
// Created by danielsf97 on 2/26/20.
//

#ifndef P2PFS_SERIALIZE_H
#define P2PFS_SERIALIZE_H

#include <boost/serialization/access.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

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

#endif //P2PFS_SERIALIZE_H