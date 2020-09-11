//
// Created by danielsf97 on 3/16/20.
//

#include "client_reply_handler.h"
#include "exceptions/custom_exceptions.h"
#include "df_store/kv_store_key.h"
#include <boost/regex.hpp>
#include <utility>
#include <spdlog/spdlog.h>


client_reply_handler::client_reply_handler(std::string ip, long wait_timeout):
        ip(std::move(ip)), wait_timeout(wait_timeout)
{
    this->put_replies.reserve(100);
    this->put_mutexes.reserve(100);
}

long client_reply_handler::register_put(const std::string& key, long version) {
    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    kv_store_key<std::string> comp_key = {key, kv_store_key_version(version)};

    auto it = this->put_replies.find(comp_key);
    if(it == this->put_replies.end()){
        // a chave não existe
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
        // se a chave existe, fazer lock da chave
        auto &sync_pair = this->put_mutexes.find(key)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // caso ainda não tenhamos o número de respostas necessárias

            // fazer unlock do global put lock
            lock.unlock();
            // esperar por um notify na chave -> ele faz lock automatico da chave
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            if(status == std::cv_status::timeout) timeout_happened = true;
            waited = true;
            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verificar se o put já foi realizado com sucesso
        if(waited) {
            it = this->put_replies.find(key);
        }
        succeed = it->second.size() >= wait_for;
        if (succeed) {

            // se o put já foi realizado com sucesso, como ainda temos os locks
            // podemos remover as entradas para a chave
            this->put_replies.erase(it);

            // não é necessário acordar possiveis threads presas na cond variable
            // porque é certo que apenas uma thread podes estar à espera de uma mesma key
            auto it_key = this->put_mutexes.find(key);
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // como sai do scope ele vai tentar faazer free num mutex inexistente (SIGSEV)
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
        // se a chave existe, fazer lock da chave
        auto it_key = this->put_mutexes.find(key);

        // Já tenho o apontador para o map das replies e dos mutexes logo posso fazer unlock
        // que como a única thread que pode tirar a chave do mapa é a própria, e os iteradores
        // não se invalidam com outras remoções, é certo que os apontadores permanecem válidos
        lock.unlock();

        auto &sync_pair = it_key->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // caso ainda não tenhamos o número de respostas necessárias

            // esperar por um notify na chave -> ele faz lock automatico da chave
            std::cv_status status = sync_pair->second.wait_until(lock_key, wait_until);
            if(status == std::cv_status::timeout) timeout_happened = true;
        }

        // verificar se o put já foi realizado com sucesso, já temos o lock da chave
        succeed = it->second.size() >= wait_for;
        if (succeed) {

            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();

            // se o put já foi realizado com sucesso, como ainda temos os locks
            // podemos remover as entradas para a chave
            this->put_replies.erase(it);

            // não é necessário acordar possiveis threads presas na cond variable
            // porque é certo que apenas uma thread podes estar à espera de uma mesma key
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // como sai do scope ele vai tentar fazer free num mutex inexistente (SIGSEV)
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
            // se a chave existe, fazer lock da chave
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
        // a chave não existe
        std::vector<std::pair<kv_store_key_version, std::unique_ptr<std::string>>> temp;
        this->get_replies.emplace(req_id, std::move(temp));
        auto sync_pair = std::make_unique<std::pair<std::mutex, std::condition_variable>>();
        this->get_mutexes.emplace(req_id, std::move(sync_pair));
    }else{
        //É impossível isto acontecer (registar uma mesma key)
    }

    lock.unlock();
}

long client_reply_handler::change_get_reqid(const std::string &latest_reqid_str, const std::string &new_reqid) {
    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(latest_reqid_str);
    if(it != this->get_replies.end()){
        // a chave não existe
        auto get_map_node = this->get_replies.extract(it);
        get_map_node.key() = new_reqid;
        this->get_replies.insert(std::move(get_map_node));
        auto get_mutexes_node = this->get_mutexes.extract(latest_reqid_str);
        get_mutexes_node.key() = new_reqid;
        this->get_mutexes.insert(std::move(get_mutexes_node));
    }else{
        //É impossível isto acontecer (registar uma mesma key)
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
        //se existe entrada para a chave, fazer lock dessa entrada
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // caso ainda não tenhamos o número de respostas necessárias

            // fazer unlock do global get lock
            lock.unlock();
            // esperar por um notify na chave -> ele faz lock automatico da chave
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            waited = true;
            if(status == std::cv_status::timeout) timeout_happened = true;
            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verificar se já temos a resposta
        if(waited) {
            it = this->get_replies.find(req_id);
        }
        if(it->second.size() >= wait_for){

            // se já temos uma maioria de resposta, como ainda temos os locks
            // podemos remover as entradas para a chave
            auto max_version = kv_store_key_version(-1);
            for(auto& entry : it->second){
                if(entry.first > max_version){
                    max_version = entry.first;
                    res = std::move(entry.second);
                }
            }
            this->get_replies.erase(it);

            // não é necessário acordar possiveis threads presas na cond variable
            // porque é certo que apenas uma thread podes estar à espera de uma mesma key
            auto it_req_id = this->get_mutexes.find(req_id);
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // como sai do scope ele vai tentar faazer free num mutex inexistente (SIGSEV)
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
        //se existe entrada para a chave, fazer lock dessa entrada
        auto it_key = this->get_mutexes.find(req_id);

        // Já tenho o apontador para o map das replies e dos mutexes logo posso fazer unlock
        // que como a única thread que pode tirar a chave do mapa é a própria, e os iteradores
        // não se invalidam com outras remoções, é certo que os apontadores permanecem válidos
        lock.unlock();

        auto &sync_pair = it_key->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // caso ainda não tenhamos o número de respostas necessárias

            // esperar por um notify na chave -> ele faz lock automatico da chave
            std::cv_status status = sync_pair->second.wait_until(lock_key, wait_until);
            if(status == std::cv_status::timeout) timeout_happened = true;
        }

        if(it->second.size() >= wait_for){

            // se já temos uma maioria de resposta, como ainda temos os locks
            // podemos remover as entradas para a chave
            auto max_version = kv_store_key_version(-1);
            for(auto& entry : it->second){
                if(entry.first > max_version){
                    max_version = entry.first;
                    res = std::move(entry.second);
                }
            }

            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();

            this->get_replies.erase(it);

            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // como sai do scope ele vai tentar faazer free num mutex inexistente (SIGSEV)
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
            // se a chave existe, fazer lock da chave
            auto it_key = this->get_mutexes.find(key);
            std::unique_lock<std::mutex> lock_key(it_key->second->first);
            this->get_replies.erase(it);
            lock_key.unlock();
            this->get_mutexes.erase(it_key);
        }
    }
}

void client_reply_handler::register_get_latest_version(const std::string& req_id) {
    // são utilizadas as mesmas estruturas que para os gets
    register_get(req_id);
}

std::unique_ptr<long> client_reply_handler::wait_for_get_latest_version(const std::string& req_id, int wait_for) {
    std::unique_ptr<long> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    bool timeout_happened = false;
    bool waited = false;

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //se existe entrada para a chave, fazer lock dessa entrada
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(sync_pair->first);

        if (it->second.size() < wait_for) {
            // caso ainda não tenhamos o número de respostas necessárias

            // fazer unlock do global get lock
            lock.unlock();
            // esperar por um notify na chave -> ele faz lock automatico da chave
            std::cv_status status = sync_pair->second.wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            waited = true;
            if(status == std::cv_status::timeout) timeout_happened = true;
            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verificar se já temos a resposta
        if(waited){
            it = this->get_replies.find(req_id);
        }
        if(it->second.size() >= wait_for){

            // se já temos uma maioria de resposta, como ainda temos os locks
            // podemos remover as entradas para a chave
            long max_version = -1;
            for(auto& entry : it->second){
                if(entry.first.version > max_version){
                    max_version = entry.first.version;
                }
            }
            res = std::make_unique<long>(max_version);
            this->get_replies.erase(it);

            // não é necessário acordar possiveis threads presas na cond variable
            // porque é certo que apenas uma thread podes estar à espera de uma mesma key
            auto it_req_id = this->get_mutexes.find(req_id);
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // como sai do scope ele vai tentar faazer free num mutex inexistente (SIGSEV)
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
//    spdlog::debug("<============================== GET " + std::to_string(std::stol(match[1].str(), nullptr)));

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()){
        // a chave existe
        auto& sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> reqid_lock(sync_pair->first);
        lock.unlock(); //free global get lock

        it->second.emplace_back(std::make_pair<kv_store_key_version, std::unique_ptr<std::string>>(
                kv_store_key_version(msg.version(), msg.version_client_id()),
                std::make_unique<std::string>(data)
        ));
        sync_pair->second.notify_all();
        reqid_lock.unlock();
    }else{
        spdlog::debug("GET REPLY IGNORED - NON EXISTENT KEY");
    }
}

void client_reply_handler::process_put_reply_msg(const proto::put_reply_message &msg) {
    const std::string& key = msg.key();
    long version = msg.version();
    kv_store_key<std::string> comp_key = {key, kv_store_key_version(version)};
    long replier_id = msg.id();

    std::unique_lock<std::mutex> lock(this->put_global_mutex);

//    spdlog::debug("<============================== PUT " + key + " : " + std::to_string(version));

    auto it = this->put_replies.find(comp_key);
    if(it != this->put_replies.end()){
        // a chave existe
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

//    spdlog::debug("<============================== GET " + std::to_string(std::stol(match[1].str(), nullptr)));

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()){
        // a chave existe
        auto& sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> reqid_lock(sync_pair->first);
        lock.unlock(); //free global get lock

        it->second.emplace_back(std::make_pair<kv_store_key_version, std::unique_ptr<std::string>>(
                kv_store_key_version(msg.version()),
                nullptr
        ));
        sync_pair->second.notify_all();
        reqid_lock.unlock();
    }else{
        spdlog::debug("GET REPLY IGNORED - NON EXISTENT KEY");
    }
}