//
// Created by danielsf97 on 10/8/19.
//

#ifndef DATAFLASKSCPP_PSS_H
#define DATAFLASKSCPP_PSS_H

#include <unordered_map>
#include <memory>
#include <vector>
#include "../core/peer_data.h"
#include "pss_message.h"
#include <mutex>
#include <atomic>
#include <pss_message.pb.h>

class pss {
private:
    std::recursive_mutex view_mutex;
    std::recursive_mutex last_view_mutex;
    std::unordered_map<int, peer_data> view; //port -> age
    std::vector<peer_data> last_sent_view;
    int port;
    int view_size;
    std::string ip;
    long id;
    long boot_time;
    std::atomic<bool> running;
    int sleep_interval;
    int gossip_size;
    const char* boot_ip;
    int boot_port;
    int socket_send;

public:
    pss(const char* boot_ip, int boot_port,  std::string my_ip, int my_port);
    pss(const char* boot_ip, int boot_port, std::string my_ip, int my_port, long boot_time, int view_size, int sleep, int gossip_size);
    void operator()();
    void print_view();
    void process_msg(proto::pss_message message);
    void write_view_to_file();
    void stop_thread();
    std::vector<int> get_peers_from_view();

    void bootstrapper_termination_alerting();

private:
    void age_view_members();
    void complete_view_with_last_sent();
    peer_data* get_older_from_view();
    std::vector<peer_data> select_view_to_send(int target_port);
    void send_pss_msg(int target_port, std::vector<peer_data>& view_to_send, proto::pss_message_Type);
    void incorporate_in_view(std::vector<peer_data>& vector);
    void incorporate_last_sent_view();
};


#endif //DATAFLASKSCPP_PSS_H
