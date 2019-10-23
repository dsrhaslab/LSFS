//
// Created by danielsf97 on 10/12/19.
//

#include "pss_listener.h"
#include "../serializer/capnp/capnp_serializer.h"
#include <thread>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include "../core/peer.h"

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

    int nr_threads = peer::pss_listener_thread_loop_size;
    boost::asio::io_service::work work(this->ioService);
    for(int i = 0; i < nr_threads; ++i){
        this->thread_pool.create_thread(
                boost::bind(&boost::asio::io_service::run, &(this->ioService))
        );
    }

    this->running = true;

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    while(this->running){
        pss_message recv_pss_msg;
        int peer_socket = this->connection.accept_connection();
        setsockopt(peer_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
        this->ioService.post(boost::bind(&pss_listener::pss_listener_worker, this, &peer_socket));
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

    //terminating ioService processing loop
    this->ioService.stop();
    //joining all threads of thread_loop
    this->thread_pool.join_all();
    LOG("JOINT ALL THREADS FROM POOL!!!")
}
