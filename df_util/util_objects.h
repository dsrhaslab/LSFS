#ifndef P2PFS_UTIL_OBJECTS_H
#define P2PFS_UTIL_OBJECTS_H

#include <boost/serialization/binary_object.hpp>

enum kVersionComp 
{
    Lower, Bigger, Equal, Concurrent
};

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


namespace Status{
    enum Status {ADDED = 0, REMOVED = 1, NONE = 2};

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
        
        template<class Archive>
        void serialize(Archive & ar, Status::Status & g, const unsigned int version)
        {
            ar & Status::enum_wrapper(g);
        }

    } 
} 

#endif //P2PFS_UTIL_OBJECTS_H