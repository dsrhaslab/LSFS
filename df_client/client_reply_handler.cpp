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

std::map<long, long> client_reply_handler::register_put(const std::string& key, std::map<long, long> version) {
    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    kv_store_key<std::string> comp_key = {key, kv_store_key_version(version)};

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
    return version;
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

void client_reply_handler::register_get(const std::string& req_id) {
    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(req_id);
    if(it == this->get_replies.end()){
        // key exists
        std::vector<std::pair<kv_store_key_version, std::unique_ptr<std::string>>> temp;
        this->get_replies.emplace(req_id, std::move(temp));
        auto sync_pair = std::make_unique<std::pair<std::mutex, std::condition_variable>>();
        this->get_mutexes.emplace(req_id, std::move(sync_pair));
    }else{
        //It's impossible for this to happen (register a same key)
    }

    lock.unlock();
}

long client_reply_handler::change_get_reqid(const std::string &latest_reqid_str, const std::string &new_reqid) {
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
    }

    lock.unlock();
}

std::unique_ptr<std::string> client_reply_handler::wait_for_get(const std::string& req_id, int wait_for) {
    std::unique_ptr<std::string> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        // if exists a entry for the key, lock that entry
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
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
        if(it->second.size() >= wait_for){

            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            kv_store_key_version max_version;
            for(auto& entry : it->second){
                if(entry.first > max_version){
                    max_version = entry.first;
                    res = std::move(entry.second);
                }
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

    return res;
}

std::unique_ptr<std::string> client_reply_handler::wait_for_get_until(const std::string &req_id, int wait_for,
                                                                      std::chrono::system_clock::time_point &wait_until){
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

        if (it->second.size() < wait_for) {
            // if we still don't have the necessary repies

            // wait for key notify -> it performs automatic lock of the key
            std::cv_status status = sync_pair->second.wait_until(lock_key, wait_until);
            if(status == std::cv_status::timeout) timeout_happened = true;
        }

        if(it->second.size() >= wait_for){

            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            kv_store_key_version max_version;
            for(auto& entry : it->second){
                if(entry.first > max_version){
                    max_version = entry.first;
                    res = std::move(entry.second);
                }
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

    if(res == nullptr && timeout_happened)
        throw TimeoutException();

    return res;
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

void client_reply_handler::register_get_latest_version(const std::string& req_id) {
    // are used the same structures as for the gets
    register_get(req_id);
}

std::unique_ptr<kv_store_key_version> client_reply_handler::wait_for_get_latest_version(const std::string& req_id, int wait_for) {
    std::unique_ptr<kv_store_key_version> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //if key exists, lock key
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
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
        if(it->second.size() >= wait_for){

            // if we already have the majority of replies, as we still hold the locks
            // we can remove the entries for the key
            kv_store_key_version max_version;
            for(auto& entry : it->second){
                if(entry.first > max_version){
                    max_version = entry.first;
                }
            }
            res = std::make_unique<kv_store_key_version>(max_version);
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

void client_reply_handler::process_get_reply_msg(const proto::get_reply_message &msg) {
    const std::string& req_id = msg.reqid();
    const std::string& data = msg.data();

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    boost::regex composite_key(".+:(\\d+)$");
    boost::cmatch match;
    auto res = boost::regex_search(req_id.c_str(), match, composite_key);

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()){
        // key exists
        auto& sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> reqid_lock(sync_pair->first);
        lock.unlock(); //free global get lock

        std::map<long, long> vector;
        for (auto c : msg.version())
            vector.emplace(c.client_id(), c.clock());


        it->second.emplace_back(std::make_pair<kv_store_key_version, std::unique_ptr<std::string>>(
                kv_store_key_version(vector), std::make_unique<std::string>(data)
        ));
        sync_pair->second.notify_all();
        reqid_lock.unlock();
    }else{
        spdlog::debug("GET REPLY IGNORED - NON EXISTENT KEY");
    }
}

void client_reply_handler::process_put_reply_msg(const proto::put_reply_message &msg) {
    const std::string& key = msg.key();
    kv_store_key_version version;
    for (auto c : msg.version())
        version.vv.emplace(c.client_id(), c.clock());

    kv_store_key<std::string> comp_key = {key, version};
    long replier_id = msg.id();

    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    auto it = this->put_replies.find(comp_key);
    if(it != this->put_replies.end()){
        // key exists
        auto& sync_pair = this->put_mutexes.find(comp_key)->second;
        std::unique_lock<std::mutex> key_lock(sync_pair->first);

        lock.unlock(); //free global put lock

        it->second.emplace(replier_id);
        sync_pair->second.notify_all();
        key_lock.unlock();
    }else{
        spdlog::debug("PUT REPLY IGNORED - NON EXISTENT KEY");
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
        // a chave existe
        auto& sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> reqid_lock(sync_pair->first);
        lock.unlock(); //free global get lock

        std::map<long, long> vector;
        for (auto c : msg.version())
            vector.emplace(c.client_id(), c.clock());


        it->second.emplace_back(std::make_pair<kv_store_key_version, std::unique_ptr<std::string>>(
                kv_store_key_version(vector),
                nullptr
        ));
        sync_pair->second.notify_all();
        reqid_lock.unlock();
    }else{
        spdlog::debug("GET REPLY IGNORED - NON EXISTENT KEY");
    }
}