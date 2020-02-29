//
// Created by danielsf97 on 2/28/20.
//

#ifndef P2PFS_WIREDTIGER_ERROR_H
#define P2PFS_WIREDTIGER_ERROR_H
#include <exception>

struct WiredTigerException : public std::exception
{
    const char * what () const throw ()
    {
        return "WIREDTIGER ERROR";
    }
};

#endif //P2PFS_WIREDTIGER_ERROR_H
