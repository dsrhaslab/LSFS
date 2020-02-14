//
// Created by danielsf97 on 1/16/20.
//

#ifndef P2PFS_DATA_HANDLER_LISTENER_H
#define P2PFS_DATA_HANDLER_LISTENER_H


#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>
#include "df_pss/pss.h"
#include "../df_store/kv_store.h"

class data_handler_listener {
private:
    int nr_worker_threads = 3;
    boost::asio::io_service io_service;
    boost::thread_group thread_pool;
    std::shared_ptr<kv_store<std::string>> store;
    pss* pss_ptr;
    long id;
    std::string ip;
    int port;
    float chance;
    bool smart;

public:
    data_handler_listener(std::string ip, int port, long id, float chance, pss* pss, std::shared_ptr<kv_store<std::string>> store, bool smart);
    void stop_thread();
    void operator()();
};


#endif //P2PFS_DATA_HANDLER_LISTENER_H
