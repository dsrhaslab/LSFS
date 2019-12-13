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
#include <fstream>

class pss_listener {

// uncomment this for first version
//struct ArrayWrapper{
//    char arr[1024];
//};

private:
// uncomment this for first version
//    std::atomic<bool> running;
//    boost::asio::io_service ioService;

    std::ofstream outfile;
    std::string logging_dir;

    int nr_worker_threads = 3;
    boost::asio::io_service io_service;
    boost::thread_group thread_pool;
    pss* pss_ptr;
    const char* ip;
    int port;

public:
    pss_listener(const char*  ip, int port, pss* pss, std::string logging_directory);
    void operator ()();
// uncomment this for first version
//    void pss_listener_worker(ArrayWrapper arr, int size);

    void log_to_file(std::string log);


    void stop_thread();
};

#endif //DATAFLASKSCPP_PSS_LISTENER_H