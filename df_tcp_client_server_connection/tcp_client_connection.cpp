//
// Created by danielsf97 on 10/8/19.
//

#include <errno.h>
#include <string.h>
#include <string>
#include "tcp_client_server_connection.h"
#include <netinet/in.h>
#include <unistd.h>
#include <memory>
#include <utility>
#include <exceptions/custom_exceptions.h>
#include <iostream>

#define LOG(X) std::cout << X << std::endl;

namespace tcp_client_server_connection{

    tcp_client_connection::~tcp_client_connection(){
        close(this->f_socket);
    }

    tcp_client_connection::tcp_client_connection(const char* peer_addr, int peer_port):
            f_peer_addr(peer_addr),
            f_peer_port(peer_port)
    {

        // Create a socket
        this->f_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (this->f_socket < 0) {
            throw "Error on Socket Creation";
        }

        // Fill in the address of server
        memset(&(this->f_peer_sockaddr_in), 0, sizeof(this->f_peer_sockaddr_in));

        struct hostent *host = gethostbyname(peer_addr);
        if (host == NULL) {
            throw "Error defining host address";
        }
        this->f_peer_sockaddr_in.sin_family = AF_INET;
        this->f_peer_sockaddr_in.sin_port = htons(peer_port);

        // Write resolved IP address of a server to the address structure
        memmove(&(this->f_peer_sockaddr_in.sin_addr.s_addr), host->h_addr_list[0], 4);

        // Connect to a remote server
        int res = connect(this->f_socket, (struct sockaddr *) &(this->f_peer_sockaddr_in),
                          sizeof(this->f_peer_sockaddr_in));
        if (res < 0) {
            std::cout << strerror(errno) << std::endl;
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

    int tcp_client_connection::recv_msg(char* buf){
        // Setting timeout for receiving data
        fd_set set;
        FD_ZERO(&set);
        FD_SET(this->f_socket, &set);

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int rv = select(this->f_socket + 1, &set, NULL, NULL, &timeout);
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
                int recv_msg_size = recv(this->f_socket, &msg_size_void[msg_size_sizeread], sizeof(uint16_t) - msg_size_sizeread, 0);
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

                int recv_size = recv(this->f_socket, &buf[total_size], msg_size - total_size, 0);

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

    void tcp_client_connection::wait_for_remote_end_to_close_socket(){
        shutdown(this->f_socket, SHUT_WR);
        uint16_t discard_bytes;
        bool closed_socket = false;
        while(!closed_socket) {
            int recv_bytes = recv(this->f_socket, &discard_bytes, sizeof(uint16_t), 0);
            if (recv_bytes <= 0) {
                closed_socket = true;
            }
        }
    }

    int tcp_client_connection::send_msg( char* buf, size_t size){
        uint16_t msg_size = size;
        int bytes_sent =send(this->f_socket, (char*) &msg_size, sizeof(uint16_t), 0);
        if(bytes_sent < 0){
            throw SocketSendException();
        }
        bytes_sent = send(this->f_socket, buf, size, 0);
        if(bytes_sent < 0){
            throw SocketSendException();
        }
        return bytes_sent;
    }
}
