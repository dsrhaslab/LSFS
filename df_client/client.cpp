//
// Created by danielsf97 on 1/27/20.
//

#include "client.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <memory>
#include "yaml-cpp/yaml.h"
#include "../df_core/peer.h"

client::client(std::string ip, long id, int port, int lb_port):
    ip(ip), id(id), port(port), sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
    , request_count(0)
{
    YAML::Node config = YAML::LoadFile("scripts/conf.yaml");
    auto main_confs = config["main_confs"];
    this->nr_puts_required = main_confs["nr_puts_required"].as<int>();
    std::cout << 1 << std::endl;
    long wait_timeout = main_confs["client_wait_timeout"].as<long>();
    std::cout << 2 << std::endl;
    long lb_interval = main_confs["lb_interval"].as<long>();
    std::cout << 3 << std::endl;

    this->lb = std::make_shared<dynamic_load_balancer>(peer::boot_ip, peer::boot_port, ip, lb_port, lb_interval);
    this->lb_listener = std::make_shared<load_balancer_listener>(this->lb, ip, lb_port);

    this->lb_th = std::thread (std::ref(*this->lb));
    this->lb_listener_th = std::thread (std::ref(*this->lb_listener));

    this->handler = std::make_shared<client_reply_handler>(ip, port, nr_puts_required, wait_timeout);
    this->handler_th = std::thread (std::ref(*this->handler));
}

void client::stop(){
    lb->stop();
    std::cout << "stopped load balancer" << std::endl;
    lb_listener->stop();
    std::cout << "stopped load balancer listener" << std::endl;
    lb_th.join();
    std::cout << "stopped load balancer thread" << std::endl;
    lb_listener_th.join();
    std::cout << "stopped load balancer listener thread" << std::endl;
    close(sender_socket);
    handler->stop();
    handler_th.join();
    std::cout << "stopped df_client" << std::endl;
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

        std::cout << "Size of buffer: " << buf.size() << std::endl;
        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));}
        else{ return 0; }
    }catch(...){std::cout <<"=============================== NÂO consegui enviar =================" << std::endl;}

    return 1;
}

int client::send_get(peer_data &peer, std::string key, long version, std::string req_id) {
    proto::kv_message msg;
    auto* message_content = new proto::get_message();
    message_content->set_ip(this->ip);
    message_content->set_port(this->port);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_version(version);
    message_content->set_reqid(req_id);
    msg.set_allocated_get_msg(message_content);

    std::cout << "Sending get " << key << "to " << std::to_string(peer.port) << " " << peer.pos ;

    return send_msg(peer, msg);
}

int client::send_put(peer_data &peer, std::string key, long version, const char *data, size_t size) {
    proto::kv_message msg;
    auto* message_content = new proto::put_message();
    message_content->set_ip(this->ip);
    message_content->set_port(this->port);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_version(version);
    message_content->set_data(data, size);
    msg.set_allocated_put_msg(message_content);

    return send_msg(peer, msg);
}

std::set<long> client::put(std::string key, long version, const char *data, size_t size) {
   this->handler->register_put(key);
   std::unique_ptr<std::set<long>> res = nullptr;
   while(res == nullptr || res->size() < this->nr_puts_required){
       std::cout << "WAITING FOR PUTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT" << std::endl;
       peer_data peer = this->lb->get_random_peer(); //throw exception
       int status = this->send_put(peer, key, version, data, size);
       if(status == 0){
           res = this->handler->wait_for_put(key);
//           std::cout << "SIZE: " << res->size() << std::endl;
       }
   }
   return *res;
}

std::shared_ptr<const char []> client::get(long node_id, std::string key, long version) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str = std::to_string(this->id) +":" + this->ip + ":" + std::to_string(this->port) + ":" + std::to_string(req_id);
    this->handler->register_get(req_id_str);
    std::shared_ptr<const char []> res (nullptr);
    while(res == nullptr){
        peer_data peer = this->lb->get_random_peer(); //throw exception (empty view)
        int status = this->send_get(peer, key, version, req_id_str);
        if(status == 0){
            res = this->handler->wait_for_get(req_id_str);
        }
    }
    return res;
}




