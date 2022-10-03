//
// Created by danielsf97 on 1/27/20.
//

#include "client.h"

client::client(std::string boot_ip, std::string ip, int kv_port, int pss_port, long id, std::string conf_filename):
    ip(ip), kv_port(kv_port), pss_port(pss_port), id(id), sender_socket(socket(PF_INET, SOCK_DGRAM, 0)), 
    request_count(0)
{
    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    auto client = main_confs["client"];
    auto load_balancer = client["load_balancer"];

    std::string base_path = client["base_path"].as<std::string>();
    this->nr_puts_required = client["nr_puts_required"].as<int>();
    this->nr_gets_required = client["nr_gets_required"].as<int>();
    this->nr_gets_version_required = client["nr_gets_version_required"].as<int>();
    this->max_nodes_to_send_get_request = client["max_nodes_to_send_get_request"].as<int>();
    this->max_nodes_to_send_put_request = client["max_nodes_to_send_put_request"].as<int>();
    this->max_timeouts = client["max_nr_requests_timeouts"].as<int>();
    bool mt_client_handler = client["mt_client_handler"].as<bool>();
    this->wait_timeout = client["client_wait_timeout"].as<long>();
    
    long lb_interval = load_balancer["lb_interval"].as<long>();
    auto load_balancer_type = load_balancer["type"].as<std::string>();

    if(load_balancer_type == "dynamic"){
        this->lb = std::make_shared<dynamic_load_balancer>(boot_ip, ip, pss_port, lb_interval);
    }else if(load_balancer_type == "smart"){
        this->lb = std::make_shared<smart_load_balancer>(boot_ip, ip, pss_port, lb_interval, conf_filename);
    }
    this->lb_listener = std::make_shared<load_balancer_listener>(this->lb, ip, pss_port);

    this->lb_th = std::thread (std::ref(*this->lb));
    this->lb_listener_th = std::thread (std::ref(*this->lb_listener));

    if(mt_client_handler){
        int nr_workers = client["nr_client_handler_ths"].as<int>();
        this->handler = std::make_shared<client_reply_handler_mt>(ip, kv_port, pss_port, this->wait_timeout, nr_workers);
    }else{
        this->handler = std::make_shared<client_reply_handler_st>(ip, kv_port, pss_port, this->wait_timeout);
    }
    this->handler_th = std::thread (std::ref(*this->handler));

    this->clock = std::make_shared<clock_vv>();
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
        serverAddr.sin_port = htons(target_peer.kv_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        msg.set_forwarded_within_group(false);

        std::string buf;
        msg.SerializeToString(&buf);
        std::unique_lock<std::mutex> lock(this->sender_socket_mutex);
        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        lock.unlock();

        if(res == -1){
            printf("Something went wrong with send()! %s\n", strerror(errno));
        }
        else{ 
            return 0; 
        }
    }catch(...){
        std::cout <<"===== Unable to send =====" << std::endl;
    }

    return 1;
}

int client::send_get(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const std::string& req_id, bool no_data) {
    proto::kv_message msg;

    build_get_message(&msg, this->ip, this->kv_port, this->id, req_id, key, version, no_data);
    
    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}

int client::send_get_latest_version(std::vector<peer_data>& peers, const std::string& key, const std::string& req_id, bool with_data) {
    proto::kv_message msg;
    
    build_get_latest_version_message(&msg, this->ip, this->kv_port, this->id, req_id, key, with_data);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}

int client::send_put(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const char *data, size_t size, bool extra_reply) {
    proto::kv_message msg;

    build_put_message(&msg, this->ip, this->kv_port, this->id, key, version, data, size, extra_reply);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}

int client::send_delete(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, bool extra_reply) {
    proto::kv_message msg;

    build_delete_message(&msg, this->ip, this->kv_port, this->id, key, version, extra_reply);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}


int client::send_put_with_merge(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const char *data, size_t size, bool extra_reply) {
    proto::kv_message msg;

    build_put_with_merge_message(&msg, this->ip, this->kv_port, this->id, key, version, data, size, extra_reply);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}


void client::put(const std::string& key, const kv_store_key_version& version, const char *data, size_t size, int wait_for) {
    long n_clock = clock->increment_and_get();
    kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), version);
    new_version.client_id = this->id;
    
    kv_store_key<std::string> comp_key = {key, new_version, false};
    this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        std::vector<peer_data> peers;
        if (curr_timeouts + 1 <= 2){
            peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
        }else{
            peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
        }

        if(curr_timeouts > 0){
            status = this->send_put(peers, key, new_version, data, size, true);
        }else{
            status = this->send_put(peers, key, new_version, data, size);
        }
        
        if(status == 0){
            try{
                succeed = this->handler->wait_for_put(comp_key, wait_for);
            }catch(TimeoutException& e){
                std::cout << "Timeout key Put: " << key << std::endl;
                curr_timeouts++;
            }
        }
    }

    //If put was completed but client did not receive the reply - last check before error
    if(!succeed){
        client_reply_handler::Response response = client_reply_handler::Response::Init;
        get(key, new_version, &response, wait_for, true);
        if(response == client_reply_handler::Response::Ok){
            succeed = true;
        }
    }
    
    if(!succeed){
        throw TimeoutException();
    }
}

//TODO - otimizacao keys sem ser const e alterar a versao para a nova versao por endereco
void client::put_batch(const std::vector<kv_store_key<std::string>> &keys,
                       const std::vector<const char *> &datas, const std::vector<size_t> &sizes, int wait_for) {

    long nr_writes = keys.size();
    std::vector<bool> completed(nr_writes, false);
    
    std::vector<kv_store_key<std::string>> keys_w_new_versions;

    //register all the keys and send first put
    for(size_t i = 0; i < keys.size(); i++){
        
        long n_clock = clock->increment_and_get();

        kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), keys[i].key_version);
        new_version.client_id = this->id;

        kv_store_key<std::string> comp_key = {keys[i].key, new_version, false};
        keys_w_new_versions.push_back(comp_key);
    
        this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
    }

    for(size_t i = 0; i < keys_w_new_versions.size(); i++){
        std::vector<peer_data> peers = this->lb->get_n_peers(keys[i].key, this->max_nodes_to_send_put_request); //throw exception
        this->send_put(peers, keys_w_new_versions[i].key, keys_w_new_versions[i].key_version, datas[i], sizes[i]);
    }


    long curr_timeouts = 0;
    bool all_completed = false;

    do{
        bool loop_timeout = false;
        auto wait_until = std::chrono::system_clock::now() + std::chrono::seconds(this->wait_timeout);
        for(size_t i = 0; i < completed.size(); i++){
            if(!completed[i]){
                std::string key = keys_w_new_versions[i].key;
                kv_store_key_version k_version = keys_w_new_versions[i].key_version;
                try{
                    completed[i] = this->handler->wait_for_put_until(keys_w_new_versions[i], wait_for, wait_until);
                }catch(TimeoutException& e){
                    std::cout << "Timeout key Put_batch: " << key << std::endl;
                    loop_timeout = true;
                    
                    std::vector<peer_data> peers;
                    if (curr_timeouts + 1 <= 2){
                        peers = this->lb->get_n_peers(keys_w_new_versions[i].key, this->max_nodes_to_send_put_request); //throw exception
                    }else{
                        peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
                    }
                    this->send_put(peers, key, k_version, datas[i], sizes[i], true);
                }
            }
        }
        if(loop_timeout){
            curr_timeouts++;
        }
        all_completed = std::find(completed.begin(), completed.end(), false) == completed.end();
    }while( !all_completed && curr_timeouts < this->max_timeouts); // do while there are incompleted puts

    // clear not succeded keys from put maps
    std::vector<kv_store_key<std::string>> erasing_keys;
    for(size_t i = 0; i < completed.size(); i++){
        if(!completed[i]){
            erasing_keys.push_back(keys_w_new_versions[i]);
        }
    }
    this->handler->clear_put_keys_entries(erasing_keys);

    //If put was completed but client did not receive the reply - last check before error
    if(!all_completed){
        for(size_t i = 0; i < completed.size(); i++){
            if(!completed[i]){
                std::string key = keys_w_new_versions[i].key;
                kv_store_key_version k_version = keys_w_new_versions[i].key_version;
                client_reply_handler::Response response = client_reply_handler::Response::Init;
                get(key, k_version, &response, wait_for, true);
                if(response == client_reply_handler::Response::Ok){
                    completed[i] = true;
                }
            }
        }
        all_completed = std::find(completed.begin(), completed.end(), false) == completed.end();
    }  

    if(!all_completed){
        throw TimeoutException();
    }
}

void client::put_with_merge(const std::string& key, const kv_store_key_version& version, const char *data, size_t size, int wait_for) {
    long n_clock = clock->increment_and_get();
    kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), version);
    new_version.client_id = this->id;
    
    kv_store_key<std::string> comp_key = {key, new_version, false, true};
    this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        std::vector<peer_data> peers;
        if (curr_timeouts + 1 <= 2){
            peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
        }else{
            peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
        }
        
        if(curr_timeouts > 0){
            status = this->send_put_with_merge(peers, key, new_version, data, size, true);
        }else{
            status = this->send_put_with_merge(peers, key, new_version, data, size);
        }

        if(status == 0){
            try{
                succeed = this->handler->wait_for_put(comp_key, wait_for);
            }catch(TimeoutException& e){
                std::cout << "Timeout key Put_w_merge: " << key << std::endl;
                curr_timeouts++;
            }
        }
    }
    
    //If put was completed but client did not receive the reply - last check before error
    if(!succeed){
        client_reply_handler::Response response = client_reply_handler::Response::Init;
        get(key, new_version, &response, wait_for, true);
        if(response == client_reply_handler::Response::Ok){
            succeed = true;
        }
    }

    if(!succeed){
        throw TimeoutException();
    }
}

void client::del(const std::string& key, const kv_store_key_version& version, int wait_for) {
    clock->increment();
    
    kv_store_key<std::string> comp_key = {key, version, true};
    this->handler->register_delete(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        std::vector<peer_data> peers;
        if (curr_timeouts + 1 <= 2){
            peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
        }else{
            peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
        }

        if(curr_timeouts > 0){
            status = this->send_delete(peers, key, version, true);
        }else{
            status = this->send_delete(peers, key, version);
        }

        if(status == 0){
            try{
                succeed = this->handler->wait_for_delete(comp_key, wait_for);
            }catch(TimeoutException& e){
                curr_timeouts++;
            }
        }
    }

    //If put was completed but client did not receive the reply - last check before error
    if(!succeed){
        client_reply_handler::Response response = client_reply_handler::Response::Init;
        get(key, version, &response, wait_for, true);
        if(response == client_reply_handler::Response::Deleted){
            succeed = true;
        }
    }

    if(!succeed){
        throw TimeoutException();
    }
}



std::unique_ptr<std::string> client::get(const std::string& key, const kv_store_key_version& version, client_reply_handler::Response* response, int wait_for, bool no_data) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));

    this->handler->register_get_data(req_id_str);
    std::unique_ptr<std::string> res (nullptr);

    int curr_timeouts = 0;

    while((*response) == client_reply_handler::Response::Init && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get(peers, key, version, req_id_str, no_data);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get(peers, key, version, req_id_str, no_data);
        }
        if (status == 0) {
            try{
                res = this->handler->wait_for_get(req_id_str, wait_for, response);
            }catch(TimeoutException& e){
                curr_timeouts++;
                req_id = this->inc_and_get_request_count();
                std::string latest_reqid_str = req_id_str;
                req_id_str.clear();
                req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
                this->handler->change_get_reqid(latest_reqid_str, req_id_str);
            }
        }
    }
    if(curr_timeouts >= this->max_timeouts){
        throw TimeoutException();
    }

    return res;
}


void client::get_latest_batch(const std::vector<std::string> &keys, std::vector<std::shared_ptr<std::string>> &data_strs, int wait_for) {

    long nr_reads = keys.size();
    std::vector<std::string> req_ids(nr_reads);

    //register all the keys and send first get
    for(size_t i = 0; i < keys.size(); i++){
        long req_id = this->inc_and_get_request_count();
        req_ids[i].reserve(100);
        req_ids[i].append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
        
        this->handler->register_get_data(req_ids[i]);

        std::vector<peer_data> peers = this->lb->get_n_peers(keys[i], this->max_nodes_to_send_get_request); //throw exception (empty view)
        this->send_get_latest_version(peers, keys[i], req_ids[i], true);
    }
    
    long curr_timeouts = 0;
    bool all_completed = false;
    std::vector<bool> timeout(nr_reads, false);
    bool happend_timeout = false;
    int res_count = 0;
    do{
        //wait until - numero de segundos que se espera para receber todos os 32k do batch, se exceder timeout
        auto wait_until = std::chrono::system_clock::now() + std::chrono::seconds(this->wait_timeout);

        for(size_t i = 0; i < data_strs.size(); i++){
            if(data_strs[i] == nullptr){
                try{
                    if(timeout[i]){
                        long req_id = this->inc_and_get_request_count();
                        std::string latest_reqid_str = req_ids[i];
                        req_ids[i].clear();
                        req_ids[i].append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
                        this->handler->change_get_reqid(latest_reqid_str, req_ids[i]);
                        if (curr_timeouts + 1 <= 2){
                            std::vector<peer_data> peers = this->lb->get_n_peers(keys[i], this->max_nodes_to_send_get_request); //throw exception (empty view)                            
                            this->send_get_latest_version(peers, keys[i], req_ids[i], true);
                        }else{
                            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception (empty view)
                            this->send_get_latest_version(peers, keys[i], req_ids[i], true);
                        }
                        timeout[i] = false;
                    }
                    
                    client_reply_handler::Response get_res = client_reply_handler::Response::Init;
                    data_strs[i] = this->handler->wait_for_get_latest_until(keys[i],req_ids[i], wait_for, wait_until, &get_res);
                                       
                    if(get_res == client_reply_handler::Response::Ok || get_res == client_reply_handler::Response::Deleted) 
                        res_count++;
                    
                }catch(TimeoutException& e){
                    std::cout << "Timeout key: " << keys[i] << std::endl;
                    timeout[i] = true;
                    happend_timeout = true;
                }
            }
        }
        if(happend_timeout){
            curr_timeouts++;
            happend_timeout = false;
        }
        all_completed = res_count == data_strs.size();
    }while( !all_completed && curr_timeouts < this->max_timeouts); // do while there are incompleted gets

    // clear not succeded keys from put maps
    auto it_req_ids = req_ids.begin();
    for(size_t i = 0; i < data_strs.size(); i++){
        if(data_strs[i] != nullptr){
            // if the block read succedded, remove entry from req_ids
            // as it's not necessary to remove that key from the get map
            it_req_ids = req_ids.erase(it_req_ids);
        }else{
            ++it_req_ids;
        }
    }
    this->handler->clear_get_keys_entries(req_ids);

    if(!all_completed){
        throw TimeoutException();
    }
}

std::unique_ptr<kv_store_key_version> client::get_latest_version(const std::string& key, client_reply_handler::Response* response, int wait_for) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
    this->handler->register_get_latest_version(req_id_str);
    std::unique_ptr<kv_store_key_version> res (nullptr);

    int curr_timeouts = 0;
    while((*response) == client_reply_handler::Response::Init && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_version(peers, key, req_id_str);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_version(peers, key, req_id_str);
        }
        if (status == 0) {
            try{
                res = this->handler->wait_for_get_latest_version(req_id_str, wait_for, response);
            }catch(TimeoutException& e){
                std::cout << "Timeout key Get_latest_version: " << key << std::endl;
                curr_timeouts++;
                req_id = this->inc_and_get_request_count();
                std::string latest_reqid_str = req_id_str;
                req_id_str.clear();
                req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
                this->handler->change_get_reqid(latest_reqid_str, req_id_str);
            }
        }
    }

    if(curr_timeouts >= this->max_timeouts){
        throw TimeoutException();
    }

    return res;
}


std::unique_ptr<std::string> client::get_latest(const std::string& key, client_reply_handler::Response* response, int wait_for) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
    this->handler->register_get_latest_version(req_id_str);
    std::unique_ptr<std::string> res (nullptr);

    int curr_timeouts = 0;
    while((*response) == client_reply_handler::Response::Init && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_version(peers, key, req_id_str, true);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_version(peers, key, req_id_str, true);
        }
        if (status == 0) {
            try{
                kv_store_key_version last_version;
                res = this->handler->wait_for_get_latest(key, req_id_str, wait_for, response, &last_version);
            }catch(TimeoutException& e){
                std::cout << "Timeout key Get_latest: " << key << std::endl;
                curr_timeouts++;
                req_id = this->inc_and_get_request_count();
                std::string latest_reqid_str = req_id_str;
                req_id_str.clear();
                req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
                this->handler->change_get_reqid(latest_reqid_str, req_id_str);
            }
        }
    }

    if(curr_timeouts >= this->max_timeouts){
        throw TimeoutException();
    }

    return res;
}

std::vector<std::unique_ptr<std::string>> client::get_latest_concurrent(const std::string& key, client_reply_handler::Response* response, int wait_for) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
    this->handler->register_get_latest_version(req_id_str);
    std::vector<std::unique_ptr<std::string>> res;

    int curr_timeouts = 0;
    while((*response) == client_reply_handler::Response::Init && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_version(peers, key, req_id_str, true);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_version(peers, key, req_id_str, true);
        }
        if (status == 0) {
            try{
                kv_store_key_version last_version;
                res = this->handler->wait_for_get_latest_concurrent(key, req_id_str, wait_for, response);
            }catch(TimeoutException& e){
                curr_timeouts++;
                req_id = this->inc_and_get_request_count();
                std::string latest_reqid_str = req_id_str;
                req_id_str.clear();
                req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
                this->handler->change_get_reqid(latest_reqid_str, req_id_str);
            }
        }
    }

    if(curr_timeouts >= this->max_timeouts){
        throw TimeoutException();
    }

    return res;
}


//---------------------------------------------------------------------------------------------------------------------------
//---------------------------------       Metadata Dedicated Requests       -------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------


int client::send_put_child(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const kv_store_key_version& past_version, const std::string& child_path, bool is_create, bool is_dir, bool extra_reply) {
    proto::kv_message msg;

    build_put_child_message(&msg, this->ip, this->kv_port, this->id, key, version, past_version, child_path, is_create, is_dir, extra_reply);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}


int client::send_put_metadata_stat(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const kv_store_key_version& past_version, const char *data, size_t size, bool extra_reply) {
    proto::kv_message msg;

    build_put_metadata_stat_message(&msg, this->ip, this->kv_port, this->id, key, version, past_version, data, size, extra_reply);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}


int client::send_get_latest_met_size_or_stat(std::vector<peer_data>& peers, const std::string& key, const std::string& req_id, bool get_size, bool get_stat) {
    proto::kv_message msg;

    build_get_latest_metadata_size_or_stat_message(&msg, this->ip, this->kv_port, this->id, req_id, key, get_size, get_stat);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}

int client::send_get_metadata(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const std::string& req_id) {
    proto::kv_message msg;

    build_get_metadata_message(&msg, this->ip, this->kv_port, this->id, req_id, key, version);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}



void client::put_child(const std::string& key, const kv_store_key_version& version, const std::string& child_path, bool is_create, bool is_dir, int wait_for) {
    long n_clock = clock->increment_and_get();

    kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), version);
    new_version.client_id = this->id;

    kv_store_key<std::string> comp_key = {key, new_version, false};
    this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        std::vector<peer_data> peers;
        if (curr_timeouts + 1 <= 2){
            peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
        }else{
            peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
        }

        if(curr_timeouts > 0){
            status = this->send_put_child(peers, key, new_version, version, child_path, is_create, is_dir, true);
        }else{
            status = this->send_put_child(peers, key, new_version, version, child_path, is_create, is_dir);
        }

        if(status == 0){
            try{         
                succeed = this->handler->wait_for_put(comp_key, wait_for);
            }catch(TimeoutException& e){
                std::cout << "Timeout key Put_child: " << key << std::endl;
                curr_timeouts++;
            }
        }
    }

    //If put was completed but client did not receive the reply - last check before error
    if(!succeed){
        client_reply_handler::Response response = client_reply_handler::Response::Init;
        get(key, new_version, &response, wait_for, true);
        if(response == client_reply_handler::Response::Ok){
            succeed = true;
        }
    }

    if(!succeed){
        throw TimeoutException();
    }
}


void client::put_metadata_stat(const std::string& key, const kv_store_key_version& version, const char *data, size_t size, int wait_for) {
    long n_clock = clock->increment_and_get();

    kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), version);
    new_version.client_id = this->id;
    
    kv_store_key<std::string> comp_key = {key, new_version, false};
    this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        std::vector<peer_data> peers;
        if (curr_timeouts + 1 <= 2){
            peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
        }else{
            peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception  
        }

        if(curr_timeouts > 0){
            status = this->send_put_metadata_stat(peers, key, new_version, version, data, size, true);
        }else{
            status = this->send_put_metadata_stat(peers, key, new_version, version, data, size);
        }

        if(status == 0){
            try{         
                succeed = this->handler->wait_for_put(comp_key, wait_for);
            }catch(TimeoutException& e){
                std::cout << "Timeout key Put_metadata_stat: " << key << std::endl;
                curr_timeouts++;
            }
        }
    }

    //If put was completed but client did not receive the reply - last check before error
    if(!succeed){
        client_reply_handler::Response response = client_reply_handler::Response::Init;
        get(key, new_version, &response, wait_for, true);
        if(response == client_reply_handler::Response::Ok){
            succeed = true;
        }
    }

    if(!succeed){
        throw TimeoutException();
    }
}




std::unique_ptr<std::string> client::get_latest_metadata_size(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version, int wait_for) {
    return get_latest_metadata_size_or_stat(key, response, last_version, true, false, wait_for);
}


std::unique_ptr<std::string> client::get_latest_metadata_stat(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version, int wait_for) {
    return get_latest_metadata_size_or_stat(key, response, last_version, false, true, wait_for);
}


std::unique_ptr<std::string> client::get_latest_metadata_size_or_stat(const std::string& key, client_reply_handler::Response* response, kv_store_key_version* last_version, bool get_size, bool get_stat, int wait_for) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
    this->handler->register_get_latest_version(req_id_str);
    std::unique_ptr<std::string> res(nullptr);

    int curr_timeouts = 0;
    while((*response) == client_reply_handler::Response::Init && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_met_size_or_stat(peers, key, req_id_str, get_size, get_stat);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get_latest_met_size_or_stat(peers, key, req_id_str, get_size, get_stat);
        }
        if (status == 0) {
            try{
                res = this->handler->wait_for_get_latest(key, req_id_str, wait_for, response, last_version);
            }catch(TimeoutException& e){
                std::cout << "Timeout key Get latest metadata size or stat: " << key << std::endl;
                curr_timeouts++;
                req_id = this->inc_and_get_request_count();
                std::string latest_reqid_str = req_id_str;
                req_id_str.clear();
                req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
                this->handler->change_get_reqid(latest_reqid_str, req_id_str);
            }
        }
    }

    if(curr_timeouts >= this->max_timeouts){
        throw TimeoutException();
    }

    return res;
}


// if data_strs null then the data was deleted!
void client::get_metadata_batch(const std::vector<kv_store_key<std::string>> &keys, std::vector<std::shared_ptr<std::string>> &data_strs, client_reply_handler::Response* response, int max_timeout, int wait_for) {

    long nr_reads = keys.size();
    std::vector<std::string> req_ids(nr_reads);

    //register all the keys and send first get
    for(size_t i = 0; i < keys.size(); i++){
        long req_id = this->inc_and_get_request_count();
        req_ids[i].reserve(100);
        req_ids[i].append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
        
        this->handler->register_get_data(req_ids[i]);

        std::vector<peer_data> peers = this->lb->get_n_peers(keys[i].key, this->max_nodes_to_send_get_request); //throw exception (empty view)
        this->send_get_metadata(peers, keys[i].key, keys[i].key_version, req_ids[i]);
    }

    long curr_timeouts = 0;
    bool all_completed = false;
    std::vector<bool> timeout(nr_reads, false);
    bool happend_timeout = false;
    int res_count = 0;
    *response = client_reply_handler::Response::Init;
    
    do{
        //wait until - numero de segundos que se espera para receber todos os 32k do batch, se exceder timeout
        auto wait_until = std::chrono::system_clock::now() + std::chrono::seconds(this->wait_timeout);

        for(size_t i = 0; i < data_strs.size(); i++){
            if(data_strs[i] == nullptr){
                try{
                    if(timeout[i]){
                        long req_id = this->inc_and_get_request_count();
                        std::string latest_reqid_str = req_ids[i];
                        req_ids[i].clear();
                        req_ids[i].append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
                        this->handler->change_get_reqid(latest_reqid_str, req_ids[i]);
                        if (curr_timeouts + 1 <= 2){
                            std::vector<peer_data> peers = this->lb->get_n_peers(keys[i].key, this->max_nodes_to_send_get_request); //throw exception (empty view)                            
                            this->send_get_metadata(peers, keys[i].key, keys[i].key_version, req_ids[i]);
                        }else{
                            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception (empty view)
                            this->send_get_metadata(peers, keys[i].key, keys[i].key_version, req_ids[i]);
                        }
                        timeout[i] = false;
                    }

                    client_reply_handler::Response get_res = client_reply_handler::Response::Init;
                    bool has_higher_version = false;
                    data_strs[i] = this->handler->wait_for_get_metadata_until(req_ids[i], wait_for, wait_until, &get_res, &has_higher_version);
                    
                    if(get_res == client_reply_handler::Response::Ok) 
                        res_count++;

                    if(get_res == client_reply_handler::Response::Deleted){
                        *response = client_reply_handler::Response::Deleted;
                        break;
                    }

                    if(has_higher_version){
                        *response = client_reply_handler::Response::NoData;
                        break;
                    }

                }catch(TimeoutException& e){
                    std::cout << "Timeout key: " << keys[i].key << " at " << curr_timeouts << std::endl;
                    timeout[i] = true;
                    happend_timeout = true;
                }
            }
        }
        if(happend_timeout){
            curr_timeouts++;
            happend_timeout = false;
        }
        if(*response == client_reply_handler::Response::NoData || *response == client_reply_handler::Response::Deleted){
            break;
        }

        all_completed = res_count == data_strs.size();
    }while( !all_completed && curr_timeouts < max_timeout); // do while there are incompleted gets

    if(all_completed){
        *response = client_reply_handler::Response::Ok;
    }

    // clear not succeded keys from put maps
    auto it_req_ids = req_ids.begin();
    for(size_t i = 0; i < data_strs.size(); i++){
        if(data_strs[i] != nullptr){
            // if the block read succedded, remove entry from req_ids
            // as it's not necessary to remove that key from the get map
            it_req_ids = req_ids.erase(it_req_ids);
        }else{
            ++it_req_ids;
        }
    }
    this->handler->clear_get_keys_entries(req_ids);

    if(curr_timeouts >= max_timeout){
        std::cout << "FULL TIMEOUT" << std::endl;
        throw TimeoutException();
    }
}



//-----------------------------------------------------------------------------------------------------



void client::del_db(const std::string& key, const kv_store_key_version& version, std::vector<std::string> peers_all) {
    
    kv_store_key<std::string> comp_key = {key, version, true};
    this->handler->register_delete(comp_key); // throw const char* (concurrent writes over the same key)
    
    std::vector<peer_data> peers;

    for(auto ip: peers_all){
        peer_data data;
        data.ip = ip;
        data.kv_port = 12356;
        peers.push_back(data);
    }

    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            status = this->send_delete(peers, key, version);
        }else{
            status = this->send_delete(peers, key, version);
        }
        if(status == 0){
            try{
                succeed = this->handler->wait_for_delete(comp_key, peers.size());
            }catch(TimeoutException& e){
                curr_timeouts++;
            }
        }
        std::cout << "Sended" << std::endl;
    }

    if(!succeed){
        throw TimeoutException();
    }
}
