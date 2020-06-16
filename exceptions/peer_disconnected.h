//
// Created by danielsf97 on 6/16/20.
//

#ifndef P2PFS_PEER_DISCONNECTED_H
#define P2PFS_PEER_DISCONNECTED_H

#include <exception>

struct PeerDisconnectedException : public std::exception
{
    const char * what () const throw ()
    {
        return "Peer Disconnected";
    }
};

#endif //P2PFS_PEER_DISCONNECTED_H
