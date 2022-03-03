//
// Created by danielsf97 on 10/7/19.
//

#ifndef DATAFLASKSCPP_BOOTSTRAPPER_IMPL_H
#define DATAFLASKSCPP_BOOTSTRAPPER_IMPL_H

#include "bootstrapper.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"
#include <shared_mutex>
#include <mutex>
#include <memory>
#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>

class BootstrapperImpl: public Bootstrapper{

private:
    tcp_client_server_connection::tcp_server_connection connection;
    bool running;

    boost::asio::io_service io_service;
    boost::thread_group thread_pool;
    std::shared_mutex alive_ips_mutex;
    std::recursive_mutex fila_mutex;
    std::unordered_map<long, peer_data> alivePeers;
    std::queue<std::vector<long>> fila; //int is Peer id

    int viewsize;
    int initialnodes;
    const char* ip;

private:
    void clear_fila();

public:
    BootstrapperImpl(int viewsize, const char* ip/*, int port*/);
    void run();

    std::vector<peer_data> get_view();

    void boot_worker(int* socket);
    void add_peer(long id, std::string ip, int kv_port, int pss_port, int recover_port, double pos);
    void remove_peer(int id);

    tcp_client_server_connection::tcp_server_connection* get_connection();
    void stopThread();


    void boot_fila();

    std::string get_ip();
};

#endif //DATAFLASKSCPP_BOOTSTRAPPER_IMPL_H
