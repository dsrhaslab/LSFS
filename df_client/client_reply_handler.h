//
// Created by danielsf97 on 3/16/20.
//

#ifndef P2PFS_CLIENT_REPLY_HANDLER_H
#define P2PFS_CLIENT_REPLY_HANDLER_H

#include <string>
#include <unordered_map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <boost/container/flat_map.hpp>
#include <boost/regex.hpp>
#include <utility>

#include <kv_message.pb.h>
#include <spdlog/spdlog.h>

#include "df_store/kv_store_key.h"
#include "df_util/util.h"
#include "exceptions/custom_exceptions.h"


class client_reply_handler {

struct get_Replies {
    std::unordered_map<kv_store_key<std::string>, std::unique_ptr<std::string>> keys;
    std::unordered_map<kv_store_key<std::string>, std::unique_ptr<std::string>> deleted_keys;
    int count;
};


protected:
    std::string ip;
    int kv_port;
    int pss_port;
    //std::unordered_map<std::string, std::vector<std::tuple<kv_store_key<std::string>, std::unique_ptr<std::string>, kv_store_key<std::string>>>> get_replies; //key - data - deleted key
    std::unordered_map<std::string, get_Replies> get_replies;
    std::unordered_map<kv_store_key<std::string>, std::set<long>> put_replies;
    std::unordered_map<kv_store_key<std::string>, std::set<long>> delete_replies;
    long wait_timeout;
    std::mutex get_global_mutex;
    std::map<std::string, std::unique_ptr<std::pair<std::mutex, std::condition_variable>>> get_mutexes;
    std::mutex put_global_mutex;
    std::unordered_map<kv_store_key<std::string>, std::unique_ptr<std::pair<std::mutex, std::condition_variable>>> put_mutexes;
    std::mutex delete_global_mutex;
    std::unordered_map<kv_store_key<std::string>, std::unique_ptr<std::pair<std::mutex, std::condition_variable>>> delete_mutexes;


private: 

    void register_get(const std::string& req_id);

public:

    enum Response 
    {
      Ok, Deleted, NoData,
      Init
    };

    client_reply_handler(std::string ip, int kv_port, int pss_port, long wait_timeout);

    kv_store_key_version register_put(const std::string& key, const kv_store_key_version& version);
    void change_get_reqid(const std::string& latest_reqid_str, const std::string& new_reqid);
    bool wait_for_put(const kv_store_key<std::string>& key, int wait_for);
    bool wait_for_put_until(const kv_store_key<std::string>& key, int wait_for, std::chrono::system_clock::time_point& wait_until);
    void clear_put_keys_entries(std::vector<kv_store_key<std::string>>& erasing_keys);
    kv_store_key_version register_delete(const std::string& key, const kv_store_key_version& version);
    bool wait_for_delete(const kv_store_key<std::string>& key, int wait_for);
    void register_get_data(const std::string& req_id);
    std::unique_ptr<std::string> wait_for_get(const std::string& req_id, int wait_for, Response* get_res);
    std::unique_ptr<std::string> wait_for_get_until(const std::string& req_id, int wait_for, std::chrono::system_clock::time_point& wait_until, Response* get_res);
    void clear_get_keys_entries(std::vector<std::string>& erasing_keys);
    void register_get_latest_version(const std::string& req_id);
    std::unique_ptr<std::vector<kv_store_key_version>> wait_for_get_latest_version(const std::string& req_id, int wait_for);
    void process_get_reply_msg(const proto::get_reply_message &message);
    void process_put_reply_msg(const proto::put_reply_message &message);
    void process_delete_reply_msg(const proto::delete_reply_message &msg);
    void process_get_latest_version_reply_msg(const proto::get_latest_version_reply_message &message);

    virtual void operator ()() = 0;
    virtual void stop() = 0;
};


#endif //P2PFS_CLIENT_REPLY_HANDLER_H
