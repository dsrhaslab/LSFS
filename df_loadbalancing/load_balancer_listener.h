//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_LOAD_BALANCER_LISTENER_H
#define P2PFS_LOAD_BALANCER_LISTENER_H


#include "dynamic_load_balancer.h"

class load_balancer_listener {
private:
    dynamic_load_balancer* dlb;
    int socket_rcv;
    std::atomic<bool> running;
    std::string ip;
    int port;

public:
    load_balancer_listener(dynamic_load_balancer* dlb, std::string ip, int port);
    ~load_balancer_listener();
    void stop();
    void operator()();
};


#endif //P2PFS_LOAD_BALANCER_LISTENER_H
