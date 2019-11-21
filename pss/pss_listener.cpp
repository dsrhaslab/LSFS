//
// Created by danielsf97 on 10/12/19.
//

#include "pss_listener.h"
#include "../serializer/capnp/capnp_serializer.h"
#include <thread>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include "../core/peer.h"
#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <pss_message.pb.h>
#include <ctime>
#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>

#define LOG(X) std::cout << X << std::endl;


using namespace boost;
using asio::ip::udp;
using system::error_code;

void handle_function(const char* data, size_t size, pss* pss_ptr)
{

    try {
        proto::pss_message pss_message;
        pss_message.ParseFromArray(data, size);

        pss_ptr->process_msg(pss_message);
    }
    catch(const char* e){
        std::cerr << e << std::endl;
    }
    catch(...){}
}

class udp_server; // forward declaration

struct udp_session : enable_shared_from_this<udp_session> {

    udp_session(udp_server* server, pss* pss_ptr) : server_(server), pss_ptr(pss_ptr) {}

    void handle_request(const boost::system::error_code& error);

    udp::endpoint remote_endpoint_;
    enum { max_length = 1024 };
    char recv_buffer_ [max_length];
    udp_server* server_;
    pss* pss_ptr;
};

class udp_server
{
    typedef boost::shared_ptr<udp_session> shared_session;
public:
    udp_server(asio::io_service &io_service, int port, pss *pss_ptr)
            : socket_(io_service, udp::endpoint(udp::v4(), port)),
              strand_(io_service), pss_ptr(pss_ptr)
    {
        receive_session();
    }

private:
    void receive_session()
    {
        // our session to hold the buffer + endpoint
        auto session = boost::make_shared<udp_session>(this, this->pss_ptr);

        socket_.async_receive_from(
                boost::asio::buffer(session->recv_buffer_),
                session->remote_endpoint_,
                strand_.wrap(
                        bind(&udp_server::handle_receive, this,
                             session, // keep-alive of buffer/endpoint
                             boost::asio::placeholders::error,
                             boost::asio::placeholders::bytes_transferred)));
    }

    void handle_receive(shared_session session, const boost::system::error_code& ec, std::size_t /*bytes_transferred*/) {
        // now, handle the current session on any available pool thread
        post(socket_.get_executor(),bind(&udp_session::handle_request, session, ec));

        // immediately accept new datagrams
        receive_session();
    }

    udp::socket  socket_;
    boost::asio::io_service::strand strand_;
    pss* pss_ptr;

    friend struct udp_session;
};

void udp_session::handle_request(const boost::system::error_code& error)
{
    if (!error || error == boost::asio::error::message_size)
    {
        handle_function(this->recv_buffer_, 1024, this->pss_ptr);
    }
}

pss_listener::pss_listener(const char* ip, int port, pss* pss)
{
    std::cerr << "[pss_listener] function: constructor [Creating Server Connection]" << std::endl;
    this->pss_ptr = pss;
    this->ip = ip;
    this->port = port;
}

void pss_listener::operator()() {
    try {
        udp_server server(this->io_service, this->port, this->pss_ptr);

        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));

        this->thread_pool.join_all();
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

void pss_listener::stop_thread() {
    LOG("Stopping Listener thread");

    //terminating ioService processing loop
    this->io_service.stop();
    //joining all threads of thread_loop
    this->thread_pool.join_all();
    LOG("JOINT ALL THREADS FROM POOL!!!")
}


//void pss_listener::pss_listener_worker (int* socket){
//
//    try {
//        pss_message pss_msg;
//        this->connection.recv_pss_msg(socket, pss_msg);
//        this->cyclon_ptr->process_msg(pss_msg);
//    }
//    catch(const char* e){
//        std::cerr << e << std::endl;
//    }
//    catch(...){}
//
//    std::cerr << "[pss_listener] function: pss_listener_worker [Closing] client socket -> " + std::to_string(*socket) << std::endl;
//    close(*socket);
//}
//
//pss_listener::pss_listener(const char* ip, int port, pss* pss):
//        connection(tcp_client_server_connection::tcp_server_connection(ip, port, std::unique_ptr<Serializer>(new Capnp_Serializer)))
//{
//    std::cerr << "[pss_listener] function: constructor [Creating Server Connection]" << std::endl;
//    this->cyclon_ptr = pss;
//    this->running = true;
//    this->ip = ip;
//    this->port = port;
//}
//
//
//void pss_listener::operator()() {
//
//    int nr_threads = peer::pss_listener_thread_loop_size;
//    boost::asio::io_service::work work(this->ioService);
//    for(int i = 0; i < nr_threads; ++i){
//        this->thread_pool.create_thread(
//                boost::bind(&boost::asio::io_service::run, &(this->ioService))
//        );
//    }
//
//    this->running = true;
//
//    struct timeval* timeout = (struct timeval*) malloc(sizeof(struct timeval));
//    timeout->tv_sec = 1;
//    timeout->tv_usec = 0;
//
//    while(this->running){
//        pss_message recv_pss_msg;
//        int peer_socket = this->connection.accept_connection();
//        std::cerr << "[pss_listener] function: operator [Opening] client socket -> " + std::to_string(peer_socket) << std::endl;
//        this->ioService.post(boost::bind(&pss_listener::pss_listener_worker, this, &peer_socket));
//    }
//    LOG("End Listener thread");
//}
//
//void pss_listener::stop_thread() {
//    LOG("Stopping Listener thread");
//    this->running = false;
//
//    //creating temporary connection to awake thread on accept
//    std::unique_ptr<Capnp_Serializer> capnp_serializer(nullptr);
//
//    std::cerr << "[pss_listener] function: stop_thread [Creating Connection]" << std::endl;
//    tcp_client_server_connection::tcp_client_connection connection(this->ip, this->port, nullptr);
//
//    //terminating ioService processing loop
//    this->ioService.stop();
//    //joining all threads of thread_loop
//    this->thread_pool.join_all();
//    LOG("JOINT ALL THREADS FROM POOL!!!")
//}
