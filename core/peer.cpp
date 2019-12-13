//
// Created by danielsf97 on 10/8/19.
//

#include <iostream>
#include "peer.h"
#include <thread>
#include <chrono>
#include <signal.h>
#include <time.h>

std::shared_ptr<peer> g_peer_impl;

peer::peer(long id, std::string ip, int port, double position):
    id(id), ip(ip), port(port), position(position),
    cyclon(peer::boot_ip, peer::boot_port, ip, port,2,5,10,3),
    listener("127.0.0.1", this->port, &(this->cyclon), "../logging/"),
    v_logger(this->port, &(this->cyclon),60, "../logging/")
{
}

peer::peer(long id, std::string ip, int port, double position, long pss_boot_time, int pss_view_size, long pss_sleep_interval, int pss_gossip_size, int logging_interval, std::string logging_dir)
    :   id(id), ip(ip), port(port), position(position),
        cyclon(peer::boot_ip, peer::boot_port, ip, port, pss_boot_time, pss_view_size, pss_sleep_interval, pss_gossip_size),
        listener("127.0.0.1", this->port, &(this->cyclon), logging_dir),
        v_logger(this->port, &(this->cyclon), logging_interval, logging_dir)
{
}

void peer::print_view() {
    this->cyclon.print_view();
}

void peer::start() {
    this->pss_th = std::thread (std::ref(this->cyclon));
    this->pss_listener_th = std::thread(std::ref(this->listener));
    this->v_logger_th = std::thread(std::ref(this->v_logger));
}

void peer::stop(){
    this->v_logger.stop_thread();
    this->cyclon.stop_thread();
    this->listener.stop_thread();
    this->pss_listener_th.join();
    this->pss_th.join();
    this->v_logger_th.join();
}

void peer::join(){
    this->pss_listener_th.join();
    this->pss_th.join();
    this->v_logger_th.join();
}

void term_handler(int i){
    g_peer_impl->stop();
    std::cout << "Terminating with handler!!" << std::endl;
    exit(1);
}


int main(int argc, char* argv []){

    if(argc < 6){
        exit(1);
    }

    signal(SIGTERM, term_handler);

    int port = atoi(argv[1]);
    int view_size = atoi(argv[2]);
    int gossip_size = atoi(argv[3]);
    int sleep_interval = atoi(argv[4]);
    int logging_interval = atoi(argv[5]);

    std::string logging_dir;

    if(argc > 6){
        logging_dir = argv[6];
    }else{
        logging_dir = "../logging/";
    }


//    std::cout << "Starting Peer" << std::endl;
    g_peer_impl = std::make_shared<peer>(0,"127.0.0.1",port,4,2,view_size,sleep_interval,gossip_size, logging_interval, logging_dir);
//    g_peer_impl->print_view();
    g_peer_impl->start();
    g_peer_impl->join();
}