//
// Created by danielsf97 on 3/9/20.
//

#ifndef P2PFS_KEY_NOT_FOUND_H
#define P2PFS_KEY_NOT_FOUND_H

#include <exception>

struct KeyNotFoundException : public std::exception
{
    const char * what () const throw ()
    {
        return "Key Not Found Exception!!!";
    }
};


#endif //P2PFS_KEY_NOT_FOUND_H
