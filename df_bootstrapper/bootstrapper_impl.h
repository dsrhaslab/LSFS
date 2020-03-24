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
    //DFLogger log;
    tcp_client_server_connection::tcp_server_connection connection;
    bool running;
    //std::unordered_set<std::string> aliveIPs;
//    std::unordered_map<std::string, long> aliveIds;
//    std::unordered_map<std::string, double> alivePos;
//    std::queue<std::vector<std::string>> fila; //era concurrent linked queue

    boost::asio::io_service io_service;
    boost::thread_group thread_pool;
    std::shared_mutex alive_ports_mutex;
    std::recursive_mutex fila_mutex;
    std::unordered_set<int> alivePorts;
    std::unordered_map<int, long> aliveIds;
    std::unordered_map<int, double> alivePos;
    std::queue<std::vector<int>> fila; //era concurrent linked queue

    int viewsize;
    int initialnodes;
    int port;
    const char* ip;

public:
    BootstrapperImpl(int viewsize, const char* ip, int port);
    void run();
    //void addIP(std::string ip, long id, double pos);
    //void removeIP(std::string ip);

    std::vector<peer_data> get_view();

    void boot_worker(int* socket);
    void add_peer(std::string ip,int port, long id, double pos);
    void remove_peer(int port);

    tcp_client_server_connection::tcp_server_connection* get_connection();
    void stopThread();


    void boot_fila();

    std::string get_ip();

    int get_port();
};

#endif //DATAFLASKSCPP_BOOTSTRAPPER_IMPL_H
