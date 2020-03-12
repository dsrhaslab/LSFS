//
// Created by danielsf97 on 3/10/20.
//

#ifndef P2PFS_TIMEOUT_EXCEPTION_H
#define P2PFS_TIMEOUT_EXCEPTION_H

#include <exception>

struct TimeoutException : public std::exception
{
    const char * what () const throw ()
    {
        return "Timeout Exception!!!";
    }
};

#endif //P2PFS_TIMEOUT_EXCEPTION_H
