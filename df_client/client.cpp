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
#include "client_reply_handler_mt.h"
#include "../exceptions/custom_exceptions.h"

client::client(std::string ip, long id, int port, int lb_port, const char* conf_filename):
    ip(ip), id(id), port(port), sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
    , request_count(0)
{
    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    this->nr_puts_required = main_confs["nr_puts_required"].as<int>();
    this->nr_gets_required = main_confs["nr_gets_required"].as<int>();
    this->nr_gets_version_required = main_confs["nr_gets_version_required"].as<int>();
    this->max_timeouts = main_confs["max_nr_requests_timeouts"].as<int>();
    bool mt_client_handler = main_confs["mt_client_handler"].as<bool>();
    long wait_timeout = main_confs["client_wait_timeout"].as<long>();
    long lb_interval = main_confs["lb_interval"].as<long>();

    this->lb = std::make_shared<dynamic_load_balancer>(peer::boot_ip, peer::boot_port, ip, lb_port, lb_interval);
    this->lb_listener = std::make_shared<load_balancer_listener>(this->lb, ip, lb_port);

    this->lb_th = std::thread (std::ref(*this->lb));
    this->lb_listener_th = std::thread (std::ref(*this->lb_listener));

    if(mt_client_handler){
        int nr_workers = main_confs["nr_client_handler_ths"].as<int>();
        this->handler = std::make_shared<client_reply_handler_mt>(ip, port, wait_timeout, nr_workers);
    }else{
        this->handler = std::make_shared<client_reply_handler_st>(ip, port, wait_timeout);
    }
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

int client::send_get(peer_data &peer, std::string key, long* version, std::string req_id) {
    proto::kv_message msg;
    auto* message_content = new proto::get_message();
    message_content->set_ip(this->ip);
    message_content->set_port(this->port);
    message_content->set_id(this->id);
    message_content->set_key(key);
    if(version != nullptr){
        message_content->set_version(*version);
    }else{
        message_content->set_version_none(true);
    }
    message_content->set_reqid(req_id);
    msg.set_allocated_get_msg(message_content);

    return send_msg(peer, msg);
}

int client::send_get_latest_version(peer_data &peer, std::string key, std::string req_id) {
    proto::kv_message msg;
    auto* message_content = new proto::get_latest_version_message();
    message_content->set_ip(this->ip);
    message_content->set_port(this->port);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_reqid(req_id);
    msg.set_allocated_get_latest_version_msg(message_content);

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

std::set<long> client::put(std::string key, long version, const char *data, size_t size, int wait_for) {
   this->handler->register_put(key, version); // throw const char* (Escritas concorrentes sobre a mesma chave)
   kv_store_key<std::string> comp_key = {key, version};
   std::unique_ptr<std::set<long>> res = nullptr;
   int curr_timeouts = 0;
   while(res == nullptr && curr_timeouts < this->max_timeouts){
       peer_data peer = this->lb->get_random_peer(); //throw exception
       int status = this->send_put(peer, key, version, data, size);
       if(status == 0){
           std::cout << "PUT (TO " << peer.id << ") " << key << " : " << version << " ==============================>" << std::endl;
           try{
               res = this->handler->wait_for_put(comp_key, wait_for);
           }catch(TimeoutException& e){
               curr_timeouts++;
           }
       }
   }

   if(res == nullptr){
       throw TimeoutException();
   }

   return *res;
}

std::shared_ptr<std::string> client::get(std::string key, int wait_for, long* version_ptr) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str = std::to_string(this->id) +":" + this->ip + ":" + std::to_string(this->port) + ":" + std::to_string(req_id);
    this->handler->register_get(req_id_str);
    std::shared_ptr<std::string> res (nullptr);

    int curr_timeouts = 0;
    while(res == nullptr && curr_timeouts < this->max_timeouts){
        peer_data peer = this->lb->get_random_peer(); //throw exception (empty view)
        int status = this->send_get(peer, key, version_ptr, req_id_str);
        if (status == 0) {
            std::cout << "GET " << req_id  << " " << key << (version_ptr == nullptr ? ": ?" : ": " + *version_ptr) << " ==================================>" << std::endl;
            try{
                res = this->handler->wait_for_get(req_id_str, wait_for);
            }catch(TimeoutException& e){
                curr_timeouts++;
            }
        }
    }

    if(res == nullptr){
        throw TimeoutException();
    }

    return res;
}

long client::get_latest_version(std::string key, int wait_for) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str = std::to_string(this->id) +":" + this->ip + ":" + std::to_string(this->port) + ":" + std::to_string(req_id);
    this->handler->register_get_latest_version(req_id_str);
    std::unique_ptr<long> res (nullptr);

    int curr_timeouts = 0;
    while(res == nullptr && curr_timeouts < this->max_timeouts){
        peer_data peer = this->lb->get_random_peer(); //throw exception (empty view)
        int status = this->send_get_latest_version(peer, key, req_id_str);
        if (status == 0) {
            std::cout << "GET Version " << req_id << " Key:" << key << " ==================================>" << std::endl;
            try{
                res = this->handler->wait_for_get_latest_version(req_id_str, wait_for);
            }catch(TimeoutException& e){
                curr_timeouts++;
            }
        }
    }

    if(res == nullptr){
        throw TimeoutException();
    }

    std::cout << "######################" << key << " VERSIONR: " << *res<< "#################################" << std::endl;

    return *res;
}





