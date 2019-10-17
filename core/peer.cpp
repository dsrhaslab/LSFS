//
// Created by danielsf97 on 10/8/19.
//

#include <iostream>
#include "peer.h"
#include "../pss/pss_listener.h"
#include <thread>
#include <chrono>


peer::peer(long id, std::string ip, int port, double position):
    id(id), ip(ip), port(port), position(position),
    cyclon(peer::boot_ip, peer::boot_port, ip, port),
    listener("127.0.0.1", this->port, &(this->cyclon))
{
}

peer::peer(long id, std::string ip, int port, double position, long pss_sleep_interval, long pss_boot_time, int pss_view_size)
    :   id(id), ip(ip), port(port), position(position),pss_sleep_interval(pss_sleep_interval),
        pss_boot_time(pss_boot_time), pss_view_size(pss_view_size), cyclon(peer::boot_ip, peer::boot_port, ip, port),
        listener("127.0.0.1", this->port, &(this->cyclon))
{
}

void peer::print_view() {
    this->cyclon.print_view();
}

void peer::start() {
    this->pss_th = std::thread (std::ref(this->cyclon));
    this->pss_listener_th = std::thread(std::ref(this->listener));
}

void peer::stop(){
    this->listener.stop_thread();
    this->cyclon.stop_thread();
    this->pss_listener_th.join();
    this->pss_th.join();
    this->cyclon.write_view_to_file();
}

int main(int argc, char* argv []){

    if(argc < 3){
        exit(1);
    }

    int port = atoi(argv[1]);
    int exec_time = atoi(argv[2]);

    std::cout << "Starting Peer" << std::endl;
    peer peer_impl(0,"127.0.0.1",port,4);
    peer_impl.print_view();
    peer_impl.start();
    std::this_thread::sleep_for(std::chrono::seconds(exec_time));
    peer_impl.stop();
}