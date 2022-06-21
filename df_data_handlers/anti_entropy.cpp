//
// Created by danielsf97 on 1/30/20.
//


#include "anti_entropy.h"

anti_entropy::anti_entropy(std::string ip, int kv_port, int recover_port, long id, double pos, pss *pss_ptr, group_construction* group_c,
        std::shared_ptr<kv_store<std::string>> store, long sleep_interval, bool recover_database): ip(std::move(ip)), kv_port(kv_port), recover_port(recover_port), id(id), pos(pos),
        store(std::move(store)), sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0)),
        phase(anti_entropy::Phase::Starting), pss_ptr(pss_ptr), group_c(group_c), recover_database(recover_database)
{}

void anti_entropy::send_peer_keys(std::vector<peer_data>& target_peers, proto::kv_message &msg){
    struct sockaddr_in serverAddr;
    memset(&serverAddr, '\0', sizeof(serverAddr));

    std::string buf;
    msg.SerializeToString(&buf);
    const char * buf_data = buf.data();
    auto buf_size = buf.size();

    serverAddr.sin_family = AF_INET;

    for(auto& peer: target_peers){
        serverAddr.sin_port = htons(peer.kv_port);
        serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());

        try {
            int res = sendto(this->sender_socket, buf_data, buf_size, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

            if(res == -1){
                spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            }
        }catch(...){
            spdlog::error("==================== Não consegui enviar =================");
        }
    }
}

int anti_entropy::send_recover_request(peer_data& target_peer){
    struct sockaddr_in serverAddr;
    memset(&serverAddr, '\0', sizeof(serverAddr));

    proto::kv_message message;
    message.set_forwarded_within_group(false);
    auto *message_content = new proto::recover_request_message();
    message_content->set_ip(this->ip);
    message_content->set_recover_port(this->recover_port);
    message_content->set_nr_slices(this->group_c->get_nr_groups());
    message_content->set_slice(this->group_c->get_my_group());
    message.set_allocated_recover_request_msg(message_content);

    std::string buf;
    message.SerializeToString(&buf);
    const char * buf_data = buf.data();
    auto buf_size = buf.size();

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(target_peer.kv_port);
    serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

    try {
        int res = sendto(this->sender_socket, buf_data, buf_size, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            return -1;
        }
    }catch(...){
        spdlog::error("===================== Unable to Send =================");
        return -1;
    }
    return 0;
}

bool anti_entropy::has_recovered(){
    std::lock_guard<std::mutex> lck(this->phase_mutex);
    return (this->phase == anti_entropy::Phase::Operating);
}

void anti_entropy::phase_starting() {
    if(this->group_c->has_recovered()){
        if(this->recover_database) {
            std::cout << "Phase starting =========> Phase Recovering" << std::endl;
            {
                std::lock_guard<std::mutex> lck(this->phase_mutex);
                this->phase = anti_entropy::Phase::Recovering;
            }
            this->phase_cv.notify_all();
        }else{
            std::cout << "Phase starting =========> Phase Operating" << std::endl;
            {
                std::lock_guard<std::mutex> lck(this->phase_mutex);
                this->phase = anti_entropy::Phase::Operating;
            }
            this->phase_cv.notify_all();
        }
    }
}

bool anti_entropy::recover_state(tcp_client_server_connection::tcp_server_connection& connection, int* socket){

    /*TODO Optmization -> get last 4 keys stored in order to not recover the whole database everytime
    */
    std::vector<std::string> keys_offset; //= this->store->get_last_keys_limit4();
    std::vector<std::string> keys_deleted_offset;

    proto::kv_message kv_message;
    kv_message.set_forwarded_within_group(false);
    auto* message_content = new proto::recover_offset_message();
    for(std::string& key: keys_offset)
        message_content->add_keys(key);
    for(std::string& del_key: keys_deleted_offset)
        message_content->add_deleted_keys(del_key);
    kv_message.set_allocated_recover_offset_msg(message_content);

    std::string buf;
    kv_message.SerializeToString(&buf);

    try {
        connection.send_msg(socket, buf.data(), buf.size());
    }catch(const char* e){
        std::cerr << "Exception: " << e << std::endl;
        return false;
    }

    bool finished_recovering = false;

    char rcv_buf [65500];
    while (!finished_recovering) {
        try {
            int bytes_rcv = connection.recv_msg(socket, rcv_buf); //throw exception

            proto::kv_message kv_message_rcv;
            kv_message_rcv.ParseFromArray(rcv_buf, bytes_rcv);

            if(kv_message_rcv.has_recover_termination_msg()){
                finished_recovering = true;
            }else if (kv_message_rcv.has_recover_data_msg()) {
                const proto::recover_data_message& message = kv_message_rcv.recover_data_msg();
                const proto::kv_store& store = message.store_keys();
                const proto::kv_store_key& key = store.key();

                kv_store_key_version version;
                for (auto c : key.version())
                    version.vv.emplace(c.client_id(), c.clock());
                    
                if(store.is_deleted()){
                    this->store->anti_entropy_remove(key.key(), version, message.data());
                }else{
                    this->store->anti_entropy_put(key.key(), version, message.data(), store.is_merge());
                }
            }
        }catch(const char* e) {
            std::cerr << "Exception: " << e << std::endl;
        }catch(PeerDisconnectedException& e){
            std::cerr << "Exception: " << e.what() << std::endl;
            return false;
        }catch(std::exception& e){
            std::cerr << "Exception: " << e.what() << std::endl;
        }
    }


    return finished_recovering;
}

void anti_entropy::phase_recovering() {
    tcp_client_server_connection::tcp_server_connection connection =
            tcp_client_server_connection::tcp_server_connection(this->ip.c_str(), this->recover_port);

    bool recovering = (this->phase == anti_entropy::Phase::Recovering);

    while(recovering){

        std::vector<peer_data> local_view = this->group_c->get_local_view();

        peer_data p = this->group_c->get_random_peer_from_local_view();
        int res = this->send_recover_request(p);
        if(res == -1){
            continue;
        }
        int socket = connection.accept_connection(2);
        if(socket != -1){
            bool recovered = this->recover_state(connection, &socket);
            if(recovered){
                {
                    std::lock_guard<std::mutex> lck(this->phase_mutex);
                    this->phase = anti_entropy::Phase::Operating;
                    recovering = false;
                    std::cout << "##### Recover Done #####" << std::endl;

                }
                this->phase_cv.notify_all();
            }
            close(socket);
        }
    }
}

void anti_entropy::wait_while_recovering(){
    std::unique_lock<std::mutex> lck(this->phase_mutex);
    this->phase_cv.wait(lck, [this]{ return this->phase == anti_entropy::Phase::Operating; });
}

void anti_entropy::phase_operating(){
    std::vector<peer_data> slice_view = pss_ptr->get_slice_local_view();

    try {
        proto::kv_message message;
        message.set_forwarded_within_group(false);
        auto *message_content = new proto::anti_entropy_message();
        message_content->set_ip(this->ip);
        message_content->set_port(this->kv_port);
        message_content->set_id(this->id);
        //Add random keys to propagate
        for (auto &key_size : this->store->get_keys()) {

            std::cout << "##### SENDING NORMAL KEY #####" << std::endl;

            proto::kv_store* store = message_content->add_store_keys();
            proto::kv_store_key* kv_key = new proto::kv_store_key();
            kv_key->set_key(key_size.first.key);

            std::cout << "Key: " << key_size.first.key << std::endl;
            std::cout << "Version: ";

            for(auto pair : key_size.first.key_version.vv){

                std::cout <<  pair.first << "@" << pair.second << ",";

                proto::kv_store_version *kv_version = kv_key->add_version();
                kv_version->set_client_id(pair.first);
                kv_version->set_clock(pair.second);
            }
            store->set_allocated_key(kv_key);
            store->set_is_deleted(false);
            store->set_data_size(key_size.second);
            store->set_is_merge(key_size.first.is_merge);

            std::cout << "--------------------------" << std::endl;
        }
        //Add random deleted keys to propagate
        for (auto &deleted_key : this->store->get_deleted_keys()) {
            std::cout << "##### SENDING DELETED KEY #####" << std::endl;

            proto::kv_store* store_del = message_content->add_store_keys();
            proto::kv_store_key *deleted_kv_key = new proto::kv_store_key();
            deleted_kv_key->set_key(deleted_key.key);

            std::cout << "Key: " << deleted_key.key << std::endl;
            std::cout << "Version: ";

            for(auto pair : deleted_key.key_version.vv){

                std::cout <<  pair.first << "@" << pair.second << ",";

                proto::kv_store_version *deleted_kv_version = deleted_kv_key->add_version();
                deleted_kv_version->set_client_id(pair.first);
                deleted_kv_version->set_clock(pair.second);
            }
            store_del->set_allocated_key(deleted_kv_key);
            store_del->set_is_deleted(true);
            store_del->set_is_merge(false);


            std::cout << "--------------------------" << std::endl;
        }

       

        message.set_allocated_anti_entropy_msg(message_content);

        std::cout << "################### Sending Anti-Entropy.... "<< std::endl;

        this->send_peer_keys(slice_view, message);
    }catch (std::exception& e){
        // Unable to get Keys -> Do nothing
    }
}

void anti_entropy::operator()() {
    this->running = true;

    while(this->running){
        if(this->running){
            std::unique_lock<std::mutex> lck(phase_mutex);
            switch(this->phase){
                case anti_entropy::Phase::Starting:
                    lck.unlock();
                    this->phase_starting();
		            { // if phase change we do not want to sleep before recover
                        std::lock_guard<std::mutex> lck(this->phase_mutex);
                        if(this->phase != anti_entropy::Phase::Starting){
                            continue;
                        }
                    }
                    break;
                case anti_entropy::Phase::Recovering:
                    lck.unlock();
                    this->phase_recovering();
                    break;   
                case anti_entropy::Phase::Operating:
                    lck.unlock();
                    this->phase_operating();
                    break;
                default:
                    break;
            }
        }
	std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
    }

    close(this->sender_socket);
}

void anti_entropy::stop_thread() {
    this->running = false;
}

