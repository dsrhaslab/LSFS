//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_CLIENT_REPLY_HANDLER_H
#define P2PFS_CLIENT_REPLY_HANDLER_H


#include <string>
#include <unordered_map>
#include <set>
#include <mutex>
#include <condition_variable>

class client_reply_handler {
private:
    int running;
    int socket_rcv;
    int port;
    std::string ip;
    std::unordered_map<std::string, std::shared_ptr<const char[]>> get_replies;
    std::unordered_map<long, std::set<long>> put_replies;
    int nr_puts_required;
    long wait_timeout;
    std::mutex get_mutex;
    std::condition_variable get_cond_var;
    std::mutex put_mutex;
    std::condition_variable put_cond_var;

public:
    client_reply_handler(std::string ip, int port, int nr_puts_required, long wait_timeout);
    ~client_reply_handler();
    void operator ()();
    void register_put(long key);
    std::unique_ptr<std::set<long>> wait_for_put(long key);
    void register_get(std::string req_id);
    std::shared_ptr<const char[]> wait_for_get(std::string req_id);
    void stop();
};


#endif //P2PFS_CLIENT_REPLY_HANDLER_H
