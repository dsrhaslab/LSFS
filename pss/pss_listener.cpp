//
// Created by danielsf97 on 10/12/19.
//

#include "pss_listener.h"
#include "../serializer/capnp/capnp_serializer.h"
#include <thread>
#include <ctpl_stl.h>
#include <future>

#define LOG(X) std::cout << X << std::endl;


void pss_listener::pss_listener_worker (int* socket){

    try {
        pss_message pss_msg;
        this->connection.recv_pss_msg(socket, pss_msg);
        this->cyclon_ptr->process_msg(pss_msg);
    }catch(...){}
}

pss_listener::pss_listener(const char* ip, int port, pss* pss):
        connection(tcp_client_server_connection::tcp_server_connection(ip, port, std::unique_ptr<Serializer>(new Capnp_Serializer)))
{
    this->cyclon_ptr = pss;
    this->running = true;
    this->ip = ip;
    this->port = port;
}


void pss_listener::operator()() {

    this->running = true;

    while(this->running){
        pss_message recv_pss_msg;
        int peer_socket = this->connection.accept_connection();
        std::async(std::launch::async, &pss_listener::pss_listener_worker, this, &peer_socket);

//        std::thread worker(&pss_listener::pss_listener_worker,this, &peer_socket);
//        worker.detach();
    }
    LOG("End Listener thread");
}

void pss_listener::stop_thread() {
    LOG("Stopping Listener thread");
    this->running = false;
    //this->connection.close_socket();

    //creating temporary connection to awake thread on accept
    std::unique_ptr<Capnp_Serializer> capnp_serializer(nullptr);
    tcp_client_server_connection::tcp_client_connection connection(this->ip, this->port, nullptr);
}
