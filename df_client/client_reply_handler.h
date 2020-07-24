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
#include "df_store/kv_store_key.h"
#include "df_store/kv_store_key_version.h"
#include <vector>
#include <kv_message.pb.h>
#include <boost/container/flat_map.hpp>

class client_reply_handler {

protected:
    //int port;
    std::string ip;
    std::unordered_map<std::string, std::vector<std::pair<kv_store_key_version, std::unique_ptr<std::string>>>> get_replies; //par versão-valor
    std::unordered_map<kv_store_key<std::string>, std::set<long>> put_replies;
    long wait_timeout;
    std::mutex get_global_mutex;
    std::map<std::string, std::unique_ptr<std::pair<std::mutex, std::condition_variable>>> get_mutexes;
    std::mutex put_global_mutex;
    std::unordered_map<kv_store_key<std::string>, std::unique_ptr<std::pair<std::mutex, std::condition_variable>>> put_mutexes;

public:
    client_reply_handler(std::string ip/*, int port*/, long wait_timeout);

    long register_put(const std::string& key, long version);
    long change_get_reqid(const std::string& latest_reqid_str, const std::string& new_reqid);
    bool wait_for_put(const kv_store_key<std::string>& key, int wait_for);
    bool wait_for_put_until(const kv_store_key<std::string>& key, int wait_for, std::chrono::system_clock::time_point& wait_until);
    void clear_put_keys_entries(std::vector<kv_store_key<std::string>>& erasing_keys);
    void register_get(const std::string& req_id);
    std::unique_ptr<std::string> wait_for_get(const std::string& req_id, int wait_for);
    std::unique_ptr<std::string> wait_for_get_until(const std::string& req_id, int wait_for, std::chrono::system_clock::time_point& wait_until);
    void clear_get_keys_entries(std::vector<std::string>& erasing_keys);
    void register_get_latest_version(const std::string& req_id);
    std::unique_ptr<long> wait_for_get_latest_version(const std::string& req_id, int wait_for);
    void process_get_reply_msg(const proto::get_reply_message &message);
    void process_put_reply_msg(const proto::put_reply_message &message);
    void process_get_latest_version_reply_msg(const proto::get_latest_version_reply_message &message);

    virtual void operator ()() = 0;
    virtual void stop() = 0;
};


#endif //P2PFS_CLIENT_REPLY_HANDLER_H
