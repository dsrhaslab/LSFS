//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_LOAD_BALANCER_H
#define P2PFS_LOAD_BALANCER_H

#include <df_core/peer_data.h>

class load_balancer{
public:
    virtual peer_data get_random_peer() = 0;
};

#endif //P2PFS_LOAD_BALANCER_H
