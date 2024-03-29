//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_LOAD_BALANCER_H
#define P2PFS_LOAD_BALANCER_H

#include "df_core/peer_data.h"
#include "pss_message.pb.h"

class load_balancer{
public:
    inline const static int boot_port = 12345;
    
    virtual peer_data get_peer(const std::string& key) = 0;
    virtual peer_data get_random_peer() = 0;
    virtual std::vector<peer_data> get_n_peers(const std::string& key, int nr_peers) = 0;
    virtual std::vector<peer_data> get_n_random_peers(int nr_peers) = 0;
    virtual void stop() = 0;
    virtual void process_msg(proto::pss_message& msg) = 0;
    virtual void operator()() = 0;
};

#endif //P2PFS_LOAD_BALANCER_H
