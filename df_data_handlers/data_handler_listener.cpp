//
// Created by danielsf97 on 1/16/20.
//

#include "data_handler_listener.h"
#include "df_communication/udp_async_server.h"

data_handler_listener::data_handler_listener(std::string ip, int kv_port, long id, float chance, clock_vv* clock, pss* pss, group_construction* group_c, anti_entropy* anti_ent, std::shared_ptr<kv_store<std::string>> store, bool smart)
    : ip(std::move(ip)), kv_port(kv_port), id(id), chance(chance), clock_ptr(clock), pss_ptr(pss), group_c_ptr(group_c), anti_ent_ptr(anti_ent), store(std::move(store)), smart_forward(smart), socket_send(socket(PF_INET, SOCK_DGRAM, 0)) {}

void data_handler_listener::reply_client(proto::kv_message& message, const std::string& sender_ip, int sender_port){
    try{
        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(sender_port);
        serverAddr.sin_addr.s_addr = inet_addr(sender_ip.c_str());

        message.set_forwarded_within_group(false);

        std::string buf;
        message.SerializeToString(&buf);

        std::unique_lock<std::mutex> lock (socket_send_mutex);
        int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        lock.unlock();

        if(res == -1){
            printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));
        }
    }catch(...){
        std::cout <<"================== Não consegui enviar =================" << std::endl;
    }

}

void data_handler_listener::forward_message(const std::vector<peer_data>& view_to_send, proto::kv_message& message){
    std::string buf;
    message.SerializeToString(&buf);
    const char* data = buf.data();
    size_t data_size = buf.size();

    for(const peer_data& peer: view_to_send){
        try {
            struct sockaddr_in serverAddr;
            memset(&serverAddr, '\0', sizeof(serverAddr));

            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(peer.kv_port);
            serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());

            std::unique_lock<std::mutex> lock (socket_send_mutex);
            int res = sendto(this->socket_send, data, data_size, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            lock.unlock();

            if(res == -1){
                printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));
            }
        }catch(...){
            std::cout <<"================== Unable to send =================" << std::endl;
        }
    }
}

void data_handler_listener::process_get_message(proto::kv_message &msg) {
    const proto::get_message& message = msg.get_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();
    std::unique_ptr<std::string> data(nullptr); //*data = undefined
    bool request_already_replied = msg.forwarded_within_group();

    std::cout << "Message opened " << std::endl;
    
    // if the request hasn't yet been processed and its not a internal request
    if(!this->store->in_log(req_id) && req_id.rfind("intern", 0) != 0){

        this->store->log_req(req_id);
        kv_store_key_version version;

        switch (message.version_avail_case()){
            case proto::get_message::kVersionNone:
                std::cout << "Entrei " << std::endl;
                // It only make sense to query for the last known version of the key
                // to peers that belong to the same slice of the key
                if(this->store->get_slice_for_key(key) == this->store->get_slice()) {
                    try {
                        data = this->store->get_latest(key, &version);
                    } catch (std::exception &e) {
                        //LevelDBException
                        e.what();
                    }
                }
                break;
            case !proto::get_message::kVersionNone:
                
                for (auto c : message.version())
                    version.vv.emplace(c.client_id(), c.clock());

                kv_store_key<std::string> get_key = {key, version};
                std::cout << "Get Data " << std::endl;
            
                data = this->store->get(get_key);
                if (data != nullptr) std::cout << "Data " << *data << std::endl;
                break;
        }

        if(data != nullptr){
            // if i have the key content
            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;
                auto* message_content = new proto::get_reply_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->kv_port);
                message_content->set_id(this->id);
                message_content->set_key(key);
                message_content->set_reqid(req_id);
                message_content->set_data(data->data(), data->size());

                // construct version vector
                for(auto const c: version.vv){
                    proto::kv_store_version *kv_version = message_content->add_version();
                    kv_version->set_client_id(c.first);
                    kv_version->set_clock(c.second);
                }

                reply_message.set_allocated_get_reply_msg(message_content);

                std::cout << "Message get reply constructed" << std::endl;
                std::cout << "Replying to Client " << std::endl;

                this->reply_client(reply_message, sender_ip, sender_port);

                std::cout << "Forwarding to group view " << std::endl;
                // forward to other peers from my slice if is the right slice for the key
                // trying to speed up quorum
                int obj_slice = this->store->get_slice_for_key(key);
                if(this->store->get_slice() == obj_slice){
                    std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                    msg.set_forwarded_within_group(true); //forward within group
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                }
            }else{
                std::cout << "Forwarding " << std::endl;
                
                // if the message has already been replied by an element of the group and
                // the probability is for forward the message, we don't change the content of the message
                // just forward it
                int obj_slice = this->store->get_slice_for_key(key);
                if(this->store->get_slice() == obj_slice){
                    std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                }else {
                    std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
                    if (!slice_peers.empty() && this->smart_forward) {
                        this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
                    } else {
                        std::vector<peer_data> view = this->pss_ptr->get_view();
                        this->forward_message(view, const_cast<proto::kv_message &>(msg));
                    }
                }
            }
        }else{
            // if i don't have the content of the message -> forward it
            int obj_slice = this->store->get_slice_for_key(key);
            std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
            if(!slice_peers.empty() && this->smart_forward){
                this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
            }else{
                std::vector<peer_data> view = this->pss_ptr->get_view();
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            }
        }
    }else{
        // if its a internal get message (antientropy)
        if(req_id.rfind("intern", 0) == 0){
            if(!this->store->in_anti_entropy_log(req_id)){
                kv_store_key_version version;
                
                for (auto c : message.version())
                    version.vv.emplace(c.client_id(), c.clock());

                kv_store_key<std::string> get_key = {key, version};

                this->store->log_anti_entropy_req(req_id);
                
                bool is_merge;
                data = this->store->get_anti_entropy(get_key, &is_merge);

                if(data != nullptr){
                    proto::kv_message reply_message;
                    auto* message_content = new proto::get_reply_message();
                    message_content->set_ip(this->ip);
                    message_content->set_port(this->kv_port);
                    message_content->set_id(this->id);
                    message_content->set_key(key);
                    
                    // construct version vector
                    for(auto const c: version.vv){
                        proto::kv_store_version *kv_version = message_content->add_version();
                        kv_version->set_client_id(c.first);
                        kv_version->set_clock(c.second);
                    }
                    
                    message_content->set_reqid(req_id);
                    message_content->set_data(data->data(), data->size());
                    message_content->set_merge(is_merge);
                    reply_message.set_allocated_get_reply_msg(message_content);

                    this->reply_client(reply_message, sender_ip, sender_port);
                }else{
                    // if i don't have the content of the message -> forward it
                    std::vector<peer_data> view = this->pss_ptr->get_view();
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                }
            }
        }
    }

}

void data_handler_listener::process_get_reply_message(proto::kv_message &msg) {
    const proto::get_reply_message& message = msg.get_reply_msg();
    const std::string& key = message.key();

    kv_store_key_version version;
    for (auto c : message.version())
        version.vv.emplace(c.client_id(), c.clock());

    const std::string& data = message.data();
    bool is_merge = message.merge();

    if (!this->store->have_seen(key, version)) {
        try {
            if(is_merge){
                bool stored = this->store->put_with_merge(key, version, data);
            }else{
                bool stored = this->store->put(key, version, data);
            }
        }catch(std::exception& e){}
    }
}

void data_handler_listener::process_put_message(proto::kv_message &msg) {

    const auto& message = msg.put_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();
    
    kv_store_key_version version;
    for (auto c : message.version())
        version.vv.emplace(c.client_id(), c.clock());

    const std::string& data = message.data();
    bool request_already_replied = msg.forwarded_within_group();
    std::cout << " Starting Put " << std::endl;
    if (!this->store->have_seen(key, version)) {
        bool stored;
        try {
            stored = this->store->put(key, version, data);

            std::cout << " Put return " << stored << std::endl;
    
            std::cout << " Starting print " << std::endl;
    
            this->store->print_store();

        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {
            this->clock_ptr->increment();
            this->clock_ptr->print();

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied|| achance <= this->chance) {
                proto::kv_message reply_message;
                auto *message_content = new proto::put_reply_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->kv_port);
                message_content->set_id(this->id);
                message_content->set_key(key);
                
                for(auto const c: version.vv){
                    proto::kv_store_version *kv_version = message_content->add_version();
                    kv_version->set_client_id(c.first);
                    kv_version->set_clock(c.second);
                }
                reply_message.set_allocated_put_reply_msg(message_content);

                this->reply_client(reply_message, sender_ip, sender_port);
                msg.set_forwarded_within_group(true); //forward within group
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            } else {
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            }
        } else {
            if (this->smart_forward) {
                int obj_slice = this->store->get_slice_for_key(key);
                std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
                if (slice_peers.empty()) {
                    std::vector<peer_data> view = this->pss_ptr->get_view();
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                } else {
                    this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
                }
            } else {
                std::vector<peer_data> view = this->pss_ptr->get_view();
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            }
        }
    }
}

void data_handler_listener::process_put_with_merge_message(proto::kv_message &msg) {
    const auto& message = msg.put_with_merge_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();

    kv_store_key_version version;
    for (auto c : message.version())
        version.vv.emplace(c.client_id(), c.clock());

    const std::string& data = message.data();
    bool request_already_replied = msg.forwarded_within_group();

    if (!this->store->have_seen(key, version)) {
        bool stored;
        try {
            stored = this->store->put_with_merge(key, version, data);
        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {

            this->clock_ptr->increment();

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;
                auto *message_content = new proto::put_reply_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->kv_port);
                message_content->set_id(this->id);
                message_content->set_key(key);

                for(auto const c: version.vv){
                    proto::kv_store_version *kv_version = message_content->add_version();
                    kv_version->set_client_id(c.first);
                    kv_version->set_clock(c.second);
                }
                reply_message.set_allocated_put_reply_msg(message_content);

                this->reply_client(reply_message, sender_ip, sender_port);
                msg.set_forwarded_within_group(true); //forward within group
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            } else {
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            }
        } else {
            if (this->smart_forward) {
                int obj_slice = this->store->get_slice_for_key(key);
                std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
                if (slice_peers.empty()) {
                    std::vector<peer_data> view = this->pss_ptr->get_view();
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                } else {
                    this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
                }
            } else {
                std::vector<peer_data> view = this->pss_ptr->get_view();
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            }
        }
    }
}

long data_handler_listener::get_anti_entropy_req_count(){
    return this->anti_entropy_req_count += 1;
}

void data_handler_listener::process_anti_entropy_message(proto::kv_message &msg) {
    const proto::anti_entropy_message& message = msg.anti_entropy_msg();

    try {

        std::unordered_set<kv_store_key<std::string>> keys_to_request;
        for(auto& key: message.keys()){
            kv_store_key_version version;
            for(auto& vector: key.version()){
                version.vv.emplace(vector.client_id(), vector.clock());
            }
            keys_to_request.insert({key.key(), version});
        }
        this->store->remove_from_set_existent_keys(keys_to_request);

        std::string req_id;
        req_id.reserve(50);
        req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

        for (auto &key: keys_to_request) {
            proto::kv_message get_msg;
            auto *message_content = new proto::get_message();
            message_content->set_ip(this->ip);
            message_content->set_port(this->kv_port);
            message_content->set_id(this->id);
            message_content->set_key(key.key);

            for(auto const c: key.key_version.vv){
                proto::kv_store_version *kv_version = message_content->add_version();
                kv_version->set_client_id(c.first);
                kv_version->set_clock(c.second);
            }

            message_content->set_reqid(req_id);
            get_msg.set_allocated_get_msg(message_content);

            this->reply_client(get_msg, message.ip(), message.port());
        }
    }catch (std::exception& e){
        // Unable to Get Keys
        return;
    }
}

void data_handler_listener::process_get_latest_version_msg(proto::kv_message msg) {
    const proto::get_latest_version_message& message = msg.get_latest_version_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();
    std::unique_ptr<std::map<long, long>> version(nullptr);
    bool request_already_replied = msg.forwarded_within_group();

    // if request has not yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);
        // It only make sense to query for the last known version of the key
        // to peers that belong to the same slice of the key
        if(this->store->get_slice_for_key(key) == this->store->get_slice()){
            try {
                
                std::cout << "I have the key " << std::endl;
                version = this->store->get_latest_version(key);
                std::cout << "Got version" << std::endl;
                for(auto x: *version)
                    std::cout << "Version" << x.first << "@" << x.second << std::endl ;

                // when the peer was supposed to have a key but it doesn't, replies with key version -1
                // to prevent for the client to have to wait for a timeout if a certain key does not exist.
                
                //if (version == nullptr) {
                //    version = std::make_unique<long>(-1);
                
            }catch(std::exception& e){
                //LevelDBException
                e.what();
            }
        }

        if(version != nullptr){
            // if the key belong to my slice

            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                //responder à mensagem
                proto::kv_message reply_message;
                auto* message_content = new proto::get_latest_version_reply_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->kv_port);
                message_content->set_id(this->id);
                
                for(auto const c: *version){
                    proto::kv_store_version *kv_version = message_content->add_version();
                    kv_version->set_client_id(c.first);
                    kv_version->set_clock(c.second);
                }

                message_content->set_reqid(req_id);
                reply_message.set_allocated_get_latest_version_reply_msg(message_content);

                std::cout << "Sending version to client " << std::endl ;
                this->reply_client(reply_message, sender_ip, sender_port);

                std::vector<peer_data> slice_peers = this->pss_ptr->get_slice_local_view();
                msg.set_forwarded_within_group(true); //forward within group
                this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));

            }else{
                std::vector<peer_data> slice_peers = this->pss_ptr->get_slice_local_view();
                if(!slice_peers.empty() && this->smart_forward){
                    // forward to other peers from my slice as its the right slice for the key
                    // trying to speed up quorum
                    this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
                }else{
                    std::vector<peer_data> view = this->pss_ptr->get_view();
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                }
            }
        }else{
            //se não tenho o conteudo da chave -> fazer forward
            int obj_slice = this->store->get_slice_for_key(key);
            std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
            if(!slice_peers.empty() && this->smart_forward){
                this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
            }else{
                std::vector<peer_data> view = this->pss_ptr->get_view();
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            }
        }
    }
}

void data_handler_listener::process_recover_request_msg(proto::kv_message& msg) {
    const proto::recover_request_message& message = msg.recover_request_msg();
    const std::string& sender_ip = message.ip();
    int sender_recover_port = message.recover_port();
    int nr_slices = message.nr_slices();
    int slice = message.slice();

    if(this->group_c_ptr->get_my_group() != slice ||
       this->group_c_ptr->get_nr_groups() != nr_slices ||
       !this->group_c_ptr->has_recovered())
    {
        return;
    }

    try {
        tcp_client_server_connection::tcp_client_connection connection(sender_ip.c_str(), sender_recover_port);

        char rcv_buf[65500];

        int bytes_rcv = connection.recv_msg(rcv_buf); //throw exception

        proto::kv_message kv_message_rcv;
        kv_message_rcv.ParseFromArray(rcv_buf, bytes_rcv);

        if(kv_message_rcv.has_recover_offset_msg()) {

            const proto::recover_offset_message& off_msg = kv_message_rcv.recover_offset_msg();
            std::vector<std::string> off_keys;
            for(auto& key: off_msg.keys()){
                off_keys.emplace_back(key);
            }

            // For Each Key greater than offset send to peer
            this->store->send_keys_gt(off_keys, connection,
                                      [](tcp_client_server_connection::tcp_client_connection& connection, const std::string& key,
                                         std::map<long, long>& version, bool is_merge, const char* data, size_t data_size){
                                          proto::kv_message kv_message;
                                          kv_message.set_forwarded_within_group(false);
                                          auto* message_content = new proto::recover_data_message();
                                          message_content->set_key(key);

                                           for(auto const c: version){
                                                proto::kv_store_version *kv_version = message_content->add_version();
                                                kv_version->set_client_id(c.first);
                                                kv_version->set_clock(c.second);
                                            }
                                          
                                          message_content->set_data(data, data_size);
                                          kv_message.set_allocated_recover_data_msg(message_content);

                                          std::string buf;
                                          kv_message.SerializeToString(&buf);

                                          try {
                                              connection.send_msg(buf.data(), buf.size());
                                          }catch(std::exception& e){
                                              std::cerr << "Exception: " << e.what() << std::endl;
                                          }
                                      });

            // Send Recover Done Message
            proto::kv_message kv_message;
            kv_message.set_forwarded_within_group(false);
            auto* message_content = new proto::recover_termination_message();
            kv_message.set_allocated_recover_termination_msg(message_content);
            std::string buf;
            kv_message.SerializeToString(&buf);

            try {
                connection.send_msg(buf.data(), buf.size());
            }catch(std::exception& e){
                std::cerr << "Exception: " << e.what() << " " << strerror(errno) << std::endl;
            }
            connection.wait_for_remote_end_to_close_socket();
        }

    }catch(const char* e){
        std::cerr << "Exception: " << e << std::endl;
    }catch(std::exception& e){
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

