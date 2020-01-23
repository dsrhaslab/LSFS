#include "udp_async_server.h"

void udp_session::handle_request(const boost::system::error_code& error)
{
    if (!error || error == boost::asio::error::message_size)
    {
        this->handler->handle_function(this->recv_buffer_, this->bytes_rcv);
    }
}

udp_async_server::udp_async_server(asio::io_service &io_service, int port, udp_handler* handler)
        : socket_(io_service, udp::endpoint(udp::v4(), port)),
          strand_(io_service), handler(handler)
{
    receive_session();
}

void udp_async_server::receive_session()
{
    // our session to hold the buffer + endpoint
    auto session = boost::make_shared<udp_session>(this, this->handler);

    socket_.async_receive_from(
            boost::asio::buffer(session->recv_buffer_),
            session->remote_endpoint_,
            strand_.wrap(
                    bind(&udp_async_server::handle_receive, this,
                         session, // keep-alive of buffer/endpoint
                         boost::asio::placeholders::error,
                         boost::asio::placeholders::bytes_transferred)));
}

void udp_async_server::handle_receive(shared_session session, const boost::system::error_code& ec, std::size_t bytes_rcv/*bytes_transferred*/) {
    // now, handle the current session on any available pool thread
    session->set_bytes_rcv(bytes_rcv);
    post(socket_.get_executor(),bind(&udp_session::handle_request, session, ec));

    // immediately accept new datagrams
    receive_session();
}