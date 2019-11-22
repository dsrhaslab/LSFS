//
// Created by danielsf97 on 10/12/19.
//

#ifndef DATAFLASKSCPP_PSS_LISTENER_H
#define DATAFLASKSCPP_PSS_LISTENER_H


#include "../tcp_client_server_connection/tcp_client_server_connection.h"
#include "pss.h"
#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>

class pss_listener {

struct ArrayWrapper{
    char arr[1024];
};

private:
    std::atomic<bool> running{};
    boost::asio::io_service ioService;
    boost::thread_group thread_pool;
    pss* pss_ptr;
    const char* ip;
    int port;

public:
    pss_listener(const char*  ip, int port, pss* pss);
    void operator ()();
    void pss_listener_worker(ArrayWrapper arr, int size);
    void stop_thread();
};

#endif //DATAFLASKSCPP_PSS_LISTENER_H
