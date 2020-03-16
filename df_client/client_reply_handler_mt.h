//
// Created by danielsf97 on 3/8/20.
//

#ifndef P2PFS_CLIENT_REPLY_HANDLER_MT_H
#define P2PFS_CLIENT_REPLY_HANDLER_MT_H


#include <string>
#include <unordered_map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <map>
#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>
#include <kv_message.pb.h>
#include <df_store/kv_store_key.h>
#include "client_reply_handler.h"

class client_reply_handler_mt : public client_reply_handler{
private:
    int nr_worker_threads = 3;
    boost::asio::io_service io_service;
    boost::thread_group thread_pool;

public:
    client_reply_handler_mt(std::string ip, int port, int nr_puts_required, long wait_timeout);
    ~client_reply_handler_mt() = default;
    void operator ()();
    void stop();
};



#endif //P2PFS_CLIENT_REPLY_HANDLER_MT_H
