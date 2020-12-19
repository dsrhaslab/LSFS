//
// Created by danielsf97 on 10/12/19.
//

#ifndef DATAFLASKSCPP_PSS_LISTENER_H
#define DATAFLASKSCPP_PSS_LISTENER_H


#include "df_tcp_client_server_connection/tcp_client_server_connection.h"
#include "pss.h"
#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>
#include <fstream>

class pss_listener {

private:
    std::ofstream outfile;
    std::string logging_dir;

    int nr_worker_threads = 3;
    boost::asio::io_service io_service;
    boost::thread_group thread_pool;
    pss* pss_ptr;

public:
    pss_listener(pss* pss);
    void operator ()();
    void log_to_file(std::string log);
    void stop_thread();
};

#endif //DATAFLASKSCPP_PSS_LISTENER_H