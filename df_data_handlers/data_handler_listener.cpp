#include "data_handler_listener.h"
#include "df_communication/udp_async_server.h"

data_handler_listener::data_handler_listener(std::string ip, int kv_port, long id, float chance, pss* pss, group_construction* group_c, 
                                        anti_entropy* anti_ent, std::shared_ptr<kv_store<std::string>> store, bool smart)
    : ip(std::move(ip)), kv_port(kv_port), id(id), chance(chance), pss_ptr(pss), group_c_ptr(group_c), anti_ent_ptr(anti_ent), 
        store(std::move(store)), smart_forward(smart), socket_send(socket(PF_INET, SOCK_DGRAM, 0)) {}


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

void data_handler_listener::forward_decider(proto::kv_message &msg, const std::string& key){
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
        std::vector<kv_store_version> del_v;
        bool is_deleted = false;
        
        kv_store_version version;
        for (auto c : message.key().key_version().version())
            version.vv.emplace(c.client_id(), c.clock());
        version.client_id = message.key().key_version().client_id();

        if(this->store->get_slice_for_key(key) == this->store->get_slice())
            if(this->store->get_latest_deleted_version(key, del_v)){
                for(auto dv : del_v){
                    kVersionComp comp = comp_version(version, dv);
                    if(comp == kVersionComp::Lower || comp == kVersionComp::Equal){
                        is_deleted = true;
                        break;
                    }
                }
            }
            
        
        if(!is_deleted){
            kv_store_key<std::string> key_comp = {key, version};
            data = this->store->get_data(key_comp);
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

        std::vector<kv_store_version> lastest_v;
        std::vector<kv_store_version> latest_del_v;
        std::vector<std::unique_ptr<std::string>> data_v;

        bool got_latest_version = false;
        bool got_latest_deleted_version = false;

        // It only make sense to query for the last known version of the key
        // to peers that belong to the same slice of the key
        
        if(this->store->get_slice_for_key(key) == this->store->get_slice()){
            try {
                if(get_data){
                    got_latest_version = this->store->get_latest_data_version(key, lastest_v, data_v);
                }else
                    got_latest_version = this->store->get_latest_version(key, lastest_v);

                got_latest_deleted_version = this->store->get_latest_deleted_version(key, latest_del_v);

                // when the peer was supposed to have a key but it doesn't, replies with key version -1
                // to prevent for the client to have to wait for a timeout if a certain key does not exist.
                
                if(!got_latest_version || !got_latest_deleted_version){
                    got_latest_version = true;
                    got_latest_deleted_version = true;
                }
                    
                
            }catch(std::exception& e){
                //LevelDBException
                e.what();
            }
        }

        if(got_latest_version || got_latest_deleted_version){
            
            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;

                build_get_latest_version_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, lastest_v, get_data, data_v, latest_del_v);

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
    bool request_already_replied = msg.forwarded_within_group();

    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const std::string& data = message.data();
    bool timed_out_extra_reply = message.extra_reply();
    
    kv_store_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();

    FileType::FileType f_type;
    if(message.type() == proto::FileType::DIRECTORY)
        f_type = FileType::DIRECTORY;
    else
        f_type = FileType::FILE;

    kv_store_key<std::string> key_comp = {key, version, f_type, false};

    if(!this->store->have_seen(key_comp)) {
        bool stored = false;
        try {
        
            stored = this->store->put(key_comp, data);
            
            if(key.find("/print_db") != std::string::npos){
                this->store->print_store(this->id);
            }

        }catch(std::exception& e){
            stored = false;
        }

        if(stored) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
           if(!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;
                
                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, f_type);

                this->reply_client(reply_message, sender_ip, sender_port);

                msg.set_forwarded_within_group(true);
            }
            
            this->forward_message(view, const_cast<proto::kv_message &>(msg));
                
        } else {
           forward_decider(msg, key);
        }
    }
    else if(timed_out_extra_reply){
        if(this->store->is_key_is_for_me(key_comp)){
            bool exists = this->store->verify_if_version_exists(key_comp);
            if(exists){

                float achance = random_float(0.0, 1.0);
                std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                if (!request_already_replied || achance <= this->chance) {
                    proto::kv_message reply_message;
                    
                    build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, f_type);

                    this->reply_client(reply_message, sender_ip, sender_port);
                }
            }else{
                forward_decider(msg, key);
            }
        } else {
            forward_decider(msg, key);
        }
    }
}


void data_handler_listener::process_delete_message(proto::kv_message &msg) {
    const auto& message = msg.delete_msg();
    bool request_already_replied = msg.forwarded_within_group();
    
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    bool timed_out_extra_reply = message.extra_reply();
    
    kv_store_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();
    
     FileType::FileType f_type;
    if(message.type() == proto::FileType::DIRECTORY)
        f_type = FileType::DIRECTORY;
    else 
        f_type = FileType::FILE;

    kv_store_key<std::string> key_comp = {key, version, f_type, true};
    
    if (!this->store->have_seen_deleted(key_comp)) {
        bool deleted;
        try {
            
            deleted = this->store->remove(key_comp);
            
        }catch(std::exception& e){
            deleted = false;
        }

        if (deleted) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;
                
                build_delete_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, f_type);

                this->reply_client(reply_message, sender_ip, sender_port);
                msg.set_forwarded_within_group(true);
            } 
            
            this->forward_message(view, const_cast<proto::kv_message &>(msg));
            
        } else {
            forward_decider(msg, key);
        }
    }
    else if(timed_out_extra_reply){
        if(this->store->is_key_is_for_me(key_comp)){
            bool exists = this->store->verify_if_version_exists_in_deleted(key_comp);
            if(exists){
                float achance = random_float(0.0, 1.0);
                std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                if (!request_already_replied || achance <= this->chance) {
                    proto::kv_message reply_message;
                    
                    build_delete_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, f_type);

                    this->reply_client(reply_message, sender_ip, sender_port);
                }
            }else{
                forward_decider(msg, key);
            }
        } else {
           forward_decider(msg, key);
        }
    }
}





//---------------------------------------------------------------------------------------------------------------------------
//---------------------------------       Metadata Dedicated Requests       -------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------



void data_handler_listener::process_put_child_message(proto::kv_message &msg) {
    const auto& message = msg.put_child_msg();
    bool request_already_replied = msg.forwarded_within_group();

    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key().key();
    const long id = message.id();
    bool timed_out_extra_reply = message.extra_reply();
    
    kv_store_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();
    
    kv_store_key<std::string> key_comp = {key, version, FileType::DIRECTORY, false};

    const bool is_create = message.is_create();
    const bool is_dir = message.is_dir();
    const std::string& child_path = message.child_path();

    if (!this->store->have_seen(key_comp)) {
        bool stored;
        try {
            stored = this->store->put_metadata_child(key_comp, child_path, is_create, is_dir);
            
        }catch(std::exception& e){
            stored = false;
        }

        if (stored) {

            float achance = random_float(0.0, 1.0);

            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (!request_already_replied || achance <= this->chance) {
                proto::kv_message reply_message;

                build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, FileType::DIRECTORY);

                this->reply_client(reply_message, sender_ip, sender_port);

                msg.set_forwarded_within_group(true);
            }

            this->forward_message(view, const_cast<proto::kv_message &>(msg));

        } else {
           forward_decider(msg, key);
        }
    }
    else if(timed_out_extra_reply){
        if(this->store->is_key_is_for_me(key_comp)){
            bool can_respond = this->store->verify_if_version_exists(key_comp);
            if(can_respond){
                float achance = random_float(0.0, 1.0);
                std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                if (!request_already_replied || achance <= this->chance) {
                    proto::kv_message reply_message;

                    build_put_reply_message(&reply_message, this->ip, this->kv_port, this->id, key, version, FileType::DIRECTORY);

                    this->reply_client(reply_message, sender_ip, sender_port);   
                }
            }
            else{
                forward_decider(msg, key);
            }
        } else {
            forward_decider(msg, key);
        }
    }
}


void data_handler_listener::process_get_latest_metadata_stat_msg(proto::kv_message msg) {
    const auto& message = msg.get_latest_met_stat_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();

    bool request_already_replied = msg.forwarded_within_group();
    // if request has not yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);

        std::vector<kv_store_version> latest_v;
        std::vector<kv_store_version> latest_del_v;
        std::vector<std::unique_ptr<std::string>> data_v;

        bool got_data = false;
        bool got_latest_deleted_version = false;

        // It only make sense to query for the last known version of the key
        // to peers that belong to the same slice of the key
        if(this->store->get_slice_for_key(key) == this->store->get_slice()){
            try {
                
                got_data = this->store->get_metadata_stat(key, latest_v, data_v);
                
                got_latest_deleted_version = this->store->get_latest_deleted_version(key, latest_del_v);
                
                // when the peer was supposed to have a key but it doesn't, replies with key version -1
                // to prevent for the client to have to wait for a timeout if a certain key does not exist.
                
                if(!got_data || !got_latest_deleted_version){
                    got_data = true;
                    got_latest_deleted_version = true;
                }
                   
                
            }catch(std::exception& e){
                //LevelDBException
                e.what();
            }
        }

        if(got_data || got_latest_deleted_version){
            
            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;
    
                build_get_latest_version_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, latest_v, true, data_v, latest_del_v);

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


void data_handler_listener::process_get_latest_metadata_size_msg(proto::kv_message msg) {
    const auto& message = msg.get_latest_met_size_msg();
    const std::string& sender_ip = message.ip();
    const int sender_port = message.port();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();

    bool request_already_replied = msg.forwarded_within_group();
    // if request has not yet been processed
    if(!this->store->in_log(req_id)){
        this->store->log_req(req_id);

        std::vector<kv_store_version> latest_v;
        std::vector<kv_store_version> latest_del_v;
        std::vector<std::unique_ptr<std::string>> data_v;

        bool got_data = false;
        bool got_latest_deleted_version = false;

        // It only make sense to query for the last known version of the key
        // to peers that belong to the same slice of the key
        if(this->store->get_slice_for_key(key) == this->store->get_slice()){
            try {

                got_data = this->store->get_metadata_size(key, latest_v, data_v);
                
                got_latest_deleted_version = this->store->get_latest_deleted_version(key, latest_del_v);
                
                // when the peer was supposed to have a key but it doesn't, replies with key version -1
                // to prevent for the client to have to wait for a timeout if a certain key does not exist.
                
                if(!got_data || !got_latest_deleted_version){
                    got_data = true;
                    got_latest_deleted_version = true;
                }
                   
                
            }catch(std::exception& e){
                //LevelDBException
                e.what();
            }
        }

        if(got_data || got_latest_deleted_version){
            
            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;
    
                build_get_latest_version_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, latest_v, true, data_v, latest_del_v);

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
        std::vector<kv_store_version> last_del_v;
        std::vector<kv_store_version> last_v;

        bool got_latest_version = false;
        bool got_latest_deleted_version = false;
        
        bool is_deleted = false;
        bool higher_version = false;

        kv_store_version version;
        for (auto c : message.key().key_version().version())
            version.vv.emplace(c.client_id(), c.clock());
        version.client_id = message.key().key_version().client_id();

        std::string base_path;
        std::string blk_num_str;
        
        int res_1 = get_base_path(key, &base_path);
        int res_2 = get_blk_num(key, &blk_num_str);

        if(res_1 == 0 && res_2 == 0){
            
            int blk_num = std::stoi(blk_num_str);

            if(this->store->get_slice_for_key(base_path) == this->store->get_slice()){
                
                kv_store_key<std::string> get_key = {base_path, version, FileType::DIRECTORY};
                data = this->store->get_data(get_key);
                
                if(data == nullptr){

                    got_latest_version = this->store->get_latest_version(base_path, last_v);
                    got_latest_deleted_version = this->store->get_latest_deleted_version(base_path, last_del_v);

                    if(got_latest_version){
                        kVersionComp comp = comp_version(version, last_v.at(0));
                        if(comp == kVersionComp::Lower){
                            higher_version = true;
                            
                            if(got_latest_deleted_version){
                                kVersionComp comp = comp_version(last_v.at(0), last_del_v.at(0));
                                if(comp == kVersionComp::Lower || comp == kVersionComp::Equal){
                                    is_deleted = true;
                                }
                            }
                        }
                    }

                    if(got_latest_deleted_version && !is_deleted){
                        kVersionComp comp = comp_version(version, last_del_v.at(0));
                        if(comp == kVersionComp::Lower || comp == kVersionComp::Equal){
                            is_deleted = true;
                        }
                    }
                }else{
                    data = split_data(std::move(data), blk_num);
                }
            }
        }
        
        if(data != nullptr || higher_version || is_deleted){

            float achance = random_float(0.0, 1.0);

            if(!request_already_replied || achance <= this->chance){
                proto::kv_message reply_message;

                build_get_metadata_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, std::move(data), key, version, is_deleted, higher_version);

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


// //---------------------------------------------------------------------------------------------------------------------------


long data_handler_listener::get_anti_entropy_req_count(){
    return this->anti_entropy_req_count += 1;
}

void data_handler_listener::process_anti_entropy_message(proto::kv_message &msg) {
    const proto::anti_entropy_message& message = msg.anti_entropy_msg();
    //std::cout << "################### Got Anti-Entropy.... "<< std::endl;
    try {
        
        //Normal keys - which keys do i really need
        std::unordered_map<kv_store_key<std::string>, size_t> keys_to_request;
        //Deleted keys 
        std::unordered_set<kv_store_key<std::string>> deleted_keys_to_request;
        //tmp Anti Entropy blocks to request
        std::vector<size_t> tmp_blks_to_request;
        
        for(auto& sk: message.store_keys()){
            auto& key = sk.key();
            kv_store_version version;
            for(auto& vector: key.key_version().version()){
                version.vv.emplace(vector.client_id(), vector.clock());
            }
            version.client_id = key.key_version().client_id();
            
            FileType::FileType f_type;
            if(sk.type() == proto::FileType::DIRECTORY)
                f_type = FileType::DIRECTORY;
            else 
                f_type = FileType::FILE;

            if(sk.is_deleted())
                deleted_keys_to_request.insert({key.key(), version, f_type, sk.is_deleted()});
            else{
                kv_store_key<std::string> st_key ={key.key(), version, f_type, sk.is_deleted()};
                keys_to_request.insert(std::make_pair(st_key, sk.data_size()));
            }
        }
        this->store->remove_from_map_existent_keys(keys_to_request);
        //std::cout << "################### Only need " << keys_to_request.size() << " normal keys." << std::endl;

        this->store->remove_from_set_existent_deleted_keys(deleted_keys_to_request);
        //std::cout << "################### Only need " << deleted_keys_to_request.size() << " deleted keys." << std::endl;

        for (auto &[key_comp, size]: keys_to_request) {
            
            if(key_comp.f_type == FileType::DIRECTORY && size > BLK_SIZE){
                    
                if(this->store->get_incomplete_blks(key_comp, size, tmp_blks_to_request)){
                     
                }else{
                    this->store->put_tmp_key_entry_size(key_comp, size);

                    size_t NR_BLKS = (size / BLK_SIZE);
                    if(size % BLK_SIZE > 0) NR_BLKS = NR_BLKS + 1;

                    for(int i = 1; i <= NR_BLKS; i++)
                        tmp_blks_to_request.push_back(i);

                }

                for(auto blk_num: tmp_blks_to_request){
                        
                    std::string blk_path;
                    blk_path.reserve(100);
                    blk_path.append(key_comp.key).append(":").append(std::to_string(blk_num));

                    proto::kv_message get_msg;

                    std::string req_id;
                    req_id.reserve(50);
                    req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

                    build_anti_entropy_get_metadata_message(&get_msg, this->ip, this->kv_port, this->id, req_id, blk_path, key_comp.version, key_comp.f_type, false);

                    this->reply_client(get_msg, message.ip(), message.port());
                }
            }else{

                proto::kv_message get_msg;

                std::string req_id;
                req_id.reserve(50);
                req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

                build_anti_entropy_get_message(&get_msg, this->ip, this->kv_port, this->id, req_id, key_comp.key, key_comp.version, key_comp.f_type, false);

                this->reply_client(get_msg, message.ip(), message.port());
            }            
        }

        for (auto &deleted_key: deleted_keys_to_request) {
            proto::kv_message get_msg;

            std::string req_id;
            req_id.reserve(50);
            req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

            build_anti_entropy_get_message(&get_msg, this->ip, this->kv_port, this->id, req_id, deleted_key.key, deleted_key.version, deleted_key.f_type, true);
            
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
    std::unique_ptr<std::string> data(nullptr);
    bool request_already_replied = msg.forwarded_within_group();
    
    if(req_id.rfind("intern", 0) == 0){
        if(!this->store->in_anti_entropy_log(req_id)){
            this->store->log_anti_entropy_req(req_id);
            
            kv_store_version version;
            for (auto c : message.key().key_version().version())
                version.vv.emplace(c.client_id(), c.clock());
            version.client_id = message.key().key_version().client_id();

            bool is_deleted = message.is_deleted();

            FileType::FileType f_type;
            if(message.type() == proto::FileType::DIRECTORY)
                f_type = FileType::DIRECTORY;
            else
                f_type == FileType::FILE;

            kv_store_key<std::string> get_key = {key, version, f_type, is_deleted};
            //TODO - Verify if f_type is correct.
            data = this->store->get_data(get_key);

            if(data != nullptr){
                proto::kv_message reply_message;

                build_anti_entropy_get_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, version, f_type, is_deleted, std::move(data));

                //std::cout << "################### Sending Get Reply Anti-entropy Message" << std::endl;

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
    std::unique_ptr<std::string> data(nullptr);
    bool request_already_replied = msg.forwarded_within_group();

    if(req_id.rfind("intern", 0) == 0){
        if(!this->store->in_anti_entropy_log(req_id)){
            this->store->log_anti_entropy_req(req_id);
            
            kv_store_version version;
            for (auto c : message.key().key_version().version())
                version.vv.emplace(c.client_id(), c.clock());
            version.client_id = message.key().key_version().client_id();

            FileType::FileType f_type;
            if(message.type() == proto::FileType::DIRECTORY)
                f_type = FileType::DIRECTORY;
            else
                f_type == FileType::FILE;

            bool is_deleted = message.is_deleted();

            std::string base_path;
            std::string blk_num_str;
            
            int res_1 = get_base_path(key, &base_path);
            int res_2 = get_blk_num(key, &blk_num_str);

            if(res_1 == 0 && res_2 == 0){

                int blk_num = std::stoi(blk_num_str);  

                kv_store_key<std::string> get_key = {base_path, version, f_type, is_deleted};
                
                data = this->store->get_data(get_key);
                
                data = split_data(std::move(data), blk_num);
            }
            
            if(data != nullptr){
                proto::kv_message reply_message;

                build_anti_entropy_get_metadata_reply_message(&reply_message, this->ip, this->kv_port, this->id, req_id, key, version, f_type, is_deleted, std::move(data));

                //std::cout << "################### Sending Get Metadata Reply Anti-entropy Message" << std::endl;

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
    
    //std::cout << "################### Got Reply Message for key: " << key << std::endl;

    kv_store_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();

    const std::string& data = message.data();

    FileType::FileType f_type;
    if(message.type() == proto::FileType::DIRECTORY)
        f_type = FileType::DIRECTORY;
    else
        f_type == FileType::FILE;

    bool is_deleted = message.is_deleted();

    //std::cout << "################### Is key deleted:  "<< is_deleted << std::endl;
    
    kv_store_key<std::string> key_comp = {key, version, f_type, is_deleted};
    
    if(!is_deleted && !this->store->have_seen(key_comp)) {
        //std::cout << "################### Entered normal key zone" << std::endl;
        try {
           
            bool stored = this->store->put(key_comp, data);
            
        } catch(std::exception& e){}
    }
    else if(is_deleted && !this->store->have_seen_deleted(key_comp)) {
        //std::cout << "################### Entered deleted key zone" << std::endl;
        try {
            bool stored = this->store->remove(key_comp);
        } catch(std::exception& e){}
    }
}


void data_handler_listener::process_anti_entropy_get_metadata_reply_message(proto::kv_message &msg) {
    const proto::anti_entropy_get_metadata_reply_message& message = msg.anti_entropy_get_met_reply_msg();
    const std::string& key = message.key().key();
    
    //std::cout << "################### Got Reply Message for Metadata key: " << key << std::endl;

    kv_store_version version;
    for (auto c : message.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = message.key().key_version().client_id();

    const std::string& data = message.data();

    FileType::FileType f_type;
    if(message.type() == proto::FileType::DIRECTORY)
        f_type = FileType::DIRECTORY;
    else
        f_type == FileType::FILE;

    bool is_deleted = message.is_deleted();

    kv_store_key<std::string> key_comp = {key, version, f_type, is_deleted};
    
    if(!this->store->have_seen(key_comp) || !this->store->have_seen_deleted(key_comp)) {
        //std::cout << "################### Trying to incorporate blk" << std::endl;
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
                size_t NR_BLKS = (size / BLK_SIZE);
                if(size % BLK_SIZE > 0) NR_BLKS = NR_BLKS + 1;
                bool have_all = this->store->check_if_have_all_blks_and_put_metadata(base_path, key_comp, NR_BLKS);
                if(have_all){
                    //std::cout << "################### Have all the blks, incorporate in normal db" << std::endl;
                    this->store->delete_metadata_from_tmp_anti_entropy(base_path, key_comp, NR_BLKS);
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
        // Se antes o peer mandar por ex um offset a partir do qual quer as restantes chaves (não existe isto com a leveldb)
        // char rcv_buf[65500];

        // int bytes_rcv = connection.recv_msg(rcv_buf); //throw exception

        // proto::kv_message kv_message_rcv;
        // kv_message_rcv.ParseFromArray(rcv_buf, bytes_rcv);

        // For Each Key greater than offset send to peer
        this->store->send_keys_gt(connection,
                                    [](tcp_client_server_connection::tcp_client_connection& connection, const std::string& key,
                                        std::map<long, long>& version, long client_id, bool is_deleted, FileType::FileType f_type, const char* data, size_t data_size){
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
                                        if(f_type == FileType::DIRECTORY)
                                        store->set_type(proto::FileType::DIRECTORY);
                                        else 
                                        store->set_type(proto::FileType::FILE);

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

    }catch(const char* e){
        std::cerr << "Exception: " << e << std::endl;
    }catch(std::exception& e){
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

