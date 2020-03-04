//
// Created by danielsf97 on 3/4/20.
//

#ifndef P2PFS_LEVELDB_ERROR_H
#define P2PFS_LEVELDB_ERROR_H

#include <exception>

struct LevelDBException : public std::exception
{
    const char * what () const throw ()
    {
        return "LEVELDB ERROR";
    }
};

#endif //P2PFS_LEVELDB_ERROR_H
