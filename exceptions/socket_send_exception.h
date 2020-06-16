//
// Created by danielsf97 on 6/16/20.
//

#ifndef P2PFS_SOCKET_SEND_EXCEPTION_H
#define P2PFS_SOCKET_SEND_EXCEPTION_H

#include <exception>

struct SocketSendException : public std::exception
{
    const char * what () const throw ()
    {
        return "Socket Send Exception";
    }
};

#endif //P2PFS_SOCKET_SEND_EXCEPTION_H
