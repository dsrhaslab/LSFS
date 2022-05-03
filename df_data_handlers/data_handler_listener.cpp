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
    const std::string& key = message.key().key();
    const std::string& req_id = message.reqid();
    std::unique_ptr<std::string> data(nullptr); //*data = undefined
    bool request_already_replied = msg.forwarded_within_group();

    // if the request hasn't yet been processed
    if(!this->store->in_log(req_id)){
        
        std::cout << "Have never seen that request" << std::endl;
        
        this->store->log_req(req_id);

        kv_store_key_version version;
        for (auto c : message.key().version())
            version.vv.emplace(c.client_id(), c.clock());

        std::unique_ptr<std::vector<kv_store_key_version>> del_v(nullptr);
        bool is_deleted = false;


        if(this->store->get_slice_for_key(key) == this->store->get_slice())
            del_v = this->store->get_latest_deleted_version(key);

        
        if(del_v != nullptr){
            for(auto dv : *del_v){
                kVersionComp comp = comp_version(version, dv);
                if(comp == kVersionComp::Lower || comp == kVersionComp::Equal){
                    is_deleted = true;
                    break;
                }     
            }
        }

        if(!is_deleted){
            kv_store_key<std::string> get_key = {key, version};
            data = this->store->get(get_key);
        }

        if(data != nullptr || is_deleted){
            // if i have the key content
            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;

                build_get_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, std::move(data), key, version, is_deleted);

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
    }
}


void data_handler_listener::process_get_latest_version_msg(proto::kv_message msg) {
    const proto::get_latest_version_message& message = msg.get_latest_version_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();
    std::unique_ptr<std::vector<kv_store_key_version>> version(nullptr);
    std::unique_ptr<std::vector<kv_store_key_version>> del_v(nullptr);
    bool request_already_replied = msg.forwarded_within_group();

    // if request has not yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);
        // It only make sense to query for the last known version of the key
        // to peers that belong to the same slice of the key
        if(this->store->get_slice_for_key(key) == this->store->get_slice()){
            try {
                
                version = this->store->get_latest_version(key);
                del_v = this->store->get_latest_deleted_version(key);
                
                std::cout << "##### Key: " << key << " Vector: < ";
                for(auto c: *version){
                    std::cout << "(";
                    for(auto pair : c.vv)
                        std::cout <<  pair.first << "@" << pair.second << ",";
                     std::cout << "),";
                }

                std::cout << ">" << std::endl;
                // when the peer was supposed to have a key but it doesn't, replies with key version -1
                // to prevent for the client to have to wait for a timeout if a certain key does not exist.
                
                if(version == nullptr) 
                    version = std::make_unique<std::vector<kv_store_key_version>>();
                if(del_v == nullptr)
                    del_v = std::make_unique<std::vector<kv_store_key_version>>();
                
            }catch(std::exception& e){
                //LevelDBException
                e.what();
            }
        }

        if(version != nullptr || del_v != nullptr){
            // if the key belong to my slice

            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                //responder à mensagem
                proto::kv_message reply_message;

                build_get_latest_version_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, *version, *del_v);

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


void data_handler_listener::process_put_message(proto::kv_message &msg) {

    const auto& message = msg.put_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    
    kv_store_key_version version;
    for (auto c : message.key().version())
        version.vv.emplace(c.client_id(), c.clock());

    const std::string& data = message.data();
    bool request_already_replied = msg.forwarded_within_group();
    std::cout << " Starting Put " << std::endl;

    kv_store_key<std::string> key_comp = {key, version, false, false};

    if (!this->store->have_seen(key_comp)) {
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

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied|| achance <= this->chance) {
                proto::kv_message reply_message;

                //Put Reply Message builder
                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version);

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
    const std::string& key = message.key().key();

    kv_store_key_version version;
    for (auto c : message.key().version())
        version.vv.emplace(c.client_id(), c.clock());

    const std::string& data = message.data();
    bool request_already_replied = msg.forwarded_within_group();

    kv_store_key<std::string> key_comp = {key, version, false, true};
    

    if (!this->store->have_seen(key_comp)) {
        bool stored;
        try {
            stored = this->store->put_with_merge(key, version, data);
        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;

                //Put Reply Message builder
                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version);

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


void data_handler_listener::process_delete_message(proto::kv_message &msg) {

    const auto& message = msg.delete_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    
    kv_store_key_version version;
    for (auto c : message.key().version())
        version.vv.emplace(c.client_id(), c.clock());

    bool request_already_replied = msg.forwarded_within_group();
    std::cout << " Starting Delete " << std::endl;

    kv_store_key<std::string> key_comp = {key, version, true};
    

    if (!this->store->have_seen_deleted(key_comp)) {
        bool deleted;
        try {

            deleted = this->store->remove(key, version);

            std::cout << " Delete return " << deleted << std::endl;
    
            std::cout << " Starting print " << std::endl;
    
            this->store->print_store();

        }catch(std::exception& e){
            deleted = false;
        }

        if (deleted) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied|| achance <= this->chance) {
                proto::kv_message reply_message;
                
                build_delete_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version);

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
    std::cout << "################### Got Anti-Entropy.... "<< std::endl;
    try {
        
        //Normal keys - which keys do i really need
        std::unordered_set<kv_store_key<std::string>> keys_to_request;
        //Deleted keys 
        std::unordered_set<kv_store_key<std::string>> deleted_keys_to_request;
        
        for(auto& sk: message.store_keys()){
            auto& key = sk.key();
            kv_store_key_version version;
            for(auto& vector: key.version()){
                version.vv.emplace(vector.client_id(), vector.clock());
            }

            if(sk.is_deleted())
                deleted_keys_to_request.insert({key.key(), version, sk.is_deleted()});
            else
                keys_to_request.insert({key.key(), version, sk.is_deleted(), sk.is_merge()});
        }
        this->store->remove_from_set_existent_keys(keys_to_request);
        std::cout << "################### Only need " << keys_to_request.size() << " normal keys." << std::endl;

        this->store->remove_from_set_existent_deleted_keys(deleted_keys_to_request);
        std::cout << "################### Only need " << deleted_keys_to_request.size() << " deleted keys." << std::endl;

        
        for (auto &key: keys_to_request) {
            proto::kv_message get_msg;

            std::string req_id;
            req_id.reserve(50);
            req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));
            
            auto *message_content = new proto::anti_entropy_get_message();
            message_content->set_ip(this->ip);
            message_content->set_port(this->kv_port);
            message_content->set_id(this->id);
            message_content->set_reqid(req_id);
            message_content->set_is_deleted(false);
            message_content->set_is_merge(key.is_merge);
            
            proto::kv_store_key* kv_key = new proto::kv_store_key();
            kv_key->set_key(key.key);
            
            for(auto const c: key.key_version.vv){
                proto::kv_store_version *kv_version = kv_key->add_version();
                kv_version->set_client_id(c.first);
                kv_version->set_clock(c.second);
            }
            message_content->set_allocated_key(kv_key);
            
            get_msg.set_allocated_anti_entropy_get_msg(message_content);

            this->reply_client(get_msg, message.ip(), message.port());
        }

        for (auto &deleted_key: deleted_keys_to_request) {
            proto::kv_message get_msg;

            std::string req_id;
            req_id.reserve(50);
            req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));
            
            auto *message_content = new proto::anti_entropy_get_message();
            message_content->set_ip(this->ip);
            message_content->set_port(this->kv_port);
            message_content->set_id(this->id);
            message_content->set_reqid(req_id);
            message_content->set_is_deleted(true);
            message_content->set_is_merge(false);
            
            proto::kv_store_key* kv_key = new proto::kv_store_key();
            kv_key->set_key(deleted_key.key);
            
            for(auto const c: deleted_key.key_version.vv){
                proto::kv_store_version *kv_version = kv_key->add_version();
                kv_version->set_client_id(c.first);
                kv_version->set_clock(c.second);
            }
            message_content->set_allocated_key(kv_key);
            
            get_msg.set_allocated_anti_entropy_get_msg(message_content);

            this->reply_client(get_msg, message.ip(), message.port());
        }
    }catch (std::exception& e){
        // Unable to Get Keys
        return;
    }
}


void data_handler_listener::process_anti_entropy_get_message(proto::kv_message& msg) {
    const proto::anti_entropy_get_message& message = msg.anti_entropy_get_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const std::string& req_id = message.reqid();
    std::unique_ptr<std::string> data(nullptr); //*data = undefined
    bool request_already_replied = msg.forwarded_within_group();

    if(req_id.rfind("intern", 0) == 0){
        if(!this->store->in_anti_entropy_log(req_id)){
            kv_store_key_version version;

            for (auto c : message.key().version())
                version.vv.emplace(c.client_id(), c.clock());

            bool is_deleted = message.is_deleted();

            std::cout << "################### Received Get Anti-entropy Message, that has is_deleted = " << is_deleted << std::endl;

            kv_store_key<std::string> get_key = {key, version, is_deleted};

            this->store->log_anti_entropy_req(req_id);
            
            bool is_merge;
            data = this->store->get_anti_entropy(get_key, &is_merge);

            if(data != nullptr){
                proto::kv_message reply_message;

                auto* message_content = new proto::anti_entropy_get_reply_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->kv_port);
                message_content->set_id(this->id);
                message_content->set_reqid(req_id);
                
                message_content->set_data(data->data(), data->size());

                proto::kv_store_key* kv_key = new proto::kv_store_key();
                kv_key->set_key(key);
                for(auto const c: version.vv){
                    proto::kv_store_version* kv_version = kv_key->add_version();
                    kv_version->set_client_id(c.first);
                    kv_version->set_clock(c.second);
                }
                message_content->set_allocated_key(kv_key);

                message_content->set_is_deleted(is_deleted);
                message_content->set_is_merge(is_merge);
                
                reply_message.set_allocated_anti_entropy_get_reply_msg(message_content);

                std::cout << "################### Sending Get Reply Anti-entropy Message" << std::endl;

                this->reply_client(reply_message, sender_ip, sender_port);
            }else{
                // if i don't have the content of the message -> forward it
                std::vector<peer_data> view = this->pss_ptr->get_view();
                this->forward_message(view, const_cast<proto::kv_message &>(msg));
            }
        }
    }
}


void data_handler_listener::process_anti_entropy_get_reply_message(proto::kv_message &msg) {
    const proto::anti_entropy_get_reply_message& message = msg.anti_entropy_get_reply_msg();
    const std::string& key = message.key().key();
    
    std::cout << "################### Got Reply Message for key: " << key << std::endl;

    kv_store_key_version version;
    for (auto c : message.key().version())
        version.vv.emplace(c.client_id(), c.clock());

    const std::string& data = message.data();

    bool is_deleted = message.is_deleted();
    bool is_merge = message.is_merge();

    std::cout << "################### Is key deleted:  "<< is_deleted << std::endl;
    
    kv_store_key<std::string> key_comp = {key, version, is_deleted, is_merge};
    
    if(!is_deleted && !this->store->have_seen(key_comp)) {
        std::cout << "################### Entered normal key zone" << std::endl;
        try {
            if(is_merge){
                bool stored = this->store->put_with_merge(key, version, data);
            }else{
                bool stored = this->store->put(key, version, data);
            }
        } catch(std::exception& e){}
    }
    else if(is_deleted && !this->store->have_seen_deleted(key_comp)) {
        std::cout << "################### Entered deleted key zone" << std::endl;
        try {
            bool stored = this->store->anti_entropy_remove(key, version, data);
        } catch(std::exception& e){}
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
            std::vector<std::string> off_deleted_keys;
            for(auto& key: off_msg.keys()){
                off_keys.emplace_back(key);
            }
            for(auto& del_key: off_msg.deleted_keys()){
                off_deleted_keys.emplace_back(del_key);
            }

            // For Each Key greater than offset send to peer
            this->store->send_keys_gt(off_keys, off_deleted_keys, connection,
                                      [](tcp_client_server_connection::tcp_client_connection& connection, const std::string& key,
                                         std::map<long, long>& version, bool is_deleted, bool is_merge, const char* data, size_t data_size){
                                          proto::kv_message kv_message;
                                          kv_message.set_forwarded_within_group(false);
                                          auto* message_content = new proto::recover_data_message();
                                          
                                          proto::kv_store* store = new proto::kv_store();
                                          proto::kv_store_key* kv_key = new proto::kv_store_key();
                                          kv_key->set_key(key);

                                           for(auto const c: version){
                                                proto::kv_store_version *kv_version = kv_key->add_version();
                                                kv_version->set_client_id(c.first);
                                                kv_version->set_clock(c.second);
                                            }
                                          store->set_is_deleted(is_deleted);
                                          store->set_is_merge(is_merge);
                                          store->set_allocated_key(kv_key);

                                          message_content->set_allocated_store_keys(store);

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

