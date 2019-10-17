//
// Created by danielsf97 on 10/8/19.
//

//
// Created by danielsf97 on 10/8/19.
//

#include <string>
#include "tcp_client_server_connection.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <capnp/serialize-packed.h>
#include <iostream>
#include "serializer/capnp/packet.capnp.h"
#include <unistd.h>
#include <memory>
#include <utility>
#include <bits/unordered_map.h>

#define LOG(X) std::cout << X << std::endl;

namespace tcp_client_server_connection{

    tcp_client_connection::~tcp_client_connection(){
        close(this->f_socket);
    }

    tcp_client_connection::tcp_client_connection(const char* peer_addr, int peer_port, std::unique_ptr<Serializer> serializer):
            f_peer_addr(peer_addr),
            f_peer_port(peer_port),
            serializer(std::move(serializer)) //changing ownership of the pointer
    {

        // Create a socket
        this->f_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (this->f_socket < 0) {
            perror("Cannot create a socket");
            exit(1);
        }

        // Fill in the address of server
        memset(&(this->f_peer_sockaddr_in), 0, sizeof(this->f_peer_sockaddr_in));

        struct hostent *host = gethostbyname(peer_addr);
        if (host == NULL) {
            perror("Cannot define host address");
            exit(1);
        }
        this->f_peer_sockaddr_in.sin_family = AF_INET;
        this->f_peer_sockaddr_in.sin_port = htons(peer_port);

        // Write resolved IP address of a server to the address structure
        memmove(&(this->f_peer_sockaddr_in.sin_addr.s_addr), host->h_addr_list[0], 4);

        // Connect to a remote server
        int res = connect(this->f_socket, (struct sockaddr *) &(this->f_peer_sockaddr_in),
                          sizeof(this->f_peer_sockaddr_in));
        if (res < 0) {
            throw "Cannot Connect";
        }
    }

    std::string tcp_client_connection::get_addr() const {
        return this->f_peer_addr;
    }

    int tcp_client_connection::get_port() const {
        return this->f_peer_port;
    }

    int tcp_client_connection::get_socket() const {
        return this->f_socket;
    }

//    void tcp_client_connection::send_packet(::capnp::MessageBuilder& message) {
//        try{
//            ::capnp::writePackedMessageToFd(this->f_socket,message);
//        }catch (kj::Exception e){
//            std::cerr << "Unable to satisfy request";
//        }
//    }
//
//    void tcp_client_connection::recv_view(std::unordered_map<int, int> &map) {
//        this->serializer->recv_view(map, &(this->f_socket));
//    }
//
//    void tcp_client_connection::send_identity(int port) {
//        this->serializer->send_identity(port, this->f_socket);
//    }
//
//    void tcp_client_connection::send_view(std::unordered_map<int, int> &view) {
//        return this->serializer->send_view(view, this->f_socket);
//    }

    void tcp_client_connection::recv_pss_msg(pss_message& pss_msg){
        this->serializer->recv_pss_message(&(this->f_socket), pss_msg);
    };
    void tcp_client_connection::send_pss_msg(pss_message& pss_msg){
        this->serializer->send_pss_message(&(this->f_socket), pss_msg);
    };
}
