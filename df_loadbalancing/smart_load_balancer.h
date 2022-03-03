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
    std::unordered_map<std::string, peer_data> local_view;
    std::recursive_mutex local_view_mutex;
    int nr_saved_peers_by_group;
    bool local;
    int local_interval;
    long cycle;
    double position;
    std::atomic<int> nr_groups;
    std::atomic<int> my_group;
    int replication_factor_max;
    int replication_factor_min;
    int max_age;
    int max_smart_view_age;
    std::atomic<bool> recovering_local_view;
    std::mt19937 random_eng;
    std::atomic<bool> running;
    long sleep_interval;
    int sender_socket;
    std::string ip;
    int pss_port;

private:
    void print_view();
    peer_data get_random_local_peer();
    void receive_local_message(std::vector<peer_data> received);
    void receive_message(std::vector<peer_data> received);
    int group(double peer_pos);
    int group(double peer_pos, int nr_groups);
    void merge_groups_from_view();
    void split_groups_from_view();
    void incorporate_local_peers(const std::vector<peer_data>& received);
    void clean_local_view();
    void incorporate_peers_in_view(const std::vector<peer_data>& received);
    void reconfigure_view_if_needed();
    void recover_local_view(const std::vector<peer_data>& received);
    bool has_recovered();
    void request_local_message(const std::vector<peer_data>& received);
    void send_msg(const peer_data& target_peer, proto::pss_message& msg);

public:
    smart_load_balancer(std::string boot_ip, std::string ip, int pss_port, long sleep_interval, std::string& config_filename);
    peer_data get_peer(const std::string& key) override;
    peer_data get_random_peer() override;
    std::vector<peer_data> get_n_peers(const std::string& key, int nr_peers) override;
    std::vector<peer_data> get_n_random_peers(int nr_peers) override;
    void process_msg(proto::pss_message& msg) override;
    void operator()() override;

    void stop() override;
};


#endif //P2PFS_SMART_LOAD_BALANCER_H
