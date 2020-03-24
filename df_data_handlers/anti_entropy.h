//
// Created by danielsf97 on 1/30/20.
//

#ifndef P2PFS_ANTI_ENTROPY_H
#define P2PFS_ANTI_ENTROPY_H

#include <vector>
#include <atomic>
#include <kv_message.pb.h>
#include "df_pss/pss.h"


class anti_entropy{
private:
    pss* pss_ptr;
    std::shared_ptr<kv_store<std::string>> store;
    std::atomic<bool> running;
    long sleep_interval;
    int sender_socket;
    std::string ip;
    int respond_to_port;
    long id;

public:
    anti_entropy(std::string ip, int respond_to_port, long id, pss* pss_ptr, std::shared_ptr<kv_store<std::string>> store, long sleep_interval);
    void operator()();
    void stop_thread();

private:
    void send_peer_keys(std::vector<peer_data>& target_peers, proto::kv_message &msg);
};

#endif //P2PFS_ANTI_ENTROPY_H
