//
// Created by danielsf97 on 10/8/19.
//

#include <string>
#include "tcp_client_server_connection.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <netinet/in.h>
#include <kv_message.pb.h>

#include <cstdio>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <exceptions/custom_exceptions.h>

#define LOG(X) std::cout << X << std::endl;

namespace tcp_client_server_connection{

    tcp_server_connection::~tcp_server_connection(){
        close(this->f_socket);
    }

    tcp_server_connection::tcp_server_connection(const char* host_addr, int host_port):
        f_addr(host_addr),
        f_port(host_port)
    {
        memset(&(this->f_sockaddr_in ), 0, sizeof(struct sockaddr_in));
        this->f_sockaddr_in.sin_family = AF_INET;
        this->f_sockaddr_in.sin_addr.s_addr = inet_addr(host_addr);
        this->f_sockaddr_in.sin_port = htons(host_port);

        //create socket
        this->f_socket = socket(AF_INET,SOCK_STREAM,0);
        if (this->f_socket < 0) {
            perror("Cannot create a socket");
            exit(1);
        }

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
            int msg_size_sizeread = 0;
            uint16_t msg_size;
            char* msg_size_void = (char*) &msg_size;
            while(msg_size_sizeread < sizeof(uint16_t)){
                int recv_msg_size = recv(*client_socket, &msg_size_void[msg_size_sizeread], sizeof(uint16_t) - msg_size_sizeread, 0);
                if (recv_msg_size == -1)
                {
                    throw SocketReadException();
                }
                else if (recv_msg_size == 0)
                {
                    throw PeerDisconnectedException();
                }
                else
                {
                    msg_size_sizeread += recv_msg_size;
                }
            } 
            int total_size = 0;
            while(total_size < msg_size){
                int recv_size = recv(*client_socket, &buf[total_size], msg_size - total_size, 0);

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
                    total_size += recv_size;
                }
            }

            return msg_size;
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

    void tcp_server_connection::wait_for_remote_end_to_close_socket(int* client_socket){
        shutdown(*client_socket, SHUT_WR);
        uint16_t discard_bytes;
        bool closed_socket = false;
        while(!closed_socket) {
            int recv_bytes = recv(*client_socket, &discard_bytes, sizeof(uint16_t), 0);
            if (recv_bytes <= 0) {
                closed_socket = true;
            }
        }
    }
}
