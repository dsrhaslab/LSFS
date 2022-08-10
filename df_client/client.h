//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_CLIENT_H
#define P2PFS_CLIENT_H


#include <string>
#include <thread>
#include <atomic>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <memory>
#include "yaml-cpp/yaml.h"

#include "kv_message.pb.h"
#include <spdlog/logger.h>
#include "spdlog/spdlog.h"

#include "client_reply_handler_mt.h"
#include "client_reply_handler_st.h"
#include "client_reply_handler.h"
#include "df_core/peer_data.h"
#include "df_loadbalancing/dynamic_load_balancer.h"
#include "df_loadbalancing/smart_load_balancer.h"
#include "df_loadbalancing/load_balancer_listener.h"
#include "exceptions/custom_exceptions.h"
#include "df_util/message_builder/message_builder.h"
#include "clock_vv.h"
#include "df_util/util.h"
#include "clock_vv_th.h"



class client {
public:
    inline const static int boot_port = 12345;

private:
    std::string ip;
    int kv_port;
    int pss_port;
    long id;
    std::mutex sender_socket_mutex;
    int sender_socket;
    std::atomic<long> request_count;
    int nr_puts_required;
    int nr_gets_required;
    int nr_gets_version_required;
    int max_nodes_to_send_get_request;
    int max_nodes_to_send_put_request;
    int max_timeouts;
    int wait_timeout;

    std::shared_ptr<clock_vv> clock;
    std::shared_ptr<std::condition_variable> clock_cond;

    std::shared_ptr<clock_vv_th> clock_update;
    std::thread clock_update_th;

    std::shared_ptr<load_balancer> lb;
    std::thread lb_th;

    std::shared_ptr<load_balancer_listener> lb_listener;
    std::thread lb_listener_th;

    std::shared_ptr<client_reply_handler> handler;
    std::thread handler_th;

public:
    client(std::string boot_ip, std::string ip, int kv_port, int pss_port, long id, std::string conf_filename);
    void stop();
    long get_id();
    void put_batch(const std::vector<kv_store_key<std::string>> &keys, const std::vector<const char*>& datas, const std::vector<size_t>& sizes, int wait_for);
    inline void put_batch(const std::vector<kv_store_key<std::string>> &keys, const std::vector<const char*>& datas, const std::vector<size_t>& sizes) {
        put_batch(keys, datas, sizes, nr_puts_required);
    };
    void put(const std::string& key, const kv_store_key_version& version, const char* data, size_t size, int wait_for);
    inline void put(const std::string& key, const kv_store_key_version& version, const char* data, size_t size) {
        put(key, version, data, size, nr_puts_required);
    };
    void put_with_merge(const std::string& key, const kv_store_key_version& version, const char* data, size_t size, int wait_for);
    inline void put_with_merge(const std::string& key, const kv_store_key_version& version, const char* data, size_t size) {
        put_with_merge(key, version, data, size, nr_puts_required);
    };
    void del(const std::string& key, const kv_store_key_version& version, int wait_for);
    inline void del(const std::string& key, const kv_store_key_version& version){
        del(key, version, nr_puts_required);
    };
    std::unique_ptr<std::string> get(const std::string& key, const kv_store_key_version& version, client_reply_handler::Response* response, int wait_for);
    inline std::unique_ptr<std::string> get(const std::string& key, const kv_store_key_version& version, client_reply_handler::Response* response){
        return get(key, version, response, nr_gets_required);
    };
    void get_latest_batch(const std::vector<std::string> &keys, std::vector<std::shared_ptr<std::string>> &data_strs, int wait_for);
    inline void get_latest_batch(const std::vector<std::string>& keys, std::vector<std::shared_ptr<std::string>>& data_strs){
        get_latest_batch(keys, data_strs, nr_gets_required);
    };
    std::unique_ptr<kv_store_key_version> get_latest_version(const std::string& key, client_reply_handler::Response* response, int wait_for);
    inline std::unique_ptr<kv_store_key_version> get_latest_version(const std::string& key, client_reply_handler::Response* response){
        return get_latest_version(key, response, nr_gets_version_required);
    };
    std::unique_ptr<std::string> get_latest(const std::string& key, client_reply_handler::Response* response, int wait_for);
    inline std::unique_ptr<std::string> get_latest(const std::string& key, client_reply_handler::Response* response){
        return get_latest(key, response, nr_gets_version_required);
    };

    std::vector<std::unique_ptr<std::string>> get_latest_concurrent(const std::string& key, client_reply_handler::Response* response, int wait_for);
    inline std::vector<std::unique_ptr<std::string>> get_latest_concurrent(const std::string& key, client_reply_handler::Response* response){
        return get_latest_concurrent(key, response, nr_gets_version_required);
    }

    void put_child(const std::string& key, const kv_store_key_version& version, const std::string& child_path, bool is_create, bool is_dir, int wait_for);
    inline void put_child(const std::string& key, const kv_store_key_version& version, const std::string& child_path, bool is_create, bool is_dir){
        return put_child(key, version, child_path, is_create, is_dir, nr_puts_required);
    }

    void put_metadata_stat(const std::string& key, const kv_store_key_version& version,const char *data, size_t size, int wait_for);
    inline void put_metadata_stat(const std::string& key, const kv_store_key_version& version, const char *data, size_t size){
        put_metadata_stat(key, version, data, size, nr_puts_required);
    }
    
    std::unique_ptr<std::string> get_latest_metadata_size(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version, int wait_for);
    inline std::unique_ptr<std::string> get_latest_metadata_size(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version) {
        return get_latest_metadata_size(key, response, last_version, nr_gets_version_required);
    }

    std::unique_ptr<std::string> get_latest_metadata_stat(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version, int wait_for);
    inline std::unique_ptr<std::string> get_latest_metadata_stat(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version) {
        return get_latest_metadata_stat(key, response, last_version, nr_gets_version_required);
    }

    void get_metadata_batch(const std::vector<kv_store_key<std::string>> &keys, std::vector<std::shared_ptr<std::string>> &data_strs, int wait_for);
    inline void get_metadata_batch(const std::vector<kv_store_key<std::string>> &keys, std::vector<std::shared_ptr<std::string>> &data_strs){
        return get_metadata_batch(keys, data_strs, nr_gets_required);
    }


private:
    long inc_and_get_request_count();
    int send_msg(peer_data& target_peer, proto::kv_message& msg);
    int send_get(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const std::string& req_id);
    int send_put(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const char* data, size_t size);
    int send_delete(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version);
    int send_put_with_merge(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const char* data, size_t size);
    int send_get_latest_version(std::vector<peer_data>& peers, const std::string& key, const std::string& req_id, bool with_data = false);
    int send_put_child(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const kv_store_key_version& past_version, const std::string& child_path, bool is_create, bool is_dir);
    int send_put_metadata_stat(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const kv_store_key_version& past_version, const char *data, size_t size);
    int send_get_latest_met_size_or_stat(std::vector<peer_data>& peers, const std::string& key, const std::string& req_id, bool get_size, bool get_stat);
    int send_get_metadata(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const std::string& req_id);

    std::unique_ptr<std::string> get_latest_metadata_size_or_stat(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version, bool get_size, bool get_stat, int wait_for);

};

#endif //P2PFS_CLIENT_H
