#ifndef P2PFS_CLIENT_REPLY_HANDLER_MT_H
#define P2PFS_CLIENT_REPLY_HANDLER_MT_H


#include <string>
#include <unordered_map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <map>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>
#include <regex>
#include <utility>

#include <kv_message.pb.h>

#include "client_reply_handler.h"
#include "exceptions/custom_exceptions.h"
#include "df_store/kv_store_key.h"


class client_reply_handler_mt : public client_reply_handler{
private:
    int nr_worker_threads;
    boost::asio::io_service io_service;
    boost::thread_group thread_pool;

public:
    client_reply_handler_mt(std::string ip, int kv_port, int pss_port, long wait_timeout, int nr_workers);
    ~client_reply_handler_mt() = default;
    void operator ()();
    void stop();
};



#endif //P2PFS_CLIENT_REPLY_HANDLER_MT_H
