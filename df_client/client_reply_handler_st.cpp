//
// Created by danielsf97 on 1/27/20.
//

#include "client_reply_handler_st.h"


client_reply_handler_st::client_reply_handler_st(std::string ip, int kv_port, int pss_port, long wait_timeout):
    client_reply_handler(ip, kv_port, pss_port, wait_timeout),
    socket_rcv(socket(PF_INET, SOCK_DGRAM, 0))
{}

client_reply_handler_st::~client_reply_handler_st()
{
    close(this->socket_rcv);
}

void client_reply_handler_st::operator()() {
    this->running = true;

    struct sockaddr_in si_me, si_other;
    socklen_t addr_size = sizeof(si_other);

    memset(&si_me, '\0', sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(this->kv_port);
    si_me.sin_addr.s_addr = inet_addr(this->ip.c_str());

    bind(this->socket_rcv, (struct sockaddr*)&si_me, sizeof(si_me));
    char buf [65500];

    while(this->running){
        int bytes_rcv = recvfrom(this->socket_rcv, buf, 65500, 0, (struct sockaddr*)& si_other, &addr_size);

        if(this->running){
            try {
                proto::kv_message message;
                message.ParseFromArray(buf, bytes_rcv);
                if (message.has_get_reply_msg()) {

                    const proto::get_reply_message &msg = message.get_reply_msg();
                    process_get_reply_msg(msg);
                }else if (message.has_put_reply_msg()) {

                    const proto::put_reply_message &msg = message.put_reply_msg();
                    process_put_reply_msg(msg);
                }else if(message.has_get_latest_version_reply_msg()){

                    const proto::get_latest_version_reply_message& msg = message.get_latest_version_reply_msg();
                    process_get_latest_version_reply_msg(msg);
                }else if (message.has_delete_reply_msg()) {

                    const proto::delete_reply_message &msg = message.delete_reply_msg();
                    std::cout << "Received delete reply message" << std::endl;
                    process_delete_reply_msg(msg);
                }
            }
            catch(const char* e){
                std::cerr << e << std::endl;
            }
            catch(...){}
        }
    }
}

void client_reply_handler_st::stop() {
    this->running = false;

    //send special message to awake main thread
    struct sockaddr_in serverAddr;
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&serverAddr, '\0', sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(this->kv_port);
    serverAddr.sin_addr.s_addr = inet_addr(this->ip.c_str());

    char* buf[1];

    sendto(sockfd, buf, 1, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    close(sockfd);
}
