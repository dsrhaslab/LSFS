#include "anti_entropy.h"

anti_entropy::anti_entropy(std::string ip, int kv_port, int recover_port, long id, double pos, pss *pss_ptr, group_construction* group_c,
        std::shared_ptr<kv_store<std::string>> store, long sleep_interval, bool recover_database, int total_packet_size_percentage):
        ip(std::move(ip)), kv_port(kv_port), recover_port(recover_port), id(id), pos(pos),
        store(std::move(store)), sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0)),
        phase(anti_entropy::Phase::Starting), pss_ptr(pss_ptr), group_c(group_c), recover_database(recover_database),
        total_packet_size_percentage(total_packet_size_percentage)
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
                spdlog::error("Oh dear, something went wrong with send()! %s\n", strerror(errno));
                std::cout << "Oh dear, something went wrong with send()!" << std::endl;
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
            spdlog::error("Oh dear, something went wrong with send! %s\n", strerror(errno));
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
    //Waiting for multiple replies until termination message arrival.
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

                kv_store_version version;
                for (auto c : key.key_version().version())
                    version.vv.emplace(c.client_id(), c.clock());
                version.client_id = key.key_version().client_id();

                FileType::FileType type = store.type() == proto::FileType::DIRECTORY? FileType::DIRECTORY : FileType::FILE;
                    
                kv_store_key<std::string> key_comp = {key.key(), version, type, store.is_deleted()};
                if(store.is_deleted()){
                    this->store->remove(key_comp);
                }else{
                    this->store->put(key_comp, message.data());
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
        std::unordered_map<kv_store_key<std::string>, size_t> main_keys = this->store->get_keys();
        auto it_main = main_keys.begin();
        std::unordered_set<kv_store_key<std::string>> deleted_keys = this->store->get_deleted_keys();
        auto it_deleted = deleted_keys.begin();

        int total_keys_to_send = deleted_keys.size() + main_keys.size();
        float percentage_of_deleted_keys_to_send = 0;
        float percentage_of_main_keys_to_send = 0;

        if(!deleted_keys.empty()) percentage_of_deleted_keys_to_send = (deleted_keys.size() * 100) / total_keys_to_send;
        if(!main_keys.empty()) percentage_of_main_keys_to_send = (main_keys.size() * 100) / total_keys_to_send;


        while(it_main != main_keys.end() || it_deleted != deleted_keys.end()){

            int total_size = (64507 * total_packet_size_percentage)/ 100; //Percentage of udp packet max size 

            if(total_size > 64507) total_size = 64507; //this number is lower than the max to be able to store the other parameters of the message.

            int main_keys_size_to_send = total_size - ((percentage_of_main_keys_to_send * total_size) / 100);
            int deleted_keys_size_to_send = total_size - ((percentage_of_deleted_keys_to_send * total_size) / 100);
            
            proto::kv_message message;
            message.set_forwarded_within_group(false);
            auto *message_content = new proto::anti_entropy_message();
            message_content->set_ip(this->ip);
            message_content->set_port(this->kv_port);
            message_content->set_id(this->id);
            
            while(total_size >= main_keys_size_to_send && it_main != main_keys.end()){

                auto& key_comp = it_main->first;
                auto& size = it_main->second;

                proto::kv_store* store = message_content->add_store_keys();
                proto::kv_store_key* kv_key = new proto::kv_store_key();
                kv_key->set_key(key_comp.key);

                proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();

                for(auto pair : key_comp.version.vv){

                    proto::kv_store_version *kv_version = kv_key_version->add_version();
                    kv_version->set_client_id(pair.first);
                    kv_version->set_clock(pair.second);
                }

                kv_key_version->set_client_id(key_comp.version.client_id);

                kv_key->set_allocated_key_version(kv_key_version);
                store->set_allocated_key(kv_key);
                store->set_is_deleted(false);
                if(key_comp.f_type == FileType::DIRECTORY) 
                    store->set_type(proto::FileType::DIRECTORY);
                else 
                    store->set_type(proto::FileType::FILE);
                store->set_data_size(size);

                size_t aproximated_size = store->ByteSizeLong();
                total_size -= aproximated_size;

                ++it_main;
            }

            while(total_size >= 0 && it_deleted != deleted_keys.end()){
                
                proto::kv_store* store_del = message_content->add_store_keys();
                proto::kv_store_key *deleted_kv_key = new proto::kv_store_key();
                deleted_kv_key->set_key(it_deleted->key);

                proto::kv_store_key_version* deleted_kv_key_version = new proto::kv_store_key_version();

                for(auto pair : it_deleted->version.vv){

                    proto::kv_store_version *deleted_kv_version = deleted_kv_key_version->add_version();
                    deleted_kv_version->set_client_id(pair.first);
                    deleted_kv_version->set_clock(pair.second);
                }

                deleted_kv_key_version->set_client_id(it_deleted->version.client_id);

                deleted_kv_key->set_allocated_key_version(deleted_kv_key_version);
                store_del->set_allocated_key(deleted_kv_key);
                store_del->set_is_deleted(true);
                if(it_deleted->f_type == FileType::DIRECTORY) 
                    store_del->set_type(proto::FileType::DIRECTORY);
                else 
                    store_del->set_type(proto::FileType::FILE);
                
                size_t aproximated_size = store_del->ByteSizeLong();
                total_size -= aproximated_size;

                ++it_deleted;
            }
            
            message.set_allocated_anti_entropy_msg(message_content);

            this->send_peer_keys(slice_view, message);
        }
    
    }catch (std::exception& e){
        // Unable to get Keys -> Do nothing
        e.what();
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

