//
// Created by danielsf97 on 3/16/20.
//

#include "client_reply_handler.h"



client_reply_handler::client_reply_handler(std::string ip, int kv_port, int pss_port, long wait_timeout):
        ip(std::move(ip)), kv_port(kv_port), pss_port(pss_port), wait_timeout(wait_timeout)
{
    this->put_replies.reserve(100);
    this->put_mutexes.reserve(100);
}

void client_reply_handler::register_put(const kv_store_key<std::string>& comp_key) {
    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    auto it = this->put_replies.find(comp_key);
    if(it == this->put_replies.end()){
        // key does not exist
        std::set<long> temp;
        this->put_replies.emplace(comp_key, std::move(temp));
        auto sync_pair = std::make_unique<std::pair<std::mutex, std::condition_variable>>();
        this->put_mutexes.emplace(std::move(comp_key), std::move(sync_pair));
    }else{
        throw ConcurrentWritesSameKeyException();
    }

    lock.unlock();
}

bool client_reply_handler::wait_for_put(const kv_store_key<std::string>& key, int wait_for){
    bool succeed = false;

    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->put_replies.find(key);
    if(it != this->put_replies.end()) {
        // if key exists, lock key
        auto &sync_pair = this->put_mutexes.find(key)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // if we still don't have the number of needed replies

            // unlock global put lock
            lock.unlock();
            //unlock for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            if(status == std::cv_status::timeout) timeout_happened = true;
            waited = true;
            // unlock key mutex, because we must get locks in the same order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verify if put has been performed with success
        if(waited) {
            it = this->put_replies.find(key);
        }
        succeed = it->second.size() >= wait_for;
        if (succeed) {
            // if put has already been successfully performed, since we have all locks
            // we can remove the key entries
            this->put_replies.erase(it);
            // We don't need to awake threads that can be strapped on cond variable
            // since it's certain that a single thread is able to wait for a same key
            auto it_key = this->put_mutexes.find(key);
            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();
            this->put_mutexes.erase(it_key);
        }
    }

    lock.unlock();

    if(!succeed && timeout_happened)
        throw TimeoutException();

    return succeed;
}

bool client_reply_handler::wait_for_put_until(const kv_store_key<std::string>& key, int wait_for, std::chrono::system_clock::time_point& wait_until){
    bool succeed = false;

    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    bool timeout_happened = false;
    auto it = this->put_replies.find(key);
    if(it != this->put_replies.end()) {

        // if key exists, lock key
        auto it_key = this->put_mutexes.find(key);

        // I already have the pointer to the replies and mutexes map so i can unlock because as
        // the only thread that can remove the key from the mais is the one itself, and the iterators
        // don't invalidate with other removals, it's certain that the pointers remain valid.
        lock.unlock();

        auto &sync_pair = it_key->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // if we still don't have the number of needed replies

            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_until(lock_key, wait_until);
            if(status == std::cv_status::timeout) timeout_happened = true;
        }

        // verify if put was performed with success, we already have the key lock
        succeed = it->second.size() >= wait_for;
        if (succeed) {

            // unlock key mutex, because we must get locks in the same order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();

            // if put jas already been sucessfully performed, as we still have the locks,
            // we can remove the key entries
            this->put_replies.erase(it);

            // We don't need to awake threads that can be strapped on cond variable
            // since it's certain that a single thread is able to wait for a same key
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            this->put_mutexes.erase(it_key);

            lock.unlock();
        }
    }

    if(!succeed && timeout_happened)
        throw TimeoutException();

    return succeed;
}

void client_reply_handler::clear_put_keys_entries(std::vector<kv_store_key<std::string>>& erasing_keys){
    std::scoped_lock<std::mutex> lock(this->put_global_mutex);
    for(auto& key : erasing_keys) {
        auto it = this->put_replies.find(key);
        if (it != this->put_replies.end()) {
            // if key exists, lock key
            auto it_key = this->put_mutexes.find(key);
            std::unique_lock<std::mutex> lock_key(it_key->second->first);
            this->put_replies.erase(it);
            lock_key.unlock();
            this->put_mutexes.erase(it_key);
        }
    }
}

void client_reply_handler::register_delete(const kv_store_key<std::string>& comp_key) {
    std::unique_lock<std::mutex> lock(this->delete_global_mutex);

    auto it = this->delete_replies.find(comp_key);
    if(it == this->delete_replies.end()){
        // key does not exist
        std::set<long> temp;
        this->delete_replies.emplace(comp_key, std::move(temp));
        auto sync_pair = std::make_unique<std::pair<std::mutex, std::condition_variable>>();
        this->delete_mutexes.emplace(std::move(comp_key), std::move(sync_pair));
    }else{
        throw ConcurrentWritesSameKeyException();
    }

    lock.unlock();
}


bool client_reply_handler::wait_for_delete(const kv_store_key<std::string>& key, int wait_for){
    bool succeed = false;

    std::unique_lock<std::mutex> lock(this->delete_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->delete_replies.find(key);
    if(it != this->delete_replies.end()) {
        // if key exists, lock key
        auto &sync_pair = this->delete_mutexes.find(key)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // if we still don't have the number of needed replies

            // unlock global put lock
            lock.unlock();
            //unlock for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            if(status == std::cv_status::timeout) timeout_happened = true;
            waited = true;
            // unlock key mutex, because we must get locks in the same order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verify if put has been performed with success
        if(waited) {
            it = this->delete_replies.find(key);
        }
        succeed = it->second.size() >= wait_for;
        if (succeed) {

            // if put has already been successfully performed, since we have all locks
            // we can remove the key entries
            this->delete_replies.erase(it);
            // We don't need to awake threads that can be strapped on cond variable
            // since it's certain that a single thread is able to wait for a same key
            auto it_key = this->delete_mutexes.find(key);
            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();
            this->delete_mutexes.erase(it_key);
        }
    }

    lock.unlock();

    if(!succeed && timeout_happened)
        throw TimeoutException();

    return succeed;
}



void client_reply_handler::register_get(const std::string& req_id) {
    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(req_id);
    if(it == this->get_replies.end()){
        // key exists
        get_Replies temp = {.keys = std::unordered_map<kv_store_key<std::string>, std::unique_ptr<std::string>> (), .deleted_keys = std::unordered_map<kv_store_key<std::string>, std::unique_ptr<std::string>> (),
                         .count = 0};
        this->get_replies.emplace(req_id, std::move(temp));
        auto sync_pair = std::make_unique<std::pair<std::mutex, std::condition_variable>>();
        this->get_mutexes.emplace(req_id, std::move(sync_pair));
    }else{
        //It's impossible for this to happen (register a same key)
    }

    lock.unlock();
}

void client_reply_handler::register_get_data(const std::string& req_id) {
    register_get(req_id);
}

void client_reply_handler::register_get_latest_version(const std::string& req_id) {
    register_get(req_id);
}

void client_reply_handler::clear_get_keys_entries(std::vector<std::string>& erasing_keys){
    std::scoped_lock<std::mutex> lock(this->get_global_mutex);
    for(auto& key : erasing_keys) {
        auto it = this->get_replies.find(key);
        if (it != this->get_replies.end()) {
            // key exists, lock key
            auto it_key = this->get_mutexes.find(key);
            std::unique_lock<std::mutex> lock_key(it_key->second->first);
            this->get_replies.erase(it);
            lock_key.unlock();
            this->get_mutexes.erase(it_key);
        }
    }
}

void client_reply_handler::change_get_reqid(const std::string &latest_reqid_str, const std::string &new_reqid) {
    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(latest_reqid_str);
    if(it != this->get_replies.end()){
        // key doesn't exist
        auto get_map_node = this->get_replies.extract(it);
        get_map_node.key() = new_reqid;
        this->get_replies.insert(std::move(get_map_node));
        auto get_mutexes_node = this->get_mutexes.extract(latest_reqid_str);
        get_mutexes_node.key() = new_reqid;
        this->get_mutexes.insert(std::move(get_mutexes_node));
    }else{
        //It's impossible for this to happen (register a same key)
        std::cout << "imposible" << std::endl;
    }

    lock.unlock();
}

std::unique_ptr<std::string> client_reply_handler::wait_for_get(const std::string& req_id, int wait_for, Response* get_res) {
    std::unique_ptr<std::string> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;
    bool waited = false;
    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        // if exists a entry for the key, lock that entry
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.count < wait_for) {
            // if we still don't have the number of needed replies

            // unlock global get lock
            lock.unlock();
            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            waited = true;
            if(status == std::cv_status::timeout) timeout_happened = true;
            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verify if we do already have a reply
        if(waited) {
            it = this->get_replies.find(req_id);
        }
        if(it->second.count >= wait_for){
            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key

            auto& map_keys = it->second.keys;
            auto& map_del_keys = it->second.deleted_keys;
        

            if(map_del_keys.size() > 0){
                *get_res = Response::Deleted;
            } else {
                if(map_keys.size() > 0){
                    res = std::move(map_keys.begin()->second);
                    *get_res = Response::Ok;
                } else 
                    *get_res = Response::NoData;
            }

            this->get_replies.erase(it);

            // We don't need to awake threads that can be strapped on cond variable
            // since it's certain that a single thread is able to wait for a same key
            auto it_req_id = this->get_mutexes.find(req_id);
            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();
            this->get_mutexes.erase(it_req_id);
        }
    }

    lock.unlock();

    if(*get_res == Response::Init && timeout_happened){
        throw TimeoutException();
    }

    return res;
}




std::unique_ptr<std::string> client_reply_handler::wait_for_get_until(const std::string &req_id, int wait_for,
                                                                      std::chrono::system_clock::time_point &wait_until, Response* get_res){
    std::unique_ptr<std::string> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        // if exists a entry for the key, lock that entry
        auto it_key = this->get_mutexes.find(req_id);

        // I already have the pointer to the replies and mutexes map so i can unlock because as
        // the only thread that can remove the key from the mais is the one itself, and the iterators
        // don't invalidate with other removals, it's certain that the pointers remain valid.
        lock.unlock();

        auto &sync_pair = it_key->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.count < wait_for) {
            // if we still don't have the necessary repies

            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_until(lock_key, wait_until);
            if(status == std::cv_status::timeout) timeout_happened = true;
        }

        if(it->second.count >= wait_for){

            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            
            auto& map_keys = it->second.keys;
            auto& map_del_keys = it->second.deleted_keys;
        

            if(map_del_keys.size() > 0){
                *get_res = Response::Deleted;
            } else {
                if(map_keys.size() > 0){
                    res = std::move(map_keys.begin()->second);
                    *get_res = Response::Ok;
                } else 
                    *get_res = Response::NoData;
            }


            // unlock key mutex because we have to get locks in order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();

            this->get_replies.erase(it);

            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();

            this->get_mutexes.erase(it_key);

            lock.unlock();
        }
    }

    if(*get_res == Response::Init && timeout_happened){
        throw TimeoutException();
    }
    return res;
}

std::unique_ptr<kv_store_key_version> client_reply_handler::wait_for_get_latest_version(const std::string& req_id, int wait_for, Response* get_res) {
    std::unique_ptr<kv_store_key_version> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //if key exists, lock key
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.count < wait_for) {
            // if we still don't have the number of needed replies

            // unlock global get lock
            lock.unlock();
            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            waited = true;
            if(status == std::cv_status::timeout) timeout_happened = true;
            // unlock key mutex because we have to get locks in order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verify if we already have all replies
        if(waited){
            it = this->get_replies.find(req_id);
        }
        if(it->second.count >= wait_for){
            //std::cout << "Recebi todas as respostas que queria" << std::endl;
            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            
            auto& map_keys = it->second.keys;
            auto& map_del_keys = it->second.deleted_keys;

            std::vector<kv_store_key_version> max_vv;

            for(auto& kv : map_keys){
                bool deleted = false;
                for(auto& kv_del : map_del_keys){
                    kVersionComp comp_del = comp_version(kv.first.key_version, kv_del.first.key_version);
                    
                    if(comp_del == kVersionComp::Lower || comp_del == kVersionComp::Equal){
                        deleted = true;
                        break;
                    }
                }
                if(!deleted){
                    int cout_concurrent = 0;
                    for(int i = 0; i < max_vv.size(); i++){
                        kVersionComp comp_max = comp_version(kv.first.key_version, max_vv.at(i));
                        if(comp_max == kVersionComp::Bigger){
                            max_vv.at(i) = kv.first.key_version;
                            break;
                        } else if(comp_max == kVersionComp::Concurrent)
                            cout_concurrent++;
                    }
                    if(cout_concurrent == max_vv.size())
                        max_vv.push_back(kv.first.key_version);
                }
            }

            kv_store_key_version last_v;
            if(max_vv.size() > 1)
                last_v = choose_latest_version(max_vv);
            else if(max_vv.size() == 1)
                last_v = max_vv.front();
             
            if(!max_vv.empty()){
                res = std::make_unique<kv_store_key_version>(last_v);
                *get_res = Response::Ok;
             }else if(!it->second.deleted_keys.empty()){
                *get_res = Response::Deleted;
            }else{
                *get_res = Response::NoData;
            }

            this->get_replies.erase(it);

            // We don't need to awake threads that can be strapped on cond variable
            // since it's certain that a single thread is able to wait for a same key
            auto it_req_id = this->get_mutexes.find(req_id);
            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();
            this->get_mutexes.erase(it_req_id);
        }
    }

    lock.unlock();

    if(res == nullptr && timeout_happened)
        throw TimeoutException();

    return std::move(res);
}

std::unique_ptr<std::string> client_reply_handler::wait_for_get_latest_until(const std::string &key, const std::string &req_id, int wait_for,
                                                                      std::chrono::system_clock::time_point &wait_until, Response* get_res){
    std::unique_ptr<std::string> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        // if exists a entry for the key, lock that entry
        auto it_key = this->get_mutexes.find(req_id);

        // I already have the pointer to the replies and mutexes map so i can unlock because as
        // the only thread that can remove the key from the mais is the one itself, and the iterators
        // don't invalidate with other removals, it's certain that the pointers remain valid.

        auto &sync_pair = it_key->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        lock.unlock();

        if (it->second.count < wait_for) {
            // if we still don't have the necessary repies

            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_until(lock_key, wait_until);
            if(status == std::cv_status::timeout) timeout_happened = true;
        }
        if(it->second.count >= wait_for){      
            //std::cout << "Recebi todas as respostas que queria" << std::endl;
            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            
            auto& map_keys = it->second.keys;
            auto& map_del_keys = it->second.deleted_keys;

            std::vector<kv_store_key_version> max_vv;

            for(auto& kv : map_keys){
                bool deleted = false;
                for(auto& kv_del : map_del_keys){
                    kVersionComp comp_del = comp_version(kv.first.key_version, kv_del.first.key_version);
                    
                    if(comp_del == kVersionComp::Lower || comp_del == kVersionComp::Equal){
                        deleted = true;
                        break;
                    }
                }
                if(!deleted){
                    int cout_concurrent = 0;
                    for(int i = 0; i < max_vv.size(); i++){
                        kVersionComp comp_max = comp_version(kv.first.key_version, max_vv.at(i));
                        if(comp_max == kVersionComp::Bigger){
                            max_vv.at(i) = kv.first.key_version;
                            break;
                        } else if(comp_max == kVersionComp::Concurrent)
                            cout_concurrent++;
                    }
                    if(cout_concurrent == max_vv.size())
                        max_vv.push_back(kv.first.key_version);
                }
            }

            kv_store_key_version last_v;
            if(max_vv.size() > 1)
                last_v = choose_latest_version(max_vv);
            else if(max_vv.size() == 1)
                last_v = max_vv.front();
            
            if(!max_vv.empty()){
                auto it = map_keys.find({key, last_v, false});
                if(it != map_keys.end()){
                    res = std::move(it->second);
                    *get_res = Response::Ok;
                }else{
                    *get_res = Response::NoData;
                }
            }else if(!it->second.deleted_keys.empty()){
                *get_res = Response::Deleted;
            }else{
                it->second.count--;
                lock_key.unlock();
                throw TimeoutException();
            }
            

            // unlock key mutex because we have to get locks in order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();

            this->get_replies.erase(it);

            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();

            this->get_mutexes.erase(it_key);

            lock.unlock();
        }
    }

    if(*get_res == Response::Init && timeout_happened)
        throw TimeoutException();
    return res;
}


std::unique_ptr<std::string> client_reply_handler::wait_for_get_latest(const std::string &key, const std::string& req_id, int wait_for, Response* get_res, kv_store_key_version* last_version) {
    std::unique_ptr<std::string> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //if key exists, lock key
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.count < wait_for) {
            // if we still don't have the number of needed replies

            // unlock global get lock
            lock.unlock();
            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            waited = true;
            if(status == std::cv_status::timeout) timeout_happened = true;
            // unlock key mutex because we have to get locks in order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verify if we already have all replies
        if(waited){
            it = this->get_replies.find(req_id);
        }
        if(it->second.count >= wait_for){
            //std::cout << "Recebi todas as respostas que queria" << std::endl;
            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            
            auto& map_keys = it->second.keys;
            auto& map_del_keys = it->second.deleted_keys;

            std::vector<kv_store_key_version> max_vv;

            for(auto& kv : map_keys){
                bool deleted = false;
                for(auto& kv_del : map_del_keys){
                    kVersionComp comp_del = comp_version(kv.first.key_version, kv_del.first.key_version);
                    
                    if(comp_del == kVersionComp::Lower || comp_del == kVersionComp::Equal){
                        deleted = true;
                        break;
                    }
                }
                if(!deleted){
                    int cout_concurrent = 0;
                    for(int i = 0; i < max_vv.size(); i++){
                        kVersionComp comp_max = comp_version(kv.first.key_version, max_vv.at(i));
                        if(comp_max == kVersionComp::Bigger){
                            max_vv.at(i) = kv.first.key_version;
                            break;
                        } else if(comp_max == kVersionComp::Concurrent)
                            cout_concurrent++;
                    }
                    if(cout_concurrent == max_vv.size())
                        max_vv.push_back(kv.first.key_version);
                }
            }

            kv_store_key_version last_v;
            if(max_vv.size() > 1)
                last_v = choose_latest_version(max_vv);
            else if(max_vv.size() == 1)
                last_v = max_vv.front();
            

            if(!max_vv.empty()){
                auto it = map_keys.find({key, last_v, false});
                if(it != map_keys.end()){
                    res = std::move(it->second);
                    *get_res = Response::Ok;
                    *last_version = last_v;
                }else{
                    *get_res = Response::NoData;
                }
            }else if(!it->second.deleted_keys.empty()){
                *get_res = Response::Deleted;
            }else{
                *get_res = Response::NoData;
            }

            this->get_replies.erase(it);

            // We don't need to awake threads that can be strapped on cond variable
            // since it's certain that a single thread is able to wait for a same key
            auto it_req_id = this->get_mutexes.find(req_id);
            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();
            this->get_mutexes.erase(it_req_id);
        }
    }

    lock.unlock();

    if(res == nullptr && timeout_happened)
        throw TimeoutException();

    return std::move(res);
}


std::vector<std::unique_ptr<std::string>> client_reply_handler::wait_for_get_latest_concurrent(const std::string &key, const std::string& req_id, int wait_for, Response* get_res) {
    std::vector<std::unique_ptr<std::string>> res;

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //if key exists, lock key
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.count < wait_for) {
            // if we still don't have the number of needed replies

            // unlock global get lock
            lock.unlock();
            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            waited = true;
            if(status == std::cv_status::timeout) timeout_happened = true;
            // unlock key mutex because we have to get locks in order
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verify if we already have all replies
        if(waited){
            it = this->get_replies.find(req_id);
        }
        if(it->second.count >= wait_for){
            //std::cout << "Recebi todas as respostas que queria" << std::endl;
            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            
            auto& map_keys = it->second.keys;
            auto& map_del_keys = it->second.deleted_keys;

            std::vector<kv_store_key_version> max_vv;

            for(auto& kv : map_keys){
                bool deleted = false;
                for(auto& kv_del : map_del_keys){
                    kVersionComp comp_del = comp_version(kv.first.key_version, kv_del.first.key_version);
                    
                    if(comp_del == kVersionComp::Lower || comp_del == kVersionComp::Equal){
                        deleted = true;
                        break;
                    }
                }
                if(!deleted){
                    int cout_concurrent = 0;
                    for(int i = 0; i < max_vv.size(); i++){
                        kVersionComp comp_max = comp_version(kv.first.key_version, max_vv.at(i));
                        if(comp_max == kVersionComp::Bigger){
                            max_vv.at(i) = kv.first.key_version;
                            res.at(i) = std::move(kv.second);
                            break;
                        } else if(comp_max == kVersionComp::Concurrent)
                            cout_concurrent++;
                    }
                    if(cout_concurrent == max_vv.size())
                        max_vv.push_back(kv.first.key_version);
                        res.push_back(std::move(kv.second));
                }
            }

            bool maxEmpty = max_vv.empty();

            if(maxEmpty && !it->second.deleted_keys.empty()){
                *get_res = Response::Deleted;
            }else if(maxEmpty){
                *get_res = Response::NoData;
            }else{
                *get_res = Response::Ok;
            }

            this->get_replies.erase(it);

            // We don't need to awake threads that can be strapped on cond variable
            // since it's certain that a single thread is able to wait for a same key
            auto it_req_id = this->get_mutexes.find(req_id);
            // It's strictly needes to free key lock as if we don't, when leaving scope it will
            // try to perform a release on a non-existent mutex (SIGSEV)
            lock_key.unlock();
            this->get_mutexes.erase(it_req_id);
        }
    }

    lock.unlock();

    if(*get_res == Response::Init && timeout_happened)
        throw TimeoutException();

    return std::move(res);
}



void client_reply_handler::process_get_reply_msg(const proto::get_reply_message &msg) {
    const std::string& req_id = msg.reqid();
                    
    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    boost::regex composite_key(".+:(\\d+)$");
    boost::cmatch match;
    auto res = boost::regex_search(req_id.c_str(), match, composite_key);

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()){
        
        auto& sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> reqid_lock(sync_pair->first);
        lock.unlock(); //free global get lock

        const bool is_deleted = msg.version_is_deleted();

        std::unique_ptr<std::string> data(nullptr);
        if(!is_deleted){
            data = std::make_unique<std::string>(msg.data());
        }

        kv_store_key_version version;
        for (auto c : msg.key().key_version().version())
            version.vv.emplace(c.client_id(), c.clock());
        version.client_id = msg.key().key_version().client_id();

        kv_store_key<std::string> st_key = {msg.key().key(), version, is_deleted};

        if(!is_deleted)
            it->second.keys.insert(std::make_pair(st_key, std::move(data)));
        else
            it->second.deleted_keys.insert(std::make_pair(st_key, nullptr));
        
        it->second.count++;

        sync_pair->second.notify_all();
        reqid_lock.unlock();
    }else{
        spdlog::debug("GET REPLY IGNORED - NON EXISTENT KEY");
    }
}

void client_reply_handler::process_get_latest_version_reply_msg(const proto::get_latest_version_reply_message& msg) {
    const std::string& req_id = msg.reqid();

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    boost::regex composite_key(".+:(\\d+)$");
    boost::cmatch match;
    auto res = boost::regex_search(req_id.c_str(), match, composite_key);

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()){
        
        auto& sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> reqid_lock(sync_pair->first);
        lock.unlock(); //free global get lock

        for(auto k : msg.last_v()){
            kv_store_key_version kv;
            for (auto c : k.version()){
                kv.vv.emplace(c.client_id(), c.clock());
            }
            kv.client_id = k.client_id();
            
            kv_store_key<std::string> st_key = {msg.key(), kv, false};
            
            std::unique_ptr<std::string> data(nullptr);
            if(!k.data().empty())
                data = std::make_unique<std::string>(k.data());
            
            it->second.keys.insert(std::make_pair(st_key, std::move(data)));
        }

       for(auto k : msg.last_deleted_v()){
            kv_store_key_version kv_del;
            for (auto c : k.version()){
                kv_del.vv.emplace(c.client_id(), c.clock());
            }
            kv_del.client_id = k.client_id();

            kv_store_key<std::string> st_del_key = {msg.key(), kv_del, true};
            it->second.deleted_keys.insert(std::make_pair(st_del_key, nullptr));
        }

        it->second.count++;

        sync_pair->second.notify_all();
        reqid_lock.unlock();

    }else{
        spdlog::debug("GET REPLY IGNORED - NON EXISTENT KEY");
    }
}

void client_reply_handler::process_put_reply_msg(const proto::put_reply_message &msg) {
    const std::string& key = msg.key().key();
    kv_store_key_version version;
    for (auto c : msg.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = msg.key().key_version().client_id();

    bool is_merge = msg.is_merge();

    kv_store_key<std::string> comp_key = {key, version, false, is_merge};

    std::unique_lock<std::mutex> lock(this->put_global_mutex);
    auto it = this->put_replies.find(comp_key);
    if(it != this->put_replies.end()){
        
        auto& sync_pair = this->put_mutexes.find(comp_key)->second;
        std::unique_lock<std::mutex> key_lock(sync_pair->first);

        lock.unlock(); //free global put lock

        long replier_id = msg.id();

        it->second.emplace(replier_id);
        sync_pair->second.notify_all();
        key_lock.unlock();
    }else{
        spdlog::debug("PUT REPLY IGNORED - NON EXISTENT KEY");
    }
}

void client_reply_handler::process_delete_reply_msg(const proto::delete_reply_message &msg) {
    const std::string& key = msg.key().key();
    kv_store_key_version version;
    for (auto c : msg.key().key_version().version())
        version.vv.emplace(c.client_id(), c.clock());
    version.client_id = msg.key().key_version().client_id();

    kv_store_key<std::string> comp_key = {key, version, true};
    long replier_id = msg.id();

    std::unique_lock<std::mutex> lock(this->delete_global_mutex);

    auto it = this->delete_replies.find(comp_key);
    if(it != this->delete_replies.end()){
        
        auto& sync_pair = this->delete_mutexes.find(comp_key)->second;
        std::unique_lock<std::mutex> key_lock(sync_pair->first);

        lock.unlock(); //free global put lock

        it->second.emplace(replier_id);
        sync_pair->second.notify_all();
        key_lock.unlock();
    }else{
        spdlog::debug("DELETE REPLY IGNORED - NON EXISTENT KEY");
    }
}