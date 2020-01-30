//
// Created by danielsf97 on 1/27/20.
//

#include "client.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <kv_message.pb.h>

client::client(std::string ip, long id, int port, load_balancer* lb, int nr_puts_required, long wait_timeout):
    ip(ip), id(id), port(port), nr_puts_required(nr_puts_required), sender_socket(socket(PF_INET, SOCK_DGRAM, 0)),
    lb(lb), request_count(0), handler(client_reply_handler(ip, port, nr_puts_required, wait_timeout)), handler_th(std::thread (std::ref(this->handler)))
{}

void client::stop(){
    close(sender_socket);
    handler.stop();
    handler_th.join();
}

long client::inc_and_get_request_count() {
    //TODO apesar de ser atomic precisa de um lock aqui
    this->request_count += 1;
    return this->request_count;
}

int client::send_msg(peer_data& target_peer, proto::kv_message& msg){
    try {

        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_peer.port + 1);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        std::string buf;
        msg.SerializeToString(&buf);

        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));}
        else{ return 0; }
    }catch(...){std::cout <<"=============================== NÂO consegui enviar =================" << std::endl;}

    return 1;
}

int client::send_get(peer_data &peer, long key, long version, std::string req_id) {
    proto::kv_message msg;
    auto* message_content = new proto::get_message();
    message_content->set_ip(this->ip);
    message_content->set_port(this->port);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_version(version);
    message_content->set_reqid(req_id);
    msg.set_allocated_get_msg(message_content);

    return send_msg(peer, msg);
}

int client::send_put(peer_data &peer, long key, long version, const char *data) {
    proto::kv_message msg;
    auto* message_content = new proto::put_message();
    message_content->set_ip(this->ip);
    message_content->set_port(this->port);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_version(version);
    message_content->set_data(data);
    msg.set_allocated_put_msg(message_content);

    return send_msg(peer, msg);
}

std::set<long> client::put(long key, long version, const char *data) {
   this->handler.register_put(key);
   std::unique_ptr<std::set<long>> res = nullptr;
   while(res == nullptr || res->size() < this->nr_puts_required){
       peer_data peer = this->lb->get_random_peer();
       std::cout << peer.port << std::endl;
       int status = this->send_put(peer, key, version, data);
       if(status == 0){
           std::cout << "Put sucessfully sent!!" << std::endl;
           res = this->handler.wait_for_put(key);
           if(res != nullptr) std::cout << res->size() << std::endl;
       }
   }
   return *res;
}

std::shared_ptr<const char []> client::get(long node_id, long key, long version) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str = std::to_string(this->id) +":" + this->ip + ":" + std::to_string(this->port) + ":" + std::to_string(req_id);
    this->handler.register_get(req_id_str);
    std::shared_ptr<const char []> res (nullptr);
    while(res == nullptr){
        peer_data peer = this->lb->get_random_peer();
        int status = this->send_get(peer, key, version, req_id_str);
        if(status == 0){
            res = this->handler.wait_for_get(req_id_str);
        }
    }
    return res;
}




