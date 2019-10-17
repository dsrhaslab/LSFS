//
// Created by danielsf97 on 10/8/19.
//

#ifndef DATAFLASKSCPP_TCP_CLIENT_SERVER_CONNECTION_H
#define DATAFLASKSCPP_TCP_CLIENT_SERVER_CONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdexcept>
#include "../serializer/serializer.h"
#include <memory>
#include "serializer/capnp/packet.capnp.h"
#include <unordered_map>
#include <vector>

namespace tcp_client_server_connection
{

    class tcp_client_server_runtime_error : public std::runtime_error
    {
    public:
        tcp_client_server_runtime_error(const char *w) : std::runtime_error(w) {}
    };


    class tcp_client_connection
    {
    public:

        tcp_client_connection(const char* addr, int port, std::unique_ptr<Serializer> serializer);
        ~tcp_client_connection();

        int                     get_socket() const;
        int                     get_port() const;
        std::string             get_addr() const;

//        void                                    send_packet(::capnp::MessageBuilder& message);
//        std::unique_ptr<packet::Packet::Reader> recv_packet();
//        void recv_view(std::unordered_map<int, int> &map);
//        void send_identity(int port);
//        void send_view( std::unordered_map<int, int>& view);
        void recv_pss_msg(pss_message &pss_msg);

        void send_pss_msg(pss_message &pss_msg);

    private:
        int                         f_socket;
        int                         f_peer_port;
        std::string                 f_peer_addr;
        struct sockaddr_in          f_peer_sockaddr_in;
        std::unique_ptr<Serializer> serializer;
    };


    class tcp_server_connection {

    private:
        int f_socket;
        int f_port;
        std::string f_addr;
        struct sockaddr_in f_sockaddr_in;
        std::unique_ptr<Serializer> serializer;

    public:
        tcp_server_connection(const char *host_addr, int host_port, std::unique_ptr<Serializer> serializer);

        ~tcp_server_connection();

        int get_socket() const;

        int get_port() const;

        std::string get_addr() const;

        int accept_connection();

//        void send_view( std::unordered_map<int, int>& view ,int client_socket);
//
//        int recv_identity(int client_socket);

        void recv_pss_msg(int *client_socket, pss_message &pss_msg);

        void send_pss_msg(int *client_socket, pss_message &pss_msg);

//        void close_socket();
    };
}


#endif //DATAFLASKSCPP_TCP_CLIENT_SERVER_CONNECTION_H
