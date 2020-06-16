//
// Created by danielsf97 on 10/12/19.
//

#include "pss_listener.h"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include "df_core/peer.h"
#include <iostream>
#include <pss_message.pb.h>
#include <ctime>
#include <string>
#include "df_communication/udp_async_server.h"


#define LOG(X) std::cout << X << std::endl;

class pss_listener_worker : public udp_handler{
private:
    pss* pss_ptr;

public:
    pss_listener_worker(pss* pss): pss_ptr(pss){}

    void handle_function(const char *data, size_t size) override {
        try {
            proto::pss_message pss_message;
            pss_message.ParseFromArray(data, size);

            this->pss_ptr->process_msg(pss_message);
        }
        catch(const char* e){
            spdlog::error(e);
//            std::cout << e << std::endl;
        }
        catch(...){}
    }
};

pss_listener::pss_listener(/*const char* ip, int port,*/ pss* pss)
{
//    std::cerr << "[pss_listener] function: constructor [Creating Server Connection]" << std::endl;
    this->pss_ptr = pss;
//    this->ip = ip;
//    this->port = port;
}

void pss_listener::operator()() {
    try {
        pss_listener_worker worker(this->pss_ptr);
        udp_async_server server(this->io_service, peer::pss_port/*this->port*/,(udp_handler*) &worker);

        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));

        this->thread_pool.join_all();
    }
    catch(const std::system_error& e) {
        std::cerr << "System Error: Not avaliable resources to create peer (pss_listener)!" << std::endl;
        exit(1);
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
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

//second version

//#include "pss_listener.h"
//#include "../df_serializer/capnp/capnp_serializer.h"
//#include <thread>
//#include <boost/asio/io_service.hpp>
//#include <boost/bind.hpp>
//#include "../df_core/peer.h"
//#include <cstdlib>
//#include <iostream>
//#include <boost/asio.hpp>
//#include <pss_message.pb.h>
//#include <ctime>
//#include <string>
//#include <boost/shared_ptr.hpp>
//#include <boost/enable_shared_from_this.hpp>
//#include <boost/make_shared.hpp>
//#include <boost/thread.hpp>
//
//#define LOG(X) std::cout << X << std::endl;
//
//// async boost server version //problema de para muitoes processos o
//// boost acho que têm shared resources e passa a não receber msgs
//
//using namespace boost;
//using asio::ip::udp;
//using system::error_code;
//
//void handle_function(const char* data, size_t size, df_pss* pss_ptr)
//{
//
//    try {
//        proto::pss_message pss_message;
//        pss_message.ParseFromArray(data, size);
//
//        pss_ptr->process_msg(pss_message);
//    }
//    catch(const char* e){
//        std::cout << e << std::endl;
//    }
//    catch(...){}
//}
//
//class udp_server; // forward declaration
//
//struct udp_session : enable_shared_from_this<udp_session> {
//
//    udp_session(udp_server* server, df_pss* pss_ptr) : server_(server), pss_ptr(pss_ptr) {}
//
//    void handle_request(const boost::system::error_code& error);
//
//    udp::endpoint remote_endpoint_;
//    enum { max_length = 1024 };
//    char recv_buffer_ [max_length];
//    std::size_t bytes_rcv;
//    udp_server* server_;
//    df_pss* pss_ptr;
//
//public:
//    void set_bytes_rcv(size_t bytes_rcv){
//        this->bytes_rcv = bytes_rcv;
//    }
//};
//
//class udp_server
//{
//    typedef boost::shared_ptr<udp_session> shared_session;
//public:
//    udp_server(asio::io_service &io_service, int port, df_pss *pss_ptr)
//            : socket_(io_service, udp::endpoint(udp::v4(), port)),
//              strand_(io_service), pss_ptr(pss_ptr)
//    {
//        receive_session();
//    }
//
//private:
//    void receive_session()
//    {
//        // our session to hold the buffer + endpoint
//        auto session = boost::make_shared<udp_session>(this, this->pss_ptr);
//
//        socket_.async_receive_from(
//                boost::asio::buffer(session->recv_buffer_),
//                session->remote_endpoint_,
//                strand_.wrap(
//                        bind(&udp_server::handle_receive, this,
//                             session, // keep-alive of buffer/endpoint
//                             boost::asio::placeholders::error,
//                             boost::asio::placeholders::bytes_transferred)));
//    }
//
//    void handle_receive(shared_session session, const boost::system::error_code& ec, std::size_t bytes_rcv/*bytes_transferred*/) {
//        // now, handle the current session on any available pool thread
//        session->set_bytes_rcv(bytes_rcv);
//        post(socket_.get_executor(),bind(&udp_session::handle_request, session, ec));
//
//        // immediately accept new datagrams
//        receive_session();
//    }
//
//    udp::socket  socket_;
//    boost::asio::io_service::strand strand_;
//    df_pss* pss_ptr;
//
//    friend struct udp_session;
//};
//
//void udp_session::handle_request(const boost::system::error_code& error)
//{
//    if (!error || error == boost::asio::error::message_size)
//    {
//        handle_function(this->recv_buffer_, this->bytes_rcv, this->pss_ptr);
//    }
//}
//
//pss_listener::pss_listener(const char* ip, int port, df_pss* df_pss)
//{
////    std::cerr << "[pss_listener] function: constructor [Creating Server Connection]" << std::endl;
//    this->pss_ptr = df_pss;
//    this->ip = ip;
//    this->port = port;
//}
//
//void pss_listener::operator()() {
//    try {
//        udp_server server(this->io_service, this->port, this->pss_ptr);
//
//        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
//            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));
//
//        this->thread_pool.join_all();
//    }
//    catch (std::exception& e) {
//        std::cout << e.what() << std::endl;
//    }
//}
//
//void pss_listener::stop_thread() {
//    LOG("Stopping Listener thread");
//
//    //terminating ioService processing loop
//    this->io_service.stop();
//    //joining all threads of thread_loop
//    this->thread_pool.join_all();
//    LOG("JOINT ALL THREADS FROM POOL!!!")
//}



// first version

//void pss_listener::pss_listener_worker (ArrayWrapper arr, int size){
//
//    try {
//        proto::pss_message pss_message;
//        pss_message.ParseFromArray(arr.arr, size);
//        std::cout << "received a msg" <<std::endl;
//        this->pss_ptr->process_msg(pss_message);
//    }
//    catch(const char* e){
//        std::cerr << e << std::endl;
//    }
//    catch(...){}
//}
//
//pss_listener::pss_listener(const char* ip, int port, df_pss* df_pss)
//{
//    this->pss_ptr = df_pss;
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
//    struct sockaddr_in si_me, si_other;
//    pss_message recv_message;
//    socklen_t addr_size;
//
//    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
//
//    memset(&si_me, '\0', sizeof(si_me));
//    si_me.sin_family = AF_INET;
//    si_me.sin_port = htons(port);
//    si_me.sin_addr.s_addr = inet_addr("127.0.0.1");

//    bind(sockfd, (struct sockaddr*)&si_me, sizeof(si_me));
//    addr_size = sizeof(si_other);
//
//    char buf [1024];
//
//    while(this->running){
//        int bytes_rcv = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*)& si_other, &addr_size);
//        if(this->running) {
//            ArrayWrapper temp;
//            std::copy(std::begin(buf), std::end(buf), std::begin(temp.arr));
//            this->ioService.post(boost::bind(&pss_listener::pss_listener_worker, this, temp, bytes_rcv));
//        }
//    }
//    LOG("End Listener thread");
//}
//
//void send_special_msg(int target_port){
//    struct sockaddr_in serverAddr;
//    socklen_t addr_size;
//    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
//    memset(&serverAddr, '\0', sizeof(serverAddr));
//
//    serverAddr.sin_family = AF_INET;
//    serverAddr.sin_port = htons(target_port);
//    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
//
//    char* buf[1];
//
//    sendto(sockfd, buf, 1, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
//}
//
//void pss_listener::stop_thread() {
//    LOG("Stopping Listener thread");
//    this->running = false;
//
//    //sending special message to awake thread
//    send_special_msg(this->port);
//
//    //terminating ioService processing loop
//    this->ioService.stop();
//    //joining all threads of thread_loop
//    this->thread_pool.join_all();
//    LOG("JOINT ALL THREADS FROM POOL!!!")
//}