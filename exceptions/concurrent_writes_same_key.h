//
// Created by danielsf97 on 2/20/20.
//

#ifndef P2PFS_CONCURRENT_WRITES_SAME_KEY_H
#define P2PFS_CONCURRENT_WRITES_SAME_KEY_H

#include <exception>

struct ConcurrentWritesSameKeyException : public std::exception
{
    const char * what () const throw ()
    {
        return "Concurrent writes over same key!!!";
    }
};

#endif //P2PFS_CONCURRENT_WRITES_SAME_KEY_H
