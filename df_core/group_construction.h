//
// Created by danielsf97 on 12/16/19.
//

#ifndef P2PFS_GROUP_CONSTRUCTION_H
#define P2PFS_GROUP_CONSTRUCTION_H


#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <spdlog/logger.h>
#include "peer_data.h"
#include "df_store/kv_store.h"

class group_construction {
private:
    std::recursive_mutex view_mutex;
    std::unordered_map<std::string, peer_data> local_view; //port -> age
    long id;
    std::string ip;
    double position;
    std::atomic<int> nr_groups;
    std::atomic<int> my_group;
    int replication_factor_max;
    int replication_factor_min;
    int max_age;
    bool local; //enable local messages
    std::atomic<int> cycle; // present cycle
    int local_interval; //interval of local messages if enable (cycle interval)
    std::atomic<bool> recovering_local_view; // if we are initiating group construction algorithm
    int sender_socket;
    std::shared_ptr<kv_store<std::string>> store;
    std::shared_ptr<spdlog::logger> logger;

public:
    group_construction(std::string ip, long id, double position, int replication_factor_min,
                       int replication_factor_max, int max_age, bool local, int local_interval, std::shared_ptr<kv_store<std::string>> store, std::shared_ptr<spdlog::logger> logger);

    std::vector<peer_data> get_local_view();


    int get_cycle();

    void set_cycle(int cycle);

    double get_position();

    int get_nr_groups();

    void set_nr_groups(int ngroups);

    void set_my_group(int group);

    int get_my_group();

    int group(double peer_pos);

    void incorporate_local_peers(const std::vector<peer_data>& received);

    void clean_local_view();

    void receive_local_message(const std::vector<peer_data>& received);

    void receive_message(const std::vector<peer_data>& received);

    void print_view();

    void send_pss_msg(const std::string& target_ip, const std::string &msg_string);

    void send_local_message(std::string& target_ip);

    void recover_local_view(const std::vector<peer_data>& received);

    bool has_recovered();

    void request_local_message(const std::vector<peer_data>& received);

    peer_data get_random_peer_from_local_view();
};

#endif //P2PFS_GROUP_CONSTRUCTION_H
