//
// Created by danielsf97 on 10/8/19.
//

#include <string>
#include "tcp_client_server_connection.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <capnp/serialize-packed.h>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <netinet/in.h>
#include <build/kv_message.pb.h>

#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <exceptions/custom_exceptions.h>

#define LOG(X) std::cout << X << std::endl;

namespace tcp_client_server_connection{

    tcp_server_connection::~tcp_server_connection(){
//        std::cerr << "[tcp_server_connection] function: destructor [Closing] server socket -> " + std::to_string(this->f_socket) << std::endl;
        close(this->f_socket);
    }

    tcp_server_connection::tcp_server_connection(const char* host_addr, int host_port, std::unique_ptr<Serializer> serializer):
        f_addr(host_addr),
        f_port(host_port),
        serializer(std::move(serializer))
    {
        memset(&(this->f_sockaddr_in ), 0, sizeof(struct sockaddr_in));
        this->f_sockaddr_in.sin_family = AF_INET;
        this->f_sockaddr_in.sin_addr.s_addr = inet_addr(host_addr); //htonl(INADDR_ANY);
        this->f_sockaddr_in.sin_port = htons(host_port);

        //create socket
        this->f_socket = socket(AF_INET,SOCK_STREAM,0);
        if (this->f_socket < 0) {
            perror("Cannot create a socket");
            exit(1);
        }

//        std::cerr << "[tcp_server_connection] function: constructor [Opening] server socket -> " + std::to_string(this->f_socket) << std::endl;

        //bind
        int res = bind(this->f_socket, (struct sockaddr*) &this->f_sockaddr_in, sizeof(this->f_sockaddr_in));
        if (res < 0) {
            perror("Cannot bind a socket"); exit(1);
        }

        // closes listen socket at program termination
        struct linger linger_opt = { 1, 0}; // Linger active, timeout 0
        setsockopt(this->f_socket, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));

        //mark socket for listening SOMAXCONN -> número de conexões máximo
        res = listen(this->f_socket, SOMAXCONN);
        if (res < 0) {
            perror("Cannot listen"); exit(1);
        }
    }

    int tcp_server_connection::accept_connection(long timeout) {
        struct timeval tv;
        fd_set readfds;

        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(this->f_socket, &readfds);

        sockaddr_in client;
        socklen_t client_len =  sizeof(client);
        int clientSocket;

        if (select(this->f_socket+1, &readfds, nullptr, nullptr, &tv) > 0) {
            clientSocket = accept(this->f_socket, (sockaddr *) &client, &client_len);

            if (clientSocket < 0) {
                perror("Cannot accept");
            }else{
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET,&(client.sin_addr), str,INET_ADDRSTRLEN);

                return clientSocket;
            }
        }
        return -1;
    }

    int tcp_server_connection::accept_connection() {
        sockaddr_in client;
        socklen_t client_len =  sizeof(client);
        int clientSocket;
        do {

            clientSocket = accept(this->f_socket, (sockaddr *) &client, &client_len);

            if (clientSocket < 0) {
                perror("Cannot accept");
            }

        }
        while(clientSocket < 0);

        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&(client.sin_addr), str,INET_ADDRSTRLEN);

        return clientSocket;
    }

    std::string tcp_server_connection::get_addr() const {
        return this->f_addr;
    }

    int tcp_server_connection::get_port() const {
        return this->f_port;
    }

    int tcp_server_connection::get_socket() const {
        return this->f_socket;
    }

    int tcp_server_connection::recv_msg(int* client_socket, char* buf){
        // Setting timeout for receiving data
        fd_set set;
        FD_ZERO(&set);
        FD_SET(*client_socket, &set);

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int rv = select(*client_socket + 1, &set, NULL, NULL, &timeout);
        if (rv == -1)
        {
            throw SocketException();
        }
        else if (rv == 0)
        {
            throw TimeoutException();
        }
        else
        {
            // socket has something to read
            uint16_t msg_size;
            recv(*client_socket, &msg_size, sizeof(uint16_t), 0);
            int recv_size = recv(*client_socket, buf, msg_size, 0);

            if (recv_size == -1)
            {
                throw SocketReadException();
            }
            else if (recv_size == 0)
            {
                throw PeerDisconnectedException();
            }
            else
            {
                return recv_size;
            }
        }
    }

    int tcp_server_connection::send_msg(int* client_socket, char* buf, size_t size){
        uint16_t msg_size = size;
        int bytes_sent = send(*client_socket, (char*) &msg_size, sizeof(uint16_t), 0);
        if(bytes_sent < 0){
            throw SocketSendException();
        }
        bytes_sent = send(*client_socket, buf, size, 0);
        if(bytes_sent < 0){
            throw SocketSendException();
        }
        return bytes_sent;
    }

    void tcp_server_connection::recv_pss_msg(int* client_socket, pss_message& pss_msg){
        this->serializer->recv_pss_message(client_socket, pss_msg);
    };
    void tcp_server_connection::send_pss_msg(int* client_socket, pss_message& pss_msg){
        this->serializer->send_pss_message(client_socket, pss_msg);
    }

}
