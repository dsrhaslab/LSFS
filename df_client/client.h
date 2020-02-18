//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_CLIENT_H
#define P2PFS_CLIENT_H


#include <string>
#include <thread>
#include <atomic>
#include "client_reply_handler.h"
#include "../df_loadbalancing/dynamic_load_balancer.h"
#include "../df_loadbalancing/load_balancer_listener.h"
#include "../kv_message.pb.h"
#include "../df_core/peer_data.h"

class client {
private:
    std::string ip;
    long id;
    int port;
    int sender_socket;
    std::atomic<long> request_count;
    int nr_puts_required;

    std::shared_ptr<dynamic_load_balancer> lb;
    std::thread lb_th;

    std::shared_ptr<load_balancer_listener> lb_listener;
    std::thread lb_listener_th;

    std::shared_ptr<client_reply_handler> handler;
    std::thread handler_th;

public:
    client(std::string ip, long id, int port, int lb_port);
    void stop();
    std::set<long> put(std::string key, long version, const char* data, size_t size);
    std::shared_ptr<std::string> get(long node_id, std::string key, long version);

private:
    long inc_and_get_request_count();
    int send_msg(peer_data& target_peer, proto::kv_message& msg);
    int send_get(peer_data &peer, std::string key, long version, std::string req_id);
    int send_put(peer_data& peer, std::string key, long version, const char* data, size_t size);
};


#endif //P2PFS_CLIENT_H
