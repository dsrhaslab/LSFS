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
#include <df_store/kv_store_key.h>
#include <vector>
#include <kv_message.pb.h>

class client_reply_handler {

protected:
    int port;
    std::string ip;
    std::unordered_map<std::string, std::vector<std::pair<long, std::shared_ptr<std::string>>>> get_replies; //par versão-valor
    std::unordered_map<kv_store_key<std::string>, std::set<long>> put_replies;
    int nr_puts_required;
    long wait_timeout;
    std::mutex get_global_mutex;
    std::map<std::string, std::pair<std::unique_ptr<std::mutex>, std::unique_ptr<std::condition_variable>>> get_mutexes;
    std::mutex put_global_mutex;
    std::map<kv_store_key<std::string>, std::pair<std::unique_ptr<std::mutex>, std::unique_ptr<std::condition_variable>>> put_mutexes;

public:
    client_reply_handler(std::string ip, int port, int nr_puts_required, long wait_timeout);

    long register_put(std::string key, long version);
    std::unique_ptr<std::set<long>> wait_for_put(kv_store_key<std::string> key);
    void register_get(std::string req_id);
    std::shared_ptr<std::string> wait_for_get(std::string req_id, int wait_for);
    void register_get_latest_version(std::string req_id);
    std::unique_ptr<long> wait_for_get_latest_version(std::string req_id, int wait_for);
    void process_get_reply_msg(const proto::get_reply_message &message);
    void process_put_reply_msg(const proto::put_reply_message &message);
    void process_get_latest_version_reply_msg(const proto::get_latest_version_reply_message &message);

    virtual void operator ()() = 0;
    virtual void stop() = 0;
};


#endif //P2PFS_CLIENT_REPLY_HANDLER_H
