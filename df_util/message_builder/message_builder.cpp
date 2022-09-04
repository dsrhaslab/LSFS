
#include "message_builder.h"


void build_get_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                const std::string& key, const kv_store_key_version& version, bool no_data){

    auto *message_content = new proto::get_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);
    message_content->set_no_data(no_data);
    
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_key_version);
    
    message_content->set_allocated_key(kv_key);

    msg->set_allocated_get_msg(message_content);
}


void build_get_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                std::unique_ptr<std::string> data,
                                const std::string& key, const kv_store_key_version& version, bool is_deleted){

    auto* message_content = new proto::get_reply_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);
    
    if(data != nullptr)
        message_content->set_data(data->data(), data->size());

    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);

    proto::kv_store_key_version* kv_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version* vm = kv_version->add_version();
        vm->set_client_id(c.first);
        vm->set_clock(c.second);
    }
    kv_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_version);

    message_content->set_allocated_key(kv_key);

    message_content->set_version_is_deleted(is_deleted);
    
    msg->set_allocated_get_reply_msg(message_content);
}


void build_get_latest_version_message(proto::kv_message* msg, std::string& ip, int kv_port, long id,
                                 const std::string& req_id, const std::string& key, bool get_data){
    
    auto* message_content = new proto::get_latest_version_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_key(key);
    message_content->set_reqid(req_id);
    message_content->set_get_data(get_data);
    msg->set_allocated_get_latest_version_msg(message_content);                                 
}


void build_get_latest_version_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id,
                                 const std::string& req_id, const std::string& key, const std::vector<kv_store_key_version>& vversion, bool bring_data, const std::vector<std::unique_ptr<std::string>>& vdata, const std::vector<kv_store_key_version>& vdel_version){

    auto* message_content = new proto::get_latest_version_reply_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);

    message_content->set_key(key);
    for(int i = 0; i < vversion.size() ; i++){
        proto::kv_store_key_version_w_data *kv_key_version = message_content->add_last_v();
        
        if(bring_data && vdata[i] != nullptr) 
            kv_key_version->set_data(vdata[i]->data(), vdata[i]->size());

        for(auto const c: vversion[i].vv){
            proto::kv_store_version *kv_version = kv_key_version->add_version();
            kv_version->set_client_id(c.first);
            kv_version->set_clock(c.second);
        }
        kv_key_version->set_client_id(vversion[i].client_id);
    }

    for(auto const v: vdel_version){
        proto::kv_store_key_version *kv_del_version = message_content->add_last_deleted_v();
        for(auto const c: v.vv){
            proto::kv_store_version *kv_del_v = kv_del_version->add_version();
            kv_del_v->set_client_id(c.first);
            kv_del_v->set_clock(c.second);
        }
        kv_del_version->set_client_id(v.client_id);
    }
    msg->set_allocated_get_latest_version_reply_msg(message_content);
}


void build_put_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, 
                                const std::string& key, const kv_store_key_version& version, const char* data, size_t size){
    
    auto* message_content = new proto::put_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);

    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_key_version);
    message_content->set_allocated_key(kv_key);
    message_content->set_data(data, size);
    msg->set_allocated_put_msg(message_content);
}

void build_put_with_merge_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, 
                                const std::string& key, const kv_store_key_version& version, const char* data, size_t size){

    auto* message_content = new proto::put_with_merge_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_key_version);
    message_content->set_allocated_key(kv_key);
    message_content->set_data(data, size);
    msg->set_allocated_put_with_merge_msg(message_content);
                                }


void build_put_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, 
                                const std::string& key, const kv_store_key_version& version, const bool is_merge){
    
    auto *message_content = new proto::put_reply_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_is_merge(is_merge);
    
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);

    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);

    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);
    
    msg->set_allocated_put_reply_msg(message_content);
}


void build_delete_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, 
                                const std::string& key, const kv_store_key_version& version){
   
    auto* message_content = new proto::delete_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_key_version);
    message_content->set_allocated_key(kv_key);
    msg->set_allocated_delete_msg(message_content);            
}



void build_delete_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, 
                                const std::string& key, const kv_store_key_version& version){

    auto *message_content = new proto::delete_reply_message();

    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);

    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);

    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();

    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }

    kv_key_version->set_client_id(version.client_id);

    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);

    msg->set_allocated_delete_reply_msg(message_content);

}

void build_put_child_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, 
                                const std::string& key, const kv_store_key_version& version, const kv_store_key_version& past_version, const std::string& child_path, bool is_create, bool is_dir){
    
    auto* message_content = new proto::put_child_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_key_version);
    
    message_content->set_allocated_key(kv_key);

    proto::kv_store_key_version* kv_vv = new proto::kv_store_key_version();
    for(auto const c: past_version.vv){
        proto::kv_store_version *past_kv_version = kv_vv->add_version();
        past_kv_version->set_client_id(c.first);
        past_kv_version->set_clock(c.second);
    }
    kv_vv->set_client_id(past_version.client_id);

    message_content->set_allocated_past_key_version(kv_vv);

    message_content->set_is_create(is_create);
    message_content->set_is_dir(is_dir);
    message_content->set_child_path(child_path);
    
    msg->set_allocated_put_child_msg(message_content);
}


void build_put_metadata_stat_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, 
                                const std::string& key, const kv_store_key_version& version, const kv_store_key_version& past_version, const char *data, size_t size){
    
    auto* message_content = new proto::put_metadata_stat_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);

    proto::kv_store_key_version* kv_vv = new proto::kv_store_key_version();
    for(auto const c: past_version.vv){
        proto::kv_store_version *past_kv_version = kv_vv->add_version();
        past_kv_version->set_client_id(c.first);
        past_kv_version->set_clock(c.second);
    }
    kv_vv->set_client_id(past_version.client_id);
    
    message_content->set_allocated_past_key_version(kv_vv);
    message_content->set_data(data, size);
    
    msg->set_allocated_put_met_stat_msg(message_content);
}

void build_get_latest_metadata_size_or_stat_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                const std::string& key, bool get_size, bool get_stat){
    
    auto* message_content = new proto::get_latest_metadata_size_or_stat_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);
    message_content->set_key(key);
    message_content->set_get_size(get_size);
    message_content->set_get_stat(get_stat);
    
    msg->set_allocated_get_latest_met_size_or_stat_msg(message_content);
}


void build_get_metadata_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                const std::string& key, const kv_store_key_version& version){
    
    auto* message_content = new proto::get_metadata_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);

    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();
    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }
    kv_key_version->set_client_id(version.client_id);
    
    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);
    
    msg->set_allocated_get_met_msg(message_content);
}


void build_anti_entropy_get_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                const std::string& key, const kv_store_key_version& version, bool is_deleted){

    auto *message_content = new proto::anti_entropy_get_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);
    message_content->set_is_deleted(is_deleted);
    
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();

    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }

    kv_key_version->set_client_id(version.client_id);

    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);
    
    msg->set_allocated_anti_entropy_get_msg(message_content);

}


void build_anti_entropy_get_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                const std::string& key, const kv_store_key_version& version, bool is_deleted, bool is_merge, std::unique_ptr<std::string> data){

    auto* message_content = new proto::anti_entropy_get_reply_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);
    
    message_content->set_data(data->data(), data->size());

    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);

    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();

    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }

    kv_key_version->set_client_id(version.client_id);

    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);

    message_content->set_is_deleted(is_deleted);
    message_content->set_is_merge(is_merge);
    
    msg->set_allocated_anti_entropy_get_reply_msg(message_content);
}


void build_anti_entropy_get_metadata_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                const std::string& key, const kv_store_key_version& version, bool is_deleted){

    auto *message_content = new proto::anti_entropy_get_metadata_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);
    message_content->set_is_deleted(is_deleted);
    
    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);
    
    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();

    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }

    kv_key_version->set_client_id(version.client_id);

    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);
    
    msg->set_allocated_anti_entropy_get_met_msg(message_content);

}




void build_anti_entropy_get_metadata_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id,
                                const std::string& key, const kv_store_key_version& version, bool is_deleted, bool is_merge, std::unique_ptr<std::string> data){

    auto* message_content = new proto::anti_entropy_get_metadata_reply_message();
    message_content->set_ip(ip);
    message_content->set_port(kv_port);
    message_content->set_id(id);
    message_content->set_reqid(req_id);
    
    message_content->set_data(data->data(), data->size());

    proto::kv_store_key* kv_key = new proto::kv_store_key();
    kv_key->set_key(key);

    proto::kv_store_key_version* kv_key_version = new proto::kv_store_key_version();

    for(auto const c: version.vv){
        proto::kv_store_version *kv_version = kv_key_version->add_version();
        kv_version->set_client_id(c.first);
        kv_version->set_clock(c.second);
    }

    kv_key_version->set_client_id(version.client_id);

    kv_key->set_allocated_key_version(kv_key_version);

    message_content->set_allocated_key(kv_key);

    message_content->set_is_deleted(is_deleted);
    message_content->set_is_merge(is_merge);
    
    msg->set_allocated_anti_entropy_get_met_reply_msg(message_content);
}