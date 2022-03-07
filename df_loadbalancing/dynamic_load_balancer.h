//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_DYNAMIC_LOAD_BALANCER_H
#define P2PFS_DYNAMIC_LOAD_BALANCER_H


#include <vector>
#include <atomic>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>

#include "pss_message.pb.h"
#include <spdlog/spdlog.h>

#include "load_balancer.h"
#include "df_core/peer_data.h"
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"


class dynamic_load_balancer: public load_balancer {

private:
    std::vector<peer_data> view;
    std::recursive_mutex view_mutex;
    std::atomic<bool> running;
    long sleep_interval;
    int sender_socket;
    std::string ip;
    int pss_port;
    double pos;
    std::mt19937 random_eng;

public:
    dynamic_load_balancer(std::string boot_ip, std::string ip, int pss_port, long sleep_interva);
    peer_data get_peer(const std::string& key) override;
    peer_data get_random_peer() override;
    std::vector<peer_data> get_n_peers(const std::string& key, int nr_peers) override;
    std::vector<peer_data> get_n_random_peers(int nr_peers) override;
    void process_msg(proto::pss_message& msg) override;
    void operator()() override;

    void stop() override;

private:
    void send_msg(peer_data& target_peer, proto::pss_message& msg);
};


#endif //P2PFS_DYNAMIC_LOAD_BALANCER_H
