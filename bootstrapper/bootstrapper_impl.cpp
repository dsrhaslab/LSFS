//
// Created by danielsf97 on 10/7/19.
//

#include "bootstrapper_impl.h"
#include <iostream>
#include <memory>
#include "../serializer/capnp/capnp_serializer.h"
#include <thread>

#include <algorithm>    // std::random_shuffle
#include <random>      // std::rand, std::srand
#include <map>
#include <unistd.h>

#define LOG(X) std::cout << X << std::endl;

BootstrapperImpl::BootstrapperImpl(long id, int viewsize, const char* ip, int port):

    connection(tcp_client_server_connection::tcp_server_connection(ip, port, std::unique_ptr<Serializer>(new Capnp_Serializer))),
    id(id),
    viewsize(viewsize),
    ip(ip),
    port(port)

{
    std::cerr << "[bootstrapper] function: constructor [Creating Server Connection]" << std::endl;
    this->initialnodes = 10;
    this->running = true;
}

void BootstrapperImpl::stopThread(){
    this->running = false;
}

std::string BootstrapperImpl::get_ip(){
    return this->ip;
}

int BootstrapperImpl::get_port(){
    return this->port;
}

std::vector<peer_data> BootstrapperImpl::get_view() {
//    if(this->fila.empty()){
//        this->boot_fila();
//        std::vector<int> to_send = this->fila.pop();
//    }

    std::vector<peer_data> res;
    std::vector<peer_data> tmp;
    {
        std::shared_lock<std::shared_mutex> lk (this->mutex);
        for (int port: this->alivePorts) {
            peer_data peer;
            peer.ip = "127.0.0.1";
            peer.port = port;
            peer.age = 20;

            tmp.push_back(peer);
        }
    }
    if(tmp.size() <= this->viewsize){
        res = tmp;
    }else{
        auto rng = std::default_random_engine {};
        std::shuffle(std::begin(tmp), std::end(tmp), rng);
        for(peer_data peer: tmp){
            res = std::vector<peer_data>(tmp.begin(), tmp.begin() + this->viewsize);
        }
    }
    return res;
}

void BootstrapperImpl::add_peer(std::string ip, int port, long id, double pos){
    std::scoped_lock<std::shared_mutex> lk(this->mutex);
    this->alivePorts.insert(port);
    this->aliveIds.insert(std::make_pair(port,id));
    this->alivePos.insert(std::make_pair(port,pos));
};

void BootstrapperImpl::remove_peer(int port){
    std::scoped_lock<std::shared_mutex> lk(this->mutex);
    this->alivePorts.erase(port);
    this->aliveIds.erase(port);
    this->alivePos.erase(port);
};

void BootstrapperImpl::boot_fila() {

}

void boot_worker(int* socket, BootstrapperImpl* boot){
    tcp_client_server_connection::tcp_server_connection* connection = boot->get_connection();
    pss_message recv_pss_msg;

    try {

        connection->recv_pss_msg(socket, recv_pss_msg);

        switch (recv_pss_msg.type){
            case pss_message::Type::Announce: {
                pss_message msg_to_send;
                msg_to_send.view = boot->get_view();
                msg_to_send.sender_ip = boot->get_ip();
                msg_to_send.sender_port = boot->get_port();
                msg_to_send.type = pss_message::Type::Normal;

                connection->send_pss_msg(socket, msg_to_send);
                boot->add_peer(recv_pss_msg.sender_ip, recv_pss_msg.sender_port, 0, 0);
                break;
            }
            case pss_message::Type::Termination: {
                boot->remove_peer(recv_pss_msg.sender_port);
                break;
            }
        }

    }catch(...){}

    std::cerr << "[Bootstrap] function: boot_worker [Closing] socket -> " + std::to_string(*socket) << std::endl;
    close(*socket);
}

void BootstrapperImpl::run(){

    while(this->running){
        int socket = this->connection.accept_connection();
        std::cerr << "[Bootstrap] function: run [Opening] socket -> " + std::to_string(socket) << std::endl;
        std::thread newThread(boot_worker, &socket, this);
        newThread.detach();
    }
}

tcp_client_server_connection::tcp_server_connection* BootstrapperImpl::get_connection() {
    return &(this->connection);
}


int main(int argc, char *argv[]) {

    std::cout << "Starting Bootstrapper" << std::endl;

    if(argc < 2){
        exit(1);
    }

    int view_size = atoi(argv[1]);

    const char* ip = "127.0.0.1";
    std::unique_ptr<Bootstrapper> bootstrapper(new BootstrapperImpl(0, view_size, ip, 12345));
    bootstrapper->run();
};