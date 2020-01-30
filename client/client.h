//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_CLIENT_H
#define P2PFS_CLIENT_H


#include <string>
#include <loadbalancing/load_balancer.h>
#include <thread>
#include <atomic>
#include <kv_message.pb.h>
#include "client_reply_handler.h"

class client {
private:
    std::string ip;
    long id;
    int port;
    int sender_socket;
    load_balancer* lb;
    std::atomic<long> request_count;
    int nr_puts_required;
    client_reply_handler handler;
    std::thread handler_th;

public:
    client(std::string ip, long id, int port, load_balancer* lb, int nr_puts_required, long wait_timeout);
    void stop();
    std::set<long> put(long key, long version, const char* data);
    std::shared_ptr<const char []> get(long node_id, long key, long version);

private:
    long inc_and_get_request_count();
    int send_msg(peer_data& target_peer, proto::kv_message& msg);
    int send_get(peer_data &peer, long key, long version, std::string req_id);
    int send_put(peer_data& peer, long key, long version, const char* data);
};


#endif //P2PFS_CLIENT_H
