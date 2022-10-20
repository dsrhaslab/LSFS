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
#include <fstream>
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
    bool metadata_higher_version;
};


protected:
    std::string ip;
    int kv_port;
    int pss_port;
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

    std::mutex dummy_mutex;
    std::map<std::string, std::unique_ptr<std::pair<std::mutex, std::condition_variable>>> dummy_mutexes;
    std::unordered_map<std::string, std::set<long>> dummy_replies;


private: 

    void register_get(const std::string& req_id);

public:

    enum Response 
    {
      Ok, Deleted, NoData, NoData_HigherVersion,
      Init
    };

    client_reply_handler(std::string ip, int kv_port, int pss_port, long wait_timeout);

    void register_put(const kv_store_key<std::string>& comp_key);
    void change_get_reqid(const std::string& latest_reqid_str, const std::string& new_reqid);
    bool wait_for_put(const kv_store_key<std::string>& key, int wait_for);
    bool wait_for_put_until(const kv_store_key<std::string>& key, int wait_for, std::chrono::system_clock::time_point& wait_until);
    void clear_put_keys_entries(std::vector<kv_store_key<std::string>>& erasing_keys);
    void register_delete(const kv_store_key<std::string>& comp_key);
    bool wait_for_delete(const kv_store_key<std::string>& key, int wait_for);
    void register_get_data(const std::string& req_id);
    std::unique_ptr<std::string> wait_for_get(const std::string& req_id, int wait_for, Response* get_res);
    size_t wait_for_get_metadata_size(const std::string& req_id, int wait_for, Response* get_res);
    std::unique_ptr<std::string> wait_for_get_metadata_until(const std::string& req_id, int wait_for, std::chrono::system_clock::time_point& wait_until, Response* get_res);
    void clear_get_keys_entries(std::vector<std::string>& erasing_keys);
    void register_get_latest_version(const std::string& req_id);
    std::unique_ptr<kv_store_version> wait_for_get_latest_version(const std::string& req_id, int wait_for, Response* get_res);
    std::unique_ptr<std::string> wait_for_get_latest_until(const std::string &key, const std::string &req_id, int wait_for,
                                                                      std::chrono::system_clock::time_point &wait_until, Response* get_res);
    std::unique_ptr<std::string> wait_for_get_latest(const std::string &key, const std::string& req_id, int wait_for, Response* get_res, kv_store_version* last_version);
    std::vector<std::unique_ptr<std::string>> wait_for_get_latest_concurrent(const std::string &key, const std::string& req_id, int wait_for, Response* get_res);
    void process_get_reply_msg(const proto::get_reply_message &message);
    void process_get_metadata_reply_msg(const proto::get_metadata_reply_message &msg);
    void process_put_reply_msg(const proto::put_reply_message &message);
    void process_delete_reply_msg(const proto::delete_reply_message &msg);
    void process_get_latest_version_reply_msg(const proto::get_latest_version_reply_message &message);

    void process_dummy_msg(const proto::dummy &msg);
    bool wait_for_dummy(const std::string& key);
    void register_dummy(const std::string& key);

    virtual void operator ()() = 0;
    virtual void stop() = 0;
};


#endif //P2PFS_CLIENT_REPLY_HANDLER_H
