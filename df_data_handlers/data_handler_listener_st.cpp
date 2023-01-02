#include "data_handler_listener_st.h"

data_handler_listener_st::data_handler_listener_st(std::string ip, int kv_port, long id, float chance, pss *pss, group_construction* group_c, anti_entropy* anti_ent, std::shared_ptr<kv_store<std::string>> store, bool smart)
        : data_handler_listener(std::move(ip), kv_port, id, chance, pss, group_c, anti_ent, std::move(store), smart), socket_rcv(socket(AF_INET, SOCK_DGRAM, 0)){}

void data_handler_listener_st::operator()() {

    //wait for database to recover
    this->anti_ent_ptr->wait_while_recovering();

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
                proto::kv_message msg;
                msg.ParseFromArray(buf, bytes_rcv);

                if(msg.has_get_msg()){
                    this->process_get_message(msg);
                }else if(msg.has_get_latest_version_msg()){
                    this->process_get_latest_version_msg(msg);
                }else if(msg.has_put_msg()){
                    this->process_put_message(msg);
                }else if(msg.has_delete_msg()){
                    this->process_delete_message(msg);
                }else if(msg.has_put_child_msg()){
                    this->process_put_child_message(msg);
                }else if(msg.has_get_latest_met_stat_msg()){
                    this->process_get_latest_metadata_stat_msg(msg);
                }else if(msg.has_get_latest_met_size_msg()){
                    this->process_get_latest_metadata_size_msg(msg);
                }else if(msg.has_get_met_msg()){
                    this->process_get_metadata_message(msg);
                }else if(msg.has_anti_entropy_msg()){
                    this->process_anti_entropy_message(msg);
                }else if(msg.has_anti_entropy_get_msg()){
                    this->process_anti_entropy_get_message(msg);
                }else if(msg.has_anti_entropy_get_met_msg()){
                    this->process_anti_entropy_get_metadata_message(msg);
                }else if(msg.has_anti_entropy_get_reply_msg()){
                    this->process_anti_entropy_get_reply_message(msg);
                }else if(msg.has_anti_entropy_get_met_reply_msg()){
                    this->process_anti_entropy_get_metadata_reply_message(msg);
                }else if(msg.has_recover_request_msg()){
                    this->process_recover_request_msg(msg);
                }
            }
            catch(const char* e){
                std::cerr << e << std::endl;
            }
            catch(...){}
        }
    }
}

void data_handler_listener_st::stop_thread() {
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
