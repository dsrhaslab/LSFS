//
// Created by danielsf97 on 10/12/19.
//

#ifndef DATAFLASKSCPP_PSS_LISTENER_H
#define DATAFLASKSCPP_PSS_LISTENER_H


#include "../tcp_client_server_connection/tcp_client_server_connection.h"
#include "pss.h"
#include <memory>

class pss_listener {

private:
    tcp_client_server_connection::tcp_server_connection connection;
    std::atomic<bool> running;
    pss* cyclon_ptr;
    const char* ip;
    int port;

public:
    pss_listener(const char*  ip, int port, pss* cyclon_ptr);
    void operator ()();
    void pss_listener_worker(int *socket);
    void stop_thread();
};


#endif //DATAFLASKSCPP_PSS_LISTENER_H
