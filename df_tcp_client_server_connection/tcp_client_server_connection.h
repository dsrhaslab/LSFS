//
// Created by danielsf97 on 10/8/19.
//

#ifndef DATAFLASKSCPP_TCP_CLIENT_SERVER_CONNECTION_H
#define DATAFLASKSCPP_TCP_CLIENT_SERVER_CONNECTION_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdexcept>
#include <memory>
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

        tcp_client_connection(const char* addr, int port);
        ~tcp_client_connection();

        int                     get_socket() const;
        int                     get_port() const;
        std::string             get_addr() const;

        int recv_msg(char* buf);

        int send_msg(char* buf, size_t size);

        void wait_for_remote_end_to_close_socket();

    private:
        int                         f_socket;
        int                         f_peer_port;
        std::string                 f_peer_addr;
        struct sockaddr_in          f_peer_sockaddr_in;
    };


    class tcp_server_connection {

    private:
        int f_socket;
        int f_port;
        std::string f_addr;
        struct sockaddr_in f_sockaddr_in;

    public:
        tcp_server_connection(const char *host_addr, int host_port);

        ~tcp_server_connection();

        int get_socket() const;

        int get_port() const;

        std::string get_addr() const;

        int accept_connection(long timeout);

        int accept_connection();

        int recv_msg(int* client_socket, char* buf);

        int send_msg(int* client_socket, char* buf, size_t size);

        void wait_for_remote_end_to_close_socket(int* client_socket);
    };
}


#endif //DATAFLASKSCPP_TCP_CLIENT_SERVER_CONNECTION_H
