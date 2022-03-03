//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_LOAD_BALANCER_LISTENER_H
#define P2PFS_LOAD_BALANCER_LISTENER_H


#include <memory>
#include "dynamic_load_balancer.h"

class load_balancer_listener {
private:
    std::shared_ptr<load_balancer> lb;
    int socket_rcv;
    std::atomic<bool> running;
    std::string ip;
    int pss_port;

public:
    load_balancer_listener(std::shared_ptr<load_balancer> lb, std::string ip, int pss_port);
    ~load_balancer_listener();
    void stop();
    void operator()();
};


#endif //P2PFS_LOAD_BALANCER_LISTENER_H
