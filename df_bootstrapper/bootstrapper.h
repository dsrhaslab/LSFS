//
// Created by danielsf97 on 10/7/19.
//

#ifndef DATAFLASKSCPP_BOOTSTRAPPER_H
#define DATAFLASKSCPP_BOOTSTRAPPER_H

#include <string>
#include <vector>
#include <df_core/peer_data.h>
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"

class Bootstrapper {
public:
    inline const static int bootstrapper_thread_loop_size = 4;
    inline const static int boot_port = 12344;

public:
    virtual void run() = 0;
    virtual void add_peer(std::string ip, long id, double pos) = 0;
    virtual void remove_peer(std::string ip) = 0;
    virtual std::vector<peer_data> get_view() = 0;
    virtual tcp_client_server_connection::tcp_server_connection* get_connection() = 0;

};

#endif //DATAFLASKSCPP_BOOTSTRAPPER_H
