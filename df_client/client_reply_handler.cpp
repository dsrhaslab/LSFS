//
// Created by danielsf97 on 1/27/20.
//

#include "client_reply_handler.h"
#include "../kv_message.pb.h"
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>

client_reply_handler::client_reply_handler(std::string ip, int port, int nr_puts_required, long wait_timeout):
    ip(ip), port(port), nr_puts_required(nr_puts_required), wait_timeout(wait_timeout), socket_rcv(socket(PF_INET, SOCK_DGRAM, 0))
{}

client_reply_handler::~client_reply_handler()
{
    close(this->socket_rcv);
}

void client_reply_handler::operator()() {
    this->running = true;
    std::cout << "Client Reply Handler is Active!!" << std::endl;

    struct sockaddr_in si_me, si_other;
    socklen_t addr_size = sizeof(si_other);

    memset(&si_me, '\0', sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(this->port);
    si_me.sin_addr.s_addr = inet_addr(this->ip.c_str());

    bind(this->socket_rcv, (struct sockaddr*)&si_me, sizeof(si_me));
    char buf [65500];

    while(this->running){
        int bytes_rcv = recvfrom(this->socket_rcv, buf, 65500, 0, (struct sockaddr*)& si_other, &addr_size);
        std::cout << "Received NEW KV Message" << std::endl;

        if(this->running){
            try {
                proto::kv_message message;
                message.ParseFromArray(buf, bytes_rcv);
                if(message.has_get_reply_msg()){
                    std::cout << "IS A GET REPLY MESSAGE" << std::endl;

                    const proto::get_reply_message& msg = message.get_reply_msg();
                    std::string req_id = msg.reqid();
                    long replier_id = msg.id();
                    const char* data = msg.data().c_str();

                    std::unique_lock<std::mutex> lock(this->get_mutex);

                    if(this->get_replies.find(req_id) != this->get_replies.end()){
                        // a chave existe
                        auto data_size = strlen(data);
                        char *buffer = new char[data_size + 1];
                        strncpy(buffer, data, data_size + 1); //buffer[len] = 0 std::make_shared<const char*>(buffer)
                        this->get_replies.insert_or_assign(req_id, std::shared_ptr<const char[]>(buffer, [](const char* p){delete[] p;}));
                        this->get_cond_var.notify_all();
                    }else{
                        std::cout << "GET REPLY IGNORED - NON EXISTENT KEY" << std::endl;
                    }
                }else if(message.has_put_reply_msg()){
                    std::cout << "IS A PUT REPLY MESSAGE" << std::endl;

                    const proto::put_reply_message& msg = message.put_reply_msg();
                    long key = msg.key();
                    long replier_id = msg.id();

                    std::unique_lock<std::mutex> lock(this->put_mutex);

                    std::cout << "FINDING KEY " << key << std::endl;

                    auto it = this->put_replies.find(key);
                    if(it != this->put_replies.end()){
                        // a chave existe
                        std::cout << "EXISTENT KEY " << key << std::endl;
                        it->second.emplace(replier_id);
                        this->put_cond_var.notify_all();
                    }else{
                        std::cout << "PUT REPLY IGNORED - NON EXISTENT KEY" << std::endl;
                    }
                }
            }
            catch(const char* e){
                std::cerr << e << std::endl;
            }
            catch(...){}
        }
    }
}

void client_reply_handler::register_put(long key) {
    std::unique_lock<std::mutex> lock(this->put_mutex);
    auto it = this->put_replies.find(key);
    if(it == this->put_replies.end()){
        // a chave não existe
        std::set<long> temp;
        put_replies.emplace(key,std::move(temp));
    }
}

std::unique_ptr<std::set<long>> client_reply_handler::wait_for_put(long key){
    std::unique_ptr<std::set<long>> res = nullptr;

    std::unique_lock<std::mutex> lock(this->put_mutex);
    this->put_cond_var.wait_for(lock, std::chrono::seconds(this->wait_timeout));

    auto it = this->put_replies.find(key);
    if(it != this->put_replies.end()){
        //se existe entrada para a chave
        std::cout << "Checking if size of list match nr_puts_required" << std::endl;
        if(it->second.size() >= this->nr_puts_required){
            std::cout << "Check -> Erasing ###########################" << std::endl;
            res = std::make_unique<std::set<long>>(it->second);
            this->put_replies.erase(it);
        }
    }
    return res;
}

void client_reply_handler::register_get(std::string req_id) {
    std::unique_lock<std::mutex> lock(this->get_mutex);
    auto it = this->get_replies.find(req_id);
    if(it == this->get_replies.end()){
        // a chave não existe
        char *buffer = new char[1];
        buffer[0] = '\0';
        get_replies.emplace(req_id, std::shared_ptr<const char[]>(buffer, [](const char* p){delete[] p;}));
    }
}

std::shared_ptr<const char []> client_reply_handler::wait_for_get(std::string req_id) {
    std::shared_ptr<const char[]> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_mutex);
    this->get_cond_var.wait_for(lock, std::chrono::seconds(this->wait_timeout));

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()){
        //se existe entrada para a chave
        if(strcmp(it->second.get(), "\0") != 0){
            res = it->second;
            this->get_replies.erase(it);
        }
    }
    return res;
}

void client_reply_handler::stop() {
    this->running = false;

    //send special message to awake main thread
    struct sockaddr_in serverAddr;
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&serverAddr, '\0', sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(this->port);
    serverAddr.sin_addr.s_addr = inet_addr(this->ip.c_str());

    char* buf[1];

    sendto(sockfd, buf, 1, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    close(sockfd);
}