//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_DYNAMIC_LOAD_BALANCER_H
#define P2PFS_DYNAMIC_LOAD_BALANCER_H


#include <vector>
#include <atomic>
#include "load_balancer.h"
#include "../df_core/peer_data.h"
#include "../pss_message.pb.h"

class dynamic_load_balancer: public load_balancer {
private:
    std::vector<peer_data> view;
    std::recursive_mutex view_mutex;
    std::atomic<bool> running;
    long sleep_interval;
    int sender_socket;
    std::string ip;
    int port;

public:
    dynamic_load_balancer(std::string boot_ip, int boot_port, std::string ip, int port, long sleep_interval);
    peer_data get_random_peer() override;
    void process_msg(proto::pss_message& msg);
    void operator()();

    void stop();

private:
    void send_msg(peer_data& target_peer, proto::pss_message& msg);
};


#endif //P2PFS_DYNAMIC_LOAD_BALANCER_H
