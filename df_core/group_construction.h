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
#include <pss_message.pb.h>
#include <df_store/kv_store.h>
#include "peer_data.h"

class group_construction {
private:
    std::recursive_mutex view_mutex;
    std::unordered_map<int, peer_data> local_view; //port -> age
    long id;
    std::string ip;
    int port;
    double position;
    std::atomic<int> nr_groups;
    std::atomic<int> my_group;
    int replication_factor_max;
    int replication_factor_min;
    int max_age;
    bool local; //enable local messages
    std::atomic<int> cycle; //o ciclo em que nos encontramos
    int local_interval; //interval of local messages if enable (de quantos em quantos ciclos)
    std::atomic<bool> first_message; //se estamos a iniciar o protocolo de group construction
    int sender_socket;
    std::shared_ptr<kv_store> store;

public:
    group_construction(std::string ip, int port, long id, double position, int replication_factor_min,
                       int replication_factor_max, int max_age, bool local, int local_interval, std::shared_ptr<kv_store> store);

    std::vector<peer_data> get_local_view();


    int get_cycle();

    void set_cycle(int cycle);

    double get_position();

    int get_nr_groups();

    void set_nr_groups(int ngroups);

    void set_my_group(int group);

    int get_my_group();

    int group(double peer_pos);

    void receive_local_message(std::vector<peer_data> received);

    void receive_message(std::vector<peer_data> received);

    void print_view();

    void send_pss_msg(int target_port, std::string &msg_string);
};


#endif //P2PFS_GROUP_CONSTRUCTION_H
