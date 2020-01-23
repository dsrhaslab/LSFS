//
// Created by danielsf97 on 1/16/20.
//

#ifndef P2PFS_UDP_ASYNC_SERVER_H
#define P2PFS_UDP_ASYNC_SERVER_H
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include "../core/peer.h"
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


class udp_handler{
public:
    virtual void handle_function(const char* data, size_t size) = 0;
};

class udp_async_server; // forward declaration

struct udp_session : enable_shared_from_this<udp_session> {

    udp_session(udp_async_server* server, udp_handler* handler) : server_(server), handler(handler) {}

    void handle_request(const boost::system::error_code& error);

    udp::endpoint remote_endpoint_;
    enum { max_length = 1024 };
    char recv_buffer_ [max_length];
    std::size_t bytes_rcv;
    udp_async_server* server_;
    udp_handler* handler;

public:
    void set_bytes_rcv(size_t bytes_rcv){
        this->bytes_rcv = bytes_rcv;
    }
};

class udp_async_server
{
    typedef boost::shared_ptr<udp_session> shared_session;

public:
    udp_async_server(asio::io_service &io_service, int port, udp_handler* handler);

private:
    void receive_session();
    void handle_receive(shared_session session, const boost::system::error_code& ec, std::size_t bytes_rcv/*bytes_transferred*/);

private:
    udp::socket  socket_;
    boost::asio::io_service::strand strand_;
    udp_handler* handler;

    friend struct udp_session;
};

#endif //P2PFS_UDP_ASYNC_SERVER_H
