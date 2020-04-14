//
// Created by danielsf97 on 10/8/19.
//

//
// Created by danielsf97 on 10/8/19.
//

#include <errno.h>
#include <string.h>
#include <string>
#include "tcp_client_server_connection.h"
#include <netinet/in.h>
//#include "df_serializer/capnp/packet.capnp.h"
#include <unistd.h>
#include <memory>
#include <utility>

#define LOG(X) std::cout << X << std::endl;

namespace tcp_client_server_connection{

    tcp_client_connection::~tcp_client_connection(){
        close(this->f_socket);
    }

    tcp_client_connection::tcp_client_connection(const char* peer_addr, int peer_port, std::shared_ptr<Serializer> serializer):
            f_peer_addr(peer_addr),
            f_peer_port(peer_port),
            serializer(serializer)
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
            std::cout << strerror(errno) << std::endl;
            throw "Cannot Connect";
        }

//        struct timeval timeout;
//        timeout.tv_sec = 1;
//        timeout.tv_usec = 0;
//
//        setsockopt (this->f_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
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

    void tcp_client_connection::recv_pss_msg(pss_message& pss_msg){
        this->serializer->recv_pss_message(&(this->f_socket), pss_msg);
    };
    void tcp_client_connection::send_pss_msg(pss_message& pss_msg){
        this->serializer->send_pss_message(&(this->f_socket), pss_msg);
    };
}
