//
// Created by danielsf97 on 1/16/20.
//

#include "data_handler_listener.h"
#include "df_communication/udp_async_server.h"

data_handler_listener::data_handler_listener(std::string ip, int kv_port, long id, float chance, pss* pss, group_construction* group_c, anti_entropy* anti_ent, std::shared_ptr<kv_store<std::string>> store, bool smart)
    : ip(std::move(ip)), kv_port(kv_port), id(id), chance(chance), pss_ptr(pss), group_c_ptr(group_c), anti_ent_ptr(anti_ent), store(std::move(store)), smart_forward(smart), socket_send(socket(PF_INET, SOCK_DGRAM, 0)) {}

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
            printf("Something went wrong with send()! %s\n", strerror(errno));
        }
    }catch(...){
        std::cout <<"===== Unable to send =====" << std::endl;
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
                printf("Something went wrong with send() forward! %s\n", strerror(errno));
            }
        }catch(...){
            std::cout <<"===== Unable to send =====" << std::endl;
        }
    }
}

void data_handler_listener::process_get_message(proto::kv_message &msg) {
    const proto::get_message& message = msg.get_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const std::string& req_id = message.reqid();
    bool no_data = message.no_data();
    bool request_already_replied = msg.forwarded_within_group();

    // if the request hasn't yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);

        std::unique_ptr<std::string> data(nullptr); 
        std::unique_ptr<std::vector<kv_store_key_version>> del_v(nullptr);
        bool is_deleted = false;
        kv_store_key_version version;
        version.client_id = message.key().key_version().client_id();

        for (auto c : message.key().key_version().version())
            version.vv.emplace(c.client_id(), c.clock());

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

            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;

                if(no_data) data = nullptr;

                build_get_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, std::move(data), key, version, is_deleted);

                this->reply_client(reply_message, sender_ip, sender_port);

                // forward to other peers from my slice if is the right slice for the key
                // trying to speed up quorum
                int obj_slice = this->store->get_slice_for_key(key);
                if(this->store->get_slice() == obj_slice){
                    std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                    msg.set_forwarded_within_group(true); //forward within group
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                }
            }else{
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
    const bool get_data = message.get_data();
    bool request_already_replied = msg.forwarded_within_group();

    // if request has not yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);

        //std::cout << "Get latest ##### Key: " << key << std::endl;

        std::unique_ptr<std::vector<kv_store_key_version>> version(nullptr);
        std::unique_ptr<std::vector<kv_store_key_version>> del_v(nullptr);
        std::vector<std::unique_ptr<std::string>> data_v;

        // It only make sense to query for the last known version of the key
        // to peers that belong to the same slice of the key
        if(this->store->get_slice_for_key(key) == this->store->get_slice()){
            try {
                 
                if(get_data)
                    version = this->store->get_latest_data_version(key, data_v);
                else
                    version = this->store->get_latest_version(key);

                del_v = this->store->get_latest_deleted_version(key);
                
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
            
            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;

                build_get_latest_version_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, *version, get_data, data_v, *del_v);

                this->reply_client(reply_message, sender_ip, sender_port);

                std::vector<peer_data> slice_peers = this->pss_ptr->get_slice_local_view();
                msg.set_forwarded_within_group(true); 
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
    const std::string& data = message.data();
    bool request_already_replied = msg.forwarded_within_group();
    
    kv_store_key_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());

    version.client_id = message.key().key_version().client_id();
    
    kv_store_key<std::string> key_comp = {key, version, false, false};

    if (!this->store->have_seen(key_comp)) {
        bool stored;
        try {
        
            stored = this->store->put(key_comp, data);
            
            if(key.find("/print_db") != std::string::npos) this->store->print_store(this->id);

        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;
                
                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, false);

                this->reply_client(reply_message, sender_ip, sender_port);

                msg.set_forwarded_within_group(true);
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
    const std::string& data = message.data();
    bool request_already_replied = msg.forwarded_within_group();

    kv_store_key_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();

    kv_store_key<std::string> key_comp = {key, version, false, true};

    if (!this->store->have_seen(key_comp)) {
    
        bool stored;

        try {
            stored = this->store->put_with_merge(key_comp, data);
    
        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;
    
                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, true);

                this->reply_client(reply_message, sender_ip, sender_port);
                msg.set_forwarded_within_group(true);
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
    bool request_already_replied = msg.forwarded_within_group();
    
    kv_store_key_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();
    
    kv_store_key<std::string> key_comp = {key, version, true};
    
    if (!this->store->have_seen_deleted(key_comp)) {
        bool deleted;
        try {
            
            if(key.find("delete_all_db") != std::string::npos){ 
                deleted = this->store->clean_db();
                this->store->clear_seen_log();
                this->store->clear_seen_deleted_log();
                this->store->clear_request_log();
                this->store->clear_anti_entropy_log();
                this->store->seen_it_deleted(key_comp);
            }else deleted = this->store->remove(key_comp);
        }catch(std::exception& e){
            deleted = false;
        }

        if (deleted) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;
                
                build_delete_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version);

                this->reply_client(reply_message, sender_ip, sender_port);
                msg.set_forwarded_within_group(true);
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





//---------------------------------------------------------------------------------------------------------------------------
//---------------------------------       Metadata Dedicated Requests       -------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------



void data_handler_listener::process_put_child_message(proto::kv_message &msg) {
    const auto& message = msg.put_child_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const long id = message.id();

    bool request_already_replied = msg.forwarded_within_group();
    
    kv_store_key_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();
    
    kv_store_key<std::string> key_comp = {key, version, false, false};

    kv_store_key_version past_v;
    for (auto c : message.past_key_version().version())
        past_v.vv.emplace(c.client_id(), c.clock());
    past_v.client_id = message.past_key_version().client_id();

    const bool is_create = message.is_create();
    const bool is_dir = message.is_dir();
    const std::string& child_path = message.child_path();

    if (!this->store->have_seen(key_comp)) {
        bool stored;
        try {
            stored = this->store->put_metadata_child(key, version, past_v, child_path, is_create, is_dir);
            
        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;

                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, false);

                this->reply_client(reply_message, sender_ip, sender_port);

                msg.set_forwarded_within_group(true);
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



void data_handler_listener::process_put_metadata_stat_message(proto::kv_message &msg) {
    const auto& message = msg.put_met_stat_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const std::string& data = message.data();
    bool request_already_replied = msg.forwarded_within_group();
    
    kv_store_key_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();
    
    kv_store_key<std::string> key_comp = {key, version, false, false};

    kv_store_key_version past_v;
    for (auto c : message.past_key_version().version())
        past_v.vv.emplace(c.client_id(), c.clock());
    past_v.client_id = message.past_key_version().client_id();

    if (!this->store->have_seen(key_comp)) {
        bool stored;
        try {
            stored = this->store->put_metadata_stat(key, version, past_v, data);

        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;

                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, false);

                this->reply_client(reply_message, sender_ip, sender_port);

                msg.set_forwarded_within_group(true);
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


void data_handler_listener::process_get_latest_metadata_size_or_stat_msg(proto::kv_message msg) {
    const auto& message = msg.get_latest_met_size_or_stat_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();
    const bool get_size = message.get_size();
    const bool get_stat = message.get_stat();

    bool request_already_replied = msg.forwarded_within_group();
    // if request has not yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);

        std::unique_ptr<std::vector<kv_store_key_version>> version(nullptr);
        std::unique_ptr<std::vector<kv_store_key_version>> del_v(nullptr);
        std::vector<std::unique_ptr<std::string>> data_v;

        // It only make sense to query for the last known version of the key
        // to peers that belong to the same slice of the key
        if(this->store->get_slice_for_key(key) == this->store->get_slice()){
            try {
                
                if(get_size){
                    version = this->store->get_metadata_size(key, data_v);
                }else{
                    version = this->store->get_metadata_stat(key, data_v);
                }
                del_v = this->store->get_latest_deleted_version(key);
                
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
            
            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;
    
                build_get_latest_version_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, *version, true, data_v, *del_v);

                this->reply_client(reply_message, sender_ip, sender_port);

                std::vector<peer_data> slice_peers = this->pss_ptr->get_slice_local_view();
                msg.set_forwarded_within_group(true); 
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


void data_handler_listener::process_get_metadata_message(proto::kv_message &msg) {
    const proto::get_metadata_message& message = msg.get_met_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const std::string& req_id = message.reqid();
    bool request_already_replied = msg.forwarded_within_group();

    // if the request hasn't yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);

        std::unique_ptr<std::string> data(nullptr); 
        std::unique_ptr<std::vector<kv_store_key_version>> del_v(nullptr);
        bool is_deleted = false;

        kv_store_key_version version;
        for (auto c : message.key().key_version().version())
            version.vv.emplace(c.client_id(), c.clock());
        version.client_id = message.key().key_version().client_id();

        std::string base_path;
        std::string blk_num_str;
        
        int res_1 = get_base_path(key, &base_path);
        int res_2 = get_blk_num(key, &blk_num_str);

        if(res_1 == 0 && res_2 == 0){

            int blk_num = std::stoi(blk_num_str);  

            if(this->store->get_slice_for_key(base_path) == this->store->get_slice())
                del_v = this->store->get_latest_deleted_version(base_path);

            
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
                
                kv_store_key<std::string> get_key = {base_path, version};
                data = this->store->get(get_key);
                if(data != nullptr){
                    size_t NR_BLKS = (data->size() / BLK_SIZE) + 1;
                    if(data->size() % BLK_SIZE > 0) NR_BLKS = NR_BLKS + 1;

                    size_t pos = (blk_num-1)*BLK_SIZE;

                    std::string value;

                    //Last block
                    if(blk_num == NR_BLKS){
                        value = data->substr(pos);  
                    }else{
                        value = data->substr(pos, BLK_SIZE); 
                    }

                    data = std::make_unique<std::string>(value);
                }
            }
        }

        if(data != nullptr || is_deleted){

            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;

                build_get_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, std::move(data), key, version, is_deleted);

                this->reply_client(reply_message, sender_ip, sender_port);

                // forward to other peers from my slice if is the right slice for the key
                // trying to speed up quorum
                int obj_slice = this->store->get_slice_for_key(key);
                if(this->store->get_slice() == obj_slice){
                    std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                    msg.set_forwarded_within_group(true); //forward within group
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                }
            }else{
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


//---------------------------------------------------------------------------------------------------------------------------


long data_handler_listener::get_anti_entropy_req_count(){
    return this->anti_entropy_req_count += 1;
}

void data_handler_listener::process_anti_entropy_message(proto::kv_message &msg) {
    const proto::anti_entropy_message& message = msg.anti_entropy_msg();
    std::cout << "################### Got Anti-Entropy.... "<< std::endl;
    try {
        
        //Normal keys - which keys do i really need
        std::unordered_map<kv_store_key<std::string>, size_t> keys_to_request;
        //Deleted keys 
        std::unordered_set<kv_store_key<std::string>> deleted_keys_to_request;
        //tmp Anti Entropy blocks to request
        std::vector<size_t> tmp_blks_to_request;
        
        for(auto& sk: message.store_keys()){
            auto& key = sk.key();
            kv_store_key_version version;
            for(auto& vector: key.key_version().version()){
                version.vv.emplace(vector.client_id(), vector.clock());
            }
            version.client_id = key.key_version().client_id();

            if(sk.is_deleted())
                deleted_keys_to_request.insert({key.key(), version, sk.is_deleted()});
            else{
                kv_store_key<std::string> st_key ={key.key(), version, sk.is_deleted()};
                keys_to_request.insert(std::make_pair(st_key, sk.data_size()));
            }
        }
        this->store->remove_from_map_existent_keys(keys_to_request);
        std::cout << "################### Only need " << keys_to_request.size() << " normal keys." << std::endl;

        this->store->remove_from_set_existent_deleted_keys(deleted_keys_to_request);
        std::cout << "################### Only need " << deleted_keys_to_request.size() << " deleted keys." << std::endl;

        for (auto &key_size: keys_to_request) {
            
            if(key_size.second > BLK_SIZE){
                    
                if(this->store->get_incomplete_blks(key_size.first, tmp_blks_to_request)){
                     
                    for(auto blk_num: tmp_blks_to_request){
                        
                        std::string blk_path;
                        blk_path.reserve(100);
                        blk_path.append(key_size.first.key).append(":").append(std::to_string(blk_num));

                        proto::kv_message get_msg;

                        std::string req_id;
                        req_id.reserve(50);
                        req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

                        build_anti_entropy_get_metadata_message(&get_msg, this->ip, this->kv_port, this->id, req_id, blk_path, key_size.first.key_version, false);

                        this->reply_client(get_msg, message.ip(), message.port());
                    }

                }else{
                    this->store->put_tmp_key_entry_size(key_size.first, key_size.second);

                    size_t NR_BLKS = (key_size.second / BLK_SIZE) + 1;    

                    for(int i = 1; i <= NR_BLKS; i++){

                        std::string blk_path;
                        blk_path.reserve(100);
                        blk_path.append(key_size.first.key).append(":").append(std::to_string(i));

                        proto::kv_message get_msg;

                        std::string req_id;
                        req_id.reserve(50);
                        req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

                        build_anti_entropy_get_metadata_message(&get_msg, this->ip, this->kv_port, this->id, req_id, blk_path, key_size.first.key_version, false);

                        this->reply_client(get_msg, message.ip(), message.port());               
                    }
                }
            }else{

                proto::kv_message get_msg;

                std::string req_id;
                req_id.reserve(50);
                req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

                build_anti_entropy_get_message(&get_msg, this->ip, this->kv_port, this->id, req_id, key_size.first.key, key_size.first.key_version, false);

                this->reply_client(get_msg, message.ip(), message.port());
            }            
        }

        for (auto &deleted_key: deleted_keys_to_request) {
            proto::kv_message get_msg;

            std::string req_id;
            req_id.reserve(50);
            req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

            //merge = false because this is deleted key, merge is irrelevant
            build_anti_entropy_get_message(&get_msg, this->ip, this->kv_port, this->id, req_id, deleted_key.key, deleted_key.key_version, true);
            
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
            for (auto c : message.key().key_version().version())
                version.vv.emplace(c.client_id(), c.clock());
            version.client_id = message.key().key_version().client_id();

            bool is_deleted = message.is_deleted();

            kv_store_key<std::string> get_key = {key, version, is_deleted};

            this->store->log_anti_entropy_req(req_id);
            
            bool is_merge;
            data = this->store->get_anti_entropy(get_key, &is_merge);

            if(data != nullptr){
                proto::kv_message reply_message;

                build_anti_entropy_get_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, version, is_deleted, is_merge, std::move(data));

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


void data_handler_listener::process_anti_entropy_get_metadata_message(proto::kv_message& msg) {
    const proto::anti_entropy_get_metadata_message& message = msg.anti_entropy_get_met_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const std::string& req_id = message.reqid();
    std::unique_ptr<std::string> data(nullptr); //*data = undefined
    bool request_already_replied = msg.forwarded_within_group();

    if(req_id.rfind("intern", 0) == 0){
        if(!this->store->in_anti_entropy_log(req_id)){

            this->store->log_anti_entropy_req(req_id);
            
            kv_store_key_version version;
            for (auto c : message.key().key_version().version())
                version.vv.emplace(c.client_id(), c.clock());
            version.client_id = message.key().key_version().client_id();

            bool is_deleted = message.is_deleted();
            bool is_merge = true; //merge is for metadata so true

            std::string base_path;
            std::string blk_num_str;
            
            int res_1 = get_base_path(key, &base_path);
            int res_2 = get_blk_num(key, &blk_num_str);

            if(res_1 == 0 && res_2 == 0){

                int blk_num = std::stoi(blk_num_str);  

                kv_store_key<std::string> get_key = {base_path, version, is_deleted};
                
                data = this->store->get_anti_entropy(get_key, &is_merge);
                if(data != nullptr){
                    size_t NR_BLKS = (data->size() / BLK_SIZE) + 1;

                    size_t pos = (blk_num-1)*BLK_SIZE;

                    std::string value;

                    //Last block
                    if(blk_num == NR_BLKS){
                        value = data->substr(pos);  
                    }else{
                        value = data->substr(pos, BLK_SIZE); 
                    }

                    data = std::make_unique<std::string>(value);
                }
            }
            
            if(data != nullptr){
                proto::kv_message reply_message;

                build_anti_entropy_get_metadata_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, version, is_deleted, is_merge, std::move(data));

                std::cout << "################### Sending Get Metadata Reply Anti-entropy Message" << std::endl;

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
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();

    const std::string& data = message.data();

    bool is_deleted = message.is_deleted();
    bool is_merge = message.is_merge();

    std::cout << "################### Is key deleted:  "<< is_deleted << std::endl;
    
    kv_store_key<std::string> key_comp = {key, version, is_deleted, is_merge};
    
    if(!is_deleted && !this->store->have_seen(key_comp)) {
        std::cout << "################### Entered normal key zone" << std::endl;
        try {
            if(is_merge){
                bool stored = this->store->put_with_merge(key_comp, data);
            }else{
                bool stored = this->store->put(key_comp, data);
            }
        } catch(std::exception& e){}
    }
    else if(is_deleted && !this->store->have_seen_deleted(key_comp)) {
        std::cout << "################### Entered deleted key zone" << std::endl;
        try {
            bool stored = this->store->anti_entropy_remove(key_comp, data);
        } catch(std::exception& e){}
    }
}


void data_handler_listener::process_anti_entropy_get_metadata_reply_message(proto::kv_message &msg) {
    const proto::anti_entropy_get_metadata_reply_message& message = msg.anti_entropy_get_met_reply_msg();
    const std::string& key = message.key().key();
    
    std::cout << "################### Got Reply Message for Metadata key: " << key << std::endl;

    kv_store_key_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();

    const std::string& data = message.data();

    bool is_deleted = message.is_deleted();
    bool is_merge = message.is_merge();

    kv_store_key<std::string> key_comp = {key, version, is_deleted, is_merge};
    
    if(!this->store->have_seen(key_comp) || !this->store->have_seen_deleted(key_comp)) {
        std::cout << "################### Trying to incorporate blk" << std::endl;
        try {
            std::string base_path;
            int res_1 = get_base_path(key, &base_path);

            bool completed = this->store->put_tmp_anti_entropy(base_path, key_comp, data);
            if(!completed) return;
            
            std::string value;
            size_t size;
            bool have_size = this->store->get_tmp_key_entry_size(base_path, key_comp, &value);
            if(have_size){
                size = stol(value);
                size_t blk_num = (size / BLK_SIZE) + 1;
                bool have_all = this->store->check_if_have_all_blks_and_put_metadata(base_path, key_comp, blk_num);
                if(have_all){
                    std::cout << "################### Have all the blks, incorporate in normal db" << std::endl;
                    this->store->delete_metadata_from_tmp_anti_entropy(base_path, key_comp, blk_num);
                }
            }
            

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
                                         std::map<long, long>& version, long client_id, bool is_deleted, bool is_merge, const char* data, size_t data_size){
                                          proto::kv_message kv_message;
                                          kv_message.set_forwarded_within_group(false);
                                          auto* message_content = new proto::recover_data_message();
                                          
                                          proto::kv_store* store = new proto::kv_store();
                                          proto::kv_store_key* kv_key = new proto::kv_store_key();
                                          kv_key->set_key(key);
                                          proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();

                                            for(auto const c: version){
                                                proto::kv_store_version *kv_version = kv_key_version->add_version();
                                                kv_version->set_client_id(c.first);
                                                kv_version->set_clock(c.second);
                                            }

                                          kv_key_version->set_client_id(client_id);

                                          kv_key->set_allocated_key_version(kv_key_version);

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

