//
// Created by danielsf97 on 1/27/20.
//

#include "client.h"

client::client(std::string boot_ip, std::string ip, int kv_port, int pss_port, long id, std::string conf_filename):
    ip(ip), kv_port(kv_port), pss_port(pss_port), id(id), sender_socket(socket(PF_INET, SOCK_DGRAM, 0)), 
    request_count(0), clock()
{
    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    this->nr_puts_required = main_confs["nr_puts_required"].as<int>();
    this->nr_gets_required = main_confs["nr_gets_required"].as<int>();
    this->nr_gets_version_required = main_confs["nr_gets_version_required"].as<int>();
    this->max_nodes_to_send_get_request = main_confs["max_nodes_to_send_get_request"].as<int>();
    this->max_nodes_to_send_put_request = main_confs["max_nodes_to_send_put_request"].as<int>();
    this->max_timeouts = main_confs["max_nr_requests_timeouts"].as<int>();
    bool mt_client_handler = main_confs["mt_client_handler"].as<bool>();
    this->wait_timeout = main_confs["client_wait_timeout"].as<long>();
    long lb_interval = main_confs["lb_interval"].as<long>();
    auto load_balancer_type = main_confs["load_balancer"].as<std::string>();

    if(load_balancer_type == "dynamic"){
        this->lb = std::make_shared<dynamic_load_balancer>(boot_ip, ip, pss_port, lb_interval);
    }else if(load_balancer_type == "smart"){
        this->lb = std::make_shared<smart_load_balancer>(boot_ip, ip, pss_port, lb_interval, conf_filename);
    }
    this->lb_listener = std::make_shared<load_balancer_listener>(this->lb, ip, pss_port);

    this->lb_th = std::thread (std::ref(*this->lb));
    this->lb_listener_th = std::thread (std::ref(*this->lb_listener));

    if(mt_client_handler){
        int nr_workers = main_confs["nr_client_handler_ths"].as<int>();
        this->handler = std::make_shared<client_reply_handler_mt>(ip, kv_port, pss_port, this->wait_timeout, nr_workers);
    }else{
        this->handler = std::make_shared<client_reply_handler_st>(ip, kv_port, pss_port, this->wait_timeout);
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
        serverAddr.sin_port = htons(target_peer.kv_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        msg.set_forwarded_within_group(false);

        std::string buf;
        msg.SerializeToString(&buf);
        std::unique_lock<std::mutex> lock(this->sender_socket_mutex);
        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        lock.unlock();

        if(res == -1){spdlog::error("Something went wrong with read()! %s\n", strerror(errno));}
        else{ return 0; }
    }catch(...){spdlog::error("======= Não consegui enviar =======");}

    return 1;
}

int client::send_get(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const std::string& req_id) {
    proto::kv_message msg;

    build_get_message(&msg, this->ip, this->kv_port, this->id, req_id, key, version);
    
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

int client::send_put(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const char *data, size_t size) {
    proto::kv_message msg;

    build_put_message(&msg, this->ip, this->kv_port, this->id, key, version, data, size);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}

int client::send_delete(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version) {
    proto::kv_message msg;

    build_delete_message(&msg, this->ip, this->kv_port, this->id, key, version);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}


int client::send_put_with_merge(std::vector<peer_data>& peers, const std::string& key, const kv_store_key_version& version, const char *data, size_t size) {
    proto::kv_message msg;

    build_put_with_merge_message(&msg, this->ip, this->kv_port, this->id, key, version, data, size);

    int nr_send_errors = 0;
    for(auto& peer: peers){
        nr_send_errors += send_msg(peer, msg);
    }

    return (nr_send_errors >= peers.size())? 1 : 0;
}


void client::put(const std::string& key, const kv_store_key_version& version, const char *data, size_t size, int wait_for) {
    long n_clock = clock.increment_and_get();
    kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), version);
    
    kv_store_key<std::string> comp_key = {key, new_version, false};
    this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
            status = this->send_put(peers, key, new_version, data, size);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
            status = this->send_put(peers, key, new_version, data, size);
        }
        if(status == 0){
            try{
                std::cout << "Waiting for put of key: " << key   << std::endl;
            
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
//TODO - otimizacao keys sem ser const e alterar a versao para a nova versao por endereco
void client::put_batch(const std::vector<kv_store_key<std::string>> &keys,
                       const std::vector<const char *> &datas, const std::vector<size_t> &sizes, int wait_for) {

    long nr_writes = keys.size();
    std::vector<bool> completed(nr_writes, false);
    
    std::vector<kv_store_key<std::string>> keys_w_new_versions;

    //register all the keys and send first put
    for(size_t i = 0; i < keys.size(); i++){
        
        long n_clock = clock.increment_and_get();
        kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), keys[i].key_version);
        kv_store_key<std::string> comp_key = {keys[i].key, new_version, false};

        keys_w_new_versions.push_back(comp_key);
    
        this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
        
        std::vector<peer_data> peers = this->lb->get_n_peers(keys[i].key, this->max_nodes_to_send_put_request); //throw exception
        this->send_put(peers, keys[i].key, new_version, datas[i], sizes[i]);
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
                    std::cout << "####### Timeout" << std::endl;
                    loop_timeout = true;
                    if (curr_timeouts + 1 <= 2){
                        std::vector<peer_data> peers = this->lb->get_n_peers(keys_w_new_versions[i].key, this->max_nodes_to_send_put_request); //throw exception
                        this->send_put(peers, key, k_version, datas[i], sizes[i]);
                    }else{
                        std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
                        this->send_put(peers, key, k_version, datas[i], sizes[i]);
                    }
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

    if(!all_completed){
        // Nr of timeouts exceeded
        throw TimeoutException();
    }
}

void client::put_with_merge(const std::string& key, const kv_store_key_version& version, const char *data, size_t size, int wait_for) {
    long n_clock = clock.increment_and_get();
    kv_store_key_version new_version = add_vv(std::make_pair(this->id, n_clock), version);

    kv_store_key<std::string> comp_key = {key, new_version, false, true};
    this->handler->register_put(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
            status = this->send_put_with_merge(peers, key, new_version, data, size);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
            status = this->send_put_with_merge(peers, key, new_version, data, size);
        }
        if(status == 0){
            try{
                 std::cout << "Waiting for put_w_merge of key: " << key   << std::endl;
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

void client::del(const std::string& key, const kv_store_key_version& version, int wait_for) {
    clock.increment();

    kv_store_key<std::string> comp_key = {key, version, true};
    this->handler->register_delete(comp_key); // throw const char* (concurrent writes over the same key)
    
    bool succeed = false;
    int curr_timeouts = 0;
    while(!succeed && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_put_request); //throw exception
            status = this->send_delete(peers, key, version);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_put_request); //throw exception
            status = this->send_delete(peers, key, version);
        }
        if(status == 0){
            try{
                std::cout << "Waiting for delete of key: " << key << std::endl;
            
                succeed = this->handler->wait_for_delete(comp_key, wait_for);
            }catch(TimeoutException& e){
                curr_timeouts++;
            }
        }
    }

    if(!succeed){
        throw TimeoutException();
    }
}



std::unique_ptr<std::string> client::get(const std::string& key, int wait_for, const kv_store_key_version& version) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));

    this->handler->register_get_data(req_id_str);
    std::unique_ptr<std::string> res (nullptr);

    int curr_timeouts = 0;
    client_reply_handler::Response get_res = client_reply_handler::Response::Init;

    while(get_res == client_reply_handler::Response::Init && curr_timeouts < this->max_timeouts){
        int status = 0;
        if (curr_timeouts + 1 <= 2){
            std::vector<peer_data> peers = this->lb->get_n_peers(key, this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get(peers, key, version, req_id_str);
        }else{
            std::vector<peer_data> peers = this->lb->get_n_random_peers(this->max_nodes_to_send_get_request); //throw exception
            status = this->send_get(peers, key, version, req_id_str);
        }
        if (status == 0) {
            try{
                std::cout << "Waiting for Get "<< std::endl;
                res = this->handler->wait_for_get(req_id_str, wait_for, &get_res);
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

    if (get_res == client_reply_handler::Response::Ok)
        std::cout << "Data Found, Ok "<< std::endl ;
    else if(get_res == client_reply_handler::Response::Deleted)
        std::cout << "Searched key has been deleted "<< std::endl ;
    else if (get_res == client_reply_handler::Response::NoData)
        std::cout << "Key was found but data is null "<< std::endl ;
    else if (get_res == client_reply_handler::Response::Init)
        std::cout << " No response obtained/Timeout "<< std::endl ;

    return res;
}


//Sera que pode acontecer do bloco do meio ser apagado?, nao estou a considerar esse caso por achar que não acontece
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
        // Nr of timeouts exceeded
        throw TimeoutException();
    }
}

std::unique_ptr<kv_store_key_version> client::get_latest_version(const std::string& key, int wait_for) {
    long req_id = this->inc_and_get_request_count();
    std::string req_id_str;
    req_id_str.reserve(100);
    req_id_str.append(std::to_string(this->id)).append(":").append(this->ip).append(":").append(std::to_string(req_id));
    this->handler->register_get_latest_version(req_id_str);
    std::unique_ptr<kv_store_key_version> res (nullptr);

    client_reply_handler::Response get_res = client_reply_handler::Response::Init;

    int curr_timeouts = 0;
    while(get_res == client_reply_handler::Response::Init && curr_timeouts < this->max_timeouts){
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
                res = this->handler->wait_for_get_latest_version(req_id_str, wait_for, &get_res);
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