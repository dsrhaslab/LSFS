//
// Created by danielsf97 on 6/16/20.
//

#ifndef P2PFS_SOCKET_READ_EXCEPTION_H
#define P2PFS_SOCKET_READ_EXCEPTION_H

#include <exception>

struct SocketReadException : public std::exception
{
    const char * what () const throw ()
    {
        return "Socket Read Exception";
    }
};

#endif //P2PFS_SOCKET_READ_EXCEPTION_H
