//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_CLIENT_H
#define P2PFS_CLIENT_H


#include <string>
#include <thread>
#include <atomic>
#include <utility>
#include "client_reply_handler_st.h"
#include "df_loadbalancing/dynamic_load_balancer.h"
#include "df_loadbalancing/load_balancer_listener.h"
#include "kv_message.pb.h"
#include "df_core/peer_data.h"
#include "client_reply_handler_mt.h"

class client {
private:
    std::string ip;
    long id;
    int port;
    int sender_socket;
    std::atomic<long> request_count;
    int nr_puts_required;
    int nr_gets_required;
    int nr_gets_version_required;
    int max_timeouts;

    std::shared_ptr<dynamic_load_balancer> lb;
    std::thread lb_th;

    std::shared_ptr<load_balancer_listener> lb_listener;
    std::thread lb_listener_th;

    std::shared_ptr<client_reply_handler> handler;
    std::thread handler_th;

public:
    client(std::string ip, long id, int port, int lb_port, const char* conf_filename);
    void stop();
    std::set<long> put(std::string key, long version, const char* data, size_t size, int wait_for);
    inline std::set<long> put(std::string key, long version, const char* data, size_t size) {
        return put(std::move(key), version, data, size, nr_puts_required);
    };
    std::set<long> put_with_merge(std::string key, long version, const char* data, size_t size, int wait_for);
    inline std::set<long> put_with_merge(std::string key, long version, const char* data, size_t size) {
        return put_with_merge(std::move(key), version, data, size, nr_puts_required);
    };
    std::shared_ptr<std::string> get(std::string key, int wait_for, long* version = nullptr);
    inline std::shared_ptr<std::string> get(std::string key, long* version = nullptr){
      return get(std::move(key), nr_gets_required, version);
    };
    long get_latest_version(std::string key, int wait_for);
    inline long get_latest_version(std::string key){
        return get_latest_version(std::move(key), nr_gets_version_required);
    };

private:
    long inc_and_get_request_count();
    int send_msg(peer_data& target_peer, proto::kv_message& msg);
    int send_get(peer_data &peer, std::string key, long* version, std::string req_id);
    int send_put(peer_data& peer, std::string key, long version, const char* data, size_t size);
    int send_put_with_merge(peer_data& peer, std::string key, long version, const char* data, size_t size);
    int send_get_latest_version(peer_data &peer, std::string key, std::string req_id);
};


#endif //P2PFS_CLIENT_H
