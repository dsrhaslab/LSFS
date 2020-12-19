//
// Created by danielsf97 on 1/27/20.
//

#include "load_balancer_listener.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <utility>
#include <df_client/client.h>

load_balancer_listener::load_balancer_listener(std::shared_ptr<load_balancer> lb, std::string ip):
    lb(std::move(lb)), socket_rcv(socket(AF_INET, SOCK_DGRAM, 0)), ip(ip)
{
    struct sockaddr_in si_me;

    memset(&si_me, '\0', sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(client::lb_port);
    si_me.sin_addr.s_addr = inet_addr(ip.c_str());

    bind(this->socket_rcv, (struct sockaddr*)&si_me, sizeof(si_me));
}

load_balancer_listener::~load_balancer_listener(){
    close(this->socket_rcv);
}

void load_balancer_listener::stop() {
    this->running = false;

    //send special message to awake main thread
    struct sockaddr_in serverAddr;
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&serverAddr, '\0', sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(client::lb_port);
    serverAddr.sin_addr.s_addr = inet_addr(this->ip.c_str());

    char* buf[1];

    sendto(sockfd, buf, 1, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    close(sockfd);
}

void load_balancer_listener::operator()(){
    this->running = true;
    char buf [1024];
    struct sockaddr_in si_other;
    socklen_t addr_size = sizeof(si_other);

    while(this->running){
        int bytes_rcv = recvfrom(this->socket_rcv, buf, 1024, 0, (struct sockaddr*)& si_other, &addr_size);

        if(this->running){
            try {
                proto::pss_message pss_message;
                pss_message.ParseFromArray(buf, bytes_rcv);
                this->lb->process_msg(pss_message);
            }
            catch(const char* e){
                std::cerr << e << std::endl;
            }
            catch(...){}
        }
    }
}

