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
#include <df_loadbalancing/smart_load_balancer.h>
#include "yaml-cpp/yaml.h"
#include "df_core/peer.h"
#include "client_reply_handler_mt.h"
#include "exceptions/custom_exceptions.h"

client::client(std::string boot_ip, std::string ip, long id/*, int port, int lb_port*/, std::string conf_filename):
    ip(ip), id(id)/*, port(port)*/, sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
    , request_count(0)
{
    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    this->nr_puts_required = main_confs["nr_puts_required"].as<int>();
    this->nr_gets_required = main_confs["nr_gets_required"].as<int>();
    this->nr_gets_version_required = main_confs["nr_gets_version_required"].as<int>();
    this->max_timeouts = main_confs["max_nr_requests_timeouts"].as<int>();
    bool mt_client_handler = main_confs["mt_client_handler"].as<bool>();
    this->wait_timeout = main_confs["client_wait_timeout"].as<long>();
    long lb_interval = main_confs["lb_interval"].as<long>();

    this->lb = std::make_shared<dynamic_load_balancer>(boot_ip/*, client::lb_port*/, ip/*, lb_port*/, lb_interval);
    this->lb_listener = std::make_shared<load_balancer_listener>(this->lb, ip/*, lb_port*/);

    this->lb_th = std::thread (std::ref(*this->lb));
    this->lb_listener_th = std::thread (std::ref(*this->lb_listener));

    if(mt_client_handler){
        int nr_workers = main_confs["nr_client_handler_ths"].as<int>();
        this->handler = std::make_shared<client_reply_handler_mt>(ip/*, port*/, this->wait_timeout, nr_workers);
    }else{
        this->handler = std::make_shared<client_reply_handler_st>(ip/*, port*/, this->wait_timeout);
    }
    this->handler_th = std::thread (std::ref(*this->handler));
}

void client::stop(){
    lb->stop();
    lb_listener->stop();
    lb_th.join();
    lb_listener_th.join();
    close(sender_socket);
    handler->stop();
    handler_th.join();
}

long client::inc_and_get_request_count() {
    return this->request_count++;
}

int client::send_msg(peer_data& target_peer, proto::kv_message& msg){
    try {

        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(client::kv_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        std::string buf;
        msg.SerializeToString(&buf);
//        std::cout << "BUFFER SIZE: " << buf.size() << std::endl;
//        spdlog::debug("Client sent msg with size of buffer: " + std::to_string(buf.size()));
        std::unique_lock<std::mutex> lock(this->sender_socket_mutex);
        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        lock.unlock();

        if(res == -1){spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));}
        else{ return 0; }
    }catch(...){spdlog::error("=============================== NÂO consegui enviar =================");}

    return 1;
}

int client::send_get(peer_data &peer, const std::string& key, long* version, const std::string& req_id) {
    std::cout << "Send Get [" << req_id << "] " << key << " -> " << peer.id << std::endl;
    proto::kv_message msg;
    auto* message_content = new proto::get_message();
    message_content->set_ip(this->ip);
    message_content->set_id(this->id);
    message_content->set_key(key);
    if(version != nullptr){
        message_content->set_version(*version);
    }else{
        message_content->set_version_none(true);
    }
    message_content->set_version_client_id(-1);
    message_content->set_reqid(req_id);
    msg.set_allocated_get_msg(message_content);

    return send_msg(peer, msg);
}

int client::send_get_latest_version(peer_data &peer, const std::string& key, const std::string& req_id) {
    std::cout << "Send Get Latest Version [" << req_id << "] " << key << " -> " << peer.id << std::endl;
    proto::kv_message msg;
    auto* message_content = new proto::get_latest_version_message();
    message_content->set_ip(this->ip);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_reqid(req_id);
    msg.set_allocated_get_latest_version_msg(message_content);

    return send_msg(peer, msg);
}

int client::send_put(peer_data &peer, const std::string& key, long version, const char *data, size_t size) {
    std::cout << "Send Put " << key << ":" << version << " -> " << peer.id << std::endl;
    proto::kv_message msg;
    auto* message_content = new proto::put_message();
    message_content->set_ip(this->ip);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_version(version);
    message_content->set_data(data, size);
    msg.set_allocated_put_msg(message_content);

    return send_msg(peer, msg);
}

int client::send_put_with_merge(peer_data &peer, const std::string& key, long version, const char *data, size_t size) {
    std::cout << "Send Put Merge" << key << ":" << version << " -> " << peer.id << std::endl;
    proto::kv_message msg;
    auto* message_content = new proto::put_with_merge_message();
    message_content->set_ip(this->ip);
    message_content->set_id(this->id);
    message_content->set_key(key);
    message_content->set_version(version);
    message_content->set_data(data, size);
    msg.set_allocated_put_with_merge_msg(message_content);

    return send_msg(peer, msg);
}


void client::put(const std::string& key, long version, const char *data, size_t size, int wait_for) {
    this->handler->register_put(key, version); // throw const char* (Escritas concorrentes sobre a mesma chave)
    kv_store_key<std::string> comp_key = {key, kv_store_key_version(version)};
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        peer_data peer = this->lb->get_peer(key); //throw exception
        int status = this->send_put(peer, key, version, data, size);
        if(status == 0){
            try{
                //while(res == nullptr){
                succeed = this->handler->wait_for_put(comp_key, wait_for);
                //}
            }catch(TimeoutException& e){
                curr_timeouts++;
            }
        }
    }

    if(!succeed){
        throw TimeoutException();
    }
}

void client::put_batch(const std::vector<std::string> &keys, const std::vector<long> &versions,
                       const std::vector<const char *> &datas, const std::vector<size_t> &sizes, int wait_for) {

    long nr_writes = keys.size();
    std::vector<bool> completed(nr_writes, false);

    //register all the keys and send first put
    for(size_t i = 0; i < keys.size(); i++){
        this->handler->register_put(keys[i], versions[i]); // throw const char* (Escritas concorrentes sobre a mesma chave)
        peer_data peer = this->lb->get_peer(keys[i]); //throw exception
        this->send_put(peer, keys[i], versions[i], datas[i], sizes[i]);
    }

    long remain_timeouts = nr_writes * this->max_timeouts;
    bool all_completed = false;

    do{
        auto wait_until = std::chrono::system_clock::now() + std::chrono::seconds(this->wait_timeout);
        for(size_t i = 0; i < completed.size(); i++){
            if(!completed[i]){
                try{
                    kv_store_key<std::string> comp_key = {keys[i], kv_store_key_version(versions[i])};
                    completed[i] = this->handler->wait_for_put_until(comp_key, wait_for, wait_until);
                    // if completed i can at least decrease a timeout that belongs to this key
                    remain_timeouts--;
                }catch(TimeoutException& e){
                    std::cout << "Timeout" << std::endl;
                    peer_data peer = this->lb->get_peer(keys[i]); //throw exception
                    this->send_put(peer, keys[i], versions[i], datas[i], sizes[i]);
                    remain_timeouts--;
                }
            }
        }
        all_completed = std::find(completed.begin(), completed.end(), false) == completed.end();
    }while( !all_completed && remain_timeouts > 0); // do while there are incompleted puts

    // clear not succeded keys from put maps
    std::vector<kv_store_key<std::string>> erasing_keys;
    for(size_t i = 0; i < completed.size(); i++){
        if(!completed[i]){
            erasing_keys.push_back({keys[i], kv_store_key_version(versions[i])});
        }
    }
    this->handler->clear_put_keys_entries(erasing_keys);

    if(!all_completed){
        // Nr of timeouts exceeded
        throw TimeoutException();
    }
}

void client::put_with_merge(const std::string& key, long version, const char *data, size_t size, int wait_for) {
    this->handler->register_put(key, version); // throw const char* (Escritas concorrentes sobre a mesma chave)
    kv_store_key<std::string> comp_key = {key, kv_store_key_version(version)};
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        peer_data peer = this->lb->get_peer(key); //throw exception
        int status = this->send_put_with_merge(peer, key, version, data, size);
        if(status == 0){
//            spdlog::debug("PUT (TO " + std::to_string(peer.id) + ") " + key + " : " + std::to_string(version) + " ==============================>");
            try{
                succeed = this->handler->wait_for_put(comp_key, wait_for);
            }catch(TimeoutException& e){
                curr_timeouts++;
            }
        }
    }

    if(!succeed){
        throw TimeoutException();
    }
}

void client::get_batch(const std::vector<std::string> &keys, std::vector<std::shared_ptr<std::string>> &data_strs, int wait_for) {

    long nr_reads = keys.size();
    std::vector<std::string> req_ids(nr_reads);

    //register all the keys and send first get
    for(size_t i = 0; i < keys.size(); i++){
        long req_id = this->inc_and_get_request_count();
        req_ids[i].reserve(100);
        req_ids[i].append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
        this->handler->register_get(req_ids[i]);
        peer_data peer = this->lb->get_peer(keys[i]); //throw exception (empty view)
        this->send_get(peer, keys[i], nullptr, req_ids[i]);
    }

    long remain_timeouts = nr_reads * this->max_timeouts;
    bool all_completed = false;

    do{
        auto wait_until = std::chrono::system_clock::now() + std::chrono::seconds(this->wait_timeout);
        for(size_t i = 0; i < data_strs.size(); i++){
            if(data_strs[i] == nullptr){
                try{
                    data_strs[i] = this->handler->wait_for_get_until(req_ids[i], wait_for, wait_until);
                    // if completed i can at least decrease a timeout that belongs to this key
                    remain_timeouts--;
                }catch(TimeoutException& e){
                    std::cout << "Timeout key: " << keys[i] << std::endl;
                    peer_data peer = this->lb->get_peer(keys[i]); //throw exception (empty view)
                    this->send_get(peer, keys[i], nullptr, req_ids[i]);
                    remain_timeouts--;
                }
            }
        }
        all_completed = std::find(data_strs.begin(), data_strs.end(), nullptr) == data_strs.end();
    }while( !all_completed && remain_timeouts > 0); // do while there are incompleted puts

    // clear not succeded keys from put maps
    auto it_req_ids = req_ids.begin();
    for(size_t i = 0; i < data_strs.size(); i++){
        if(data_strs[i] != nullptr){
            // se a leitura do bloco foi completa, remove a entrada dos req_ids
            // uma vez que não vai ser necessário remover essa chave dos mapas dos gets
            it_req_ids = req_ids.erase(it_req_ids);
        }else{
            ++it_req_ids;
        }
    }

    this->handler->clear_get_keys_entries(req_ids);

    if(!all_completed){
        // Nr of timeouts exceeded
        throw TimeoutException();
    }
}

std::unique_ptr<std::string> client::get(const std::string& key, int wait_for, long* version_ptr) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
    this->handler->register_get(req_id_str);
    std::unique_ptr<std::string> res (nullptr);

    int curr_timeouts = 0;
    while(res == nullptr && curr_timeouts < this->max_timeouts){
        peer_data peer = this->lb->get_peer(key); //throw exception (empty view)
        int status = this->send_get(peer, key, version_ptr, req_id_str);
        if (status == 0) {
//            spdlog::debug("GET " + std::to_string(req_id)  + " TO (" + std::to_string(peer.id) + ") " + key + (version_ptr == nullptr ? ": ?" : ": " + std::to_string(*version_ptr)) + " ==================================>");
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

long client::get_latest_version(const std::string& key, int wait_for) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
    this->handler->register_get_latest_version(req_id_str);
    std::unique_ptr<long> res (nullptr);

    int curr_timeouts = 0;
    while(res == nullptr && curr_timeouts < this->max_timeouts){
        peer_data peer = this->lb->get_peer(key); //throw exception (empty view)
        int status = this->send_get_latest_version(peer, key, req_id_str);
        if (status == 0) {
//            spdlog::debug("GET Version " + std::to_string(req_id) + " TO (" + std::to_string(peer.id) + ") " + " Key:" + key + " ==================================>");
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

//    spdlog::debug("######################" + key + " VERSIONR: " + std::to_string(*res) + "#################################");

    return *res;
}