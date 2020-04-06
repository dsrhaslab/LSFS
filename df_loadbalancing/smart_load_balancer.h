//
// Created by danielsf97 on 4/5/20.
//

#ifndef P2PFS_SMART_LOAD_BALANCER_H
#define P2PFS_SMART_LOAD_BALANCER_H


#include "load_balancer.h"
#include <random>

class smart_load_balancer: public load_balancer {
private:
    std::vector<std::unique_ptr<std::vector<peer_data>>> view;
    std::recursive_mutex view_mutex;

    std::unordered_map<int, peer_data> local_view; //port -> age
    inline const static int nr_saved_peers_by_group = 2;
    double position;
    std::atomic<int> nr_groups;
    std::atomic<int> my_group;
    int replication_factor_max;
    int replication_factor_min;
    int max_age;
    std::atomic<bool> first_message; //se estamos a iniciar o protocolo de group construction

    std::mt19937 random_eng;
    std::atomic<bool> running;
    long sleep_interval;
    int sender_socket;
    std::string ip;
    int port;

private:
    peer_data get_random_peer();
    void receive_message(std::vector<peer_data> received);
    int group(double peer_pos);
    void merge_groups_from_view();
    void split_groups_from_view();

public:
    smart_load_balancer(std::string boot_ip, int boot_port, std::string ip, int port, long sleep_interval);
    peer_data get_peer(const std::string& key) override;
    void process_msg(proto::pss_message& msg) override;
    void operator()() override;

    void stop() override;

private:
    void send_msg(peer_data& target_peer, proto::pss_message& msg);
};


#endif //P2PFS_SMART_LOAD_BALANCER_H
