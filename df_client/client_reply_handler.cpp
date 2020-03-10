//
// Created by danielsf97 on 1/27/20.
//

#include "client_reply_handler.h"
#include "../kv_message.pb.h"
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include "../exceptions/custom_exceptions.h"
#include <regex>

client_reply_handler::client_reply_handler(std::string ip, int port, int nr_puts_required, long wait_timeout):
    ip(ip), port(port), nr_puts_required(nr_puts_required), wait_timeout(wait_timeout), socket_rcv(socket(PF_INET, SOCK_DGRAM, 0))
{}

client_reply_handler::~client_reply_handler()
{
    close(this->socket_rcv);
}

void client_reply_handler::operator()() {
    this->running = true;
    std::cout << "Client Reply Handler is Active!!" << std::endl;

    struct sockaddr_in si_me, si_other;
    socklen_t addr_size = sizeof(si_other);

    memset(&si_me, '\0', sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(this->port);
    si_me.sin_addr.s_addr = inet_addr(this->ip.c_str());

    bind(this->socket_rcv, (struct sockaddr*)&si_me, sizeof(si_me));
    char buf [65500];

    while(this->running){
        int bytes_rcv = recvfrom(this->socket_rcv, buf, 65500, 0, (struct sockaddr*)& si_other, &addr_size);

        if(this->running){
            try {
                proto::kv_message message;
                message.ParseFromArray(buf, bytes_rcv);
                if(message.has_get_reply_msg()){

                    const proto::get_reply_message& msg = message.get_reply_msg();
                    std::string req_id = msg.reqid();
                    long replier_id = msg.id();
                    std::string data = msg.data();

                    std::unique_lock<std::mutex> lock(this->get_global_mutex);

                    std::regex composite_key(".+:(\\d+)$");
                    std::smatch match;
                    auto res = std::regex_search(req_id, match, composite_key);
                    std::cout << "<==============================" << " GET " << std::stol(match[1].str(), nullptr) << std::endl;

                    auto it = this->get_replies.find(req_id);
                    if(it != this->get_replies.end()){
                        // a chave existe
                        auto& sync_pair = this->get_mutexes.find(req_id)->second;
                        std::unique_lock<std::mutex> reqid_lock(*sync_pair.first);
                        lock.unlock(); //free global get lock

                        it->second.push_back(std::make_pair<long, std::shared_ptr<std::string>>(msg.version(), std::make_shared<std::string>(data)));
                        sync_pair.second->notify_all();
                        reqid_lock.unlock();
                    }else{
                        std::cout << "GET REPLY IGNORED - NON EXISTENT KEY" << std::endl;
                    }
                }else if(message.has_put_reply_msg()){

                    const proto::put_reply_message& msg = message.put_reply_msg();
                    std::string key = msg.key();
                    long version = msg.version();
                    kv_store_key<std::string> comp_key = {key, version};
                    long replier_id = msg.id();

                    std::unique_lock<std::mutex> lock(this->put_global_mutex);

                    std::cout << "<==============================" << " PUT " << key << " : " << version << std::endl;

                    auto it = this->put_replies.find(comp_key);
                    if(it != this->put_replies.end()){
                        // a chave existe
                        auto& sync_pair = this->put_mutexes.find(comp_key)->second;
                        std::unique_lock<std::mutex> key_lock(*sync_pair.first);

                        lock.unlock(); //free global put lock

                        it->second.emplace(replier_id);
                        sync_pair.second->notify_all();
                        key_lock.unlock();
                    }else{
                        std::cout << "PUT REPLY IGNORED - NON EXISTENT KEY" << std::endl;
                    }
                }else if(message.has_get_latest_version_reply_msg()){

                    const proto::get_latest_version_reply_message& msg = message.get_latest_version_reply_msg();
                    std::string req_id = msg.reqid();

                    std::unique_lock<std::mutex> lock(this->get_global_mutex);

                    std::regex composite_key(".+:(\\d+)$");
                    std::smatch match;
                    auto res = std::regex_search(req_id, match, composite_key);
                    std::cout << "<==============================" << " GET " << std::stol(match[1].str(), nullptr) << std::endl;

                    auto it = this->get_replies.find(req_id);
                    if(it != this->get_replies.end()){
                        // a chave existe
                        auto& sync_pair = this->get_mutexes.find(req_id)->second;
                        std::unique_lock<std::mutex> reqid_lock(*sync_pair.first);
                        lock.unlock(); //free global get lock

                        it->second.push_back(std::make_pair<long, std::shared_ptr<std::string>>(msg.version(), nullptr));
                        sync_pair.second->notify_all();
                        reqid_lock.unlock();
                    }else{
                        std::cout << "GET REPLY IGNORED - NON EXISTENT KEY" << std::endl;
                    }
                }
            }
            catch(const char* e){
                std::cerr << e << std::endl;
            }
            catch(...){}
        }
    }
}

long client_reply_handler::register_put(std::string key, long version) {
    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    kv_store_key<std::string> comp_key = {key, version};

    auto it = this->put_replies.find(comp_key);
    if(it == this->put_replies.end()){
        // a chave não existe
        std::set<long> temp;
        this->put_replies.emplace(comp_key, temp);
        this->put_mutexes.emplace(std::move(comp_key), std::make_pair(std::make_unique<std::mutex>(), std::make_unique<std::condition_variable>()));
    }else{
        throw ConcurrentWritesSameKeyException();
    }

    lock.unlock();
    return version;
}

std::unique_ptr<std::set<long>> client_reply_handler::wait_for_put(kv_store_key<std::string> key){
    std::unique_ptr<std::set<long>> res = nullptr;

    std::unique_lock<std::mutex> lock(this->put_global_mutex);

    auto it = this->put_replies.find(key);
    if(it != this->put_replies.end()) {
        // se a chave existe, fazer lock da chave
        auto &sync_pair = this->put_mutexes.find(key)->second;
        std::unique_lock<std::mutex> lock_key(*sync_pair.first);

        if (it->second.size() < this->nr_puts_required) {
            // caso ainda não tenhamos o número de respostas necessárias

            // fazer unlock do global put lock
            lock.unlock();
            // esperar por um notify na chave -> ele faz lock automatico da chave
            sync_pair.second->wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verificar se o put já foi realizado com sucesso
        auto it_new = this->put_replies.find(key);
        if (it_new->second.size() >= this->nr_puts_required) {

            // se o put já foi realizado com sucesso, como ainda temos os locks
            // podemos remover as entradas para a chave
            res = std::make_unique<std::set<long>>(it_new->second);
            this->put_replies.erase(it_new);

            // não é necessário acordar possiveis threads presas na cond variable
            // porque é certo que apenas uma thread podes estar à espera de uma mesma key
            auto it_key = this->put_mutexes.find(key);
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
                               // como sai do scope ele vai tentar faazer free num mutex inexistente (SIGSEV)
            this->put_mutexes.erase(it_key);
        }

    }

    lock.unlock();

    return std::move(res);
}

void client_reply_handler::register_get(std::string req_id) {
    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(req_id);
    if(it == this->get_replies.end()){
        // a chave não existe
        std::vector<std::pair<long, std::shared_ptr<std::string>>> temp;
        this->get_replies.emplace(req_id, temp);
        this->get_mutexes.emplace(req_id, std::make_pair(std::make_unique<std::mutex>(), std::make_unique<std::condition_variable>()));
    }else{
        //É impossível isto acontecer (registar uma mesma key)
    }

    lock.unlock();
}

std::shared_ptr<std::string> client_reply_handler::wait_for_get(std::string req_id, int wait_for) {
    std::shared_ptr<std::string> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //se existe entrada para a chave, fazer lock dessa entrada
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(*sync_pair.first);

        if (it->second.size() < wait_for) {
            // caso ainda não tenhamos o número de respostas necessárias

            // fazer unlock do global get lock
            lock.unlock();
            // esperar por um notify na chave -> ele faz lock automatico da chave
            sync_pair.second->wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verificar se já temos a resposta
        auto it_new = this->get_replies.find(req_id);
        if(it_new->second.size() >= wait_for){

            // se já temos uma maioria de resposta, como ainda temos os locks
            // podemos remover as entradas para a chave
            long max_version = -1;
            for(auto& entry : it_new->second){
                if(entry.first > max_version){
                    max_version = entry.first;
                    res = entry.second;
                }
            }
            this->get_replies.erase(it_new);

            // não é necessário acordar possiveis threads presas na cond variable
            // porque é certo que apenas uma thread podes estar à espera de uma mesma key
            auto it_req_id = this->get_mutexes.find(req_id);
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // como sai do scope ele vai tentar faazer free num mutex inexistente (SIGSEV)
            this->get_mutexes.erase(it_req_id);
        }
    }

    lock.unlock();

    return res;
}

void client_reply_handler::register_get_latest_version(std::string req_id) {
    // são utilizadas as mesmas estruturas que para os gets
    register_get(req_id);
}

std::unique_ptr<long> client_reply_handler::wait_for_get_latest_version(std::string req_id, int wait_for) {
    std::unique_ptr<long> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //se existe entrada para a chave, fazer lock dessa entrada
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(*sync_pair.first);

        if (it->second.size() < wait_for) {
            // caso ainda não tenhamos o número de respostas necessárias

            // fazer unlock do global get lock
            lock.unlock();
            // esperar por um notify na chave -> ele faz lock automatico da chave
            sync_pair.second->wait_for(lock_key, std::chrono::seconds(this->wait_timeout));
            // fazer o unlock da mutex da key, porque temos de obter os locks por ordem
            lock_key.unlock();
            lock.lock();
            lock_key.lock();
        }

        // verificar se já temos a resposta
        auto it_new = this->get_replies.find(req_id);
        if(it_new->second.size() >= wait_for){

            // se já temos uma maioria de resposta, como ainda temos os locks
            // podemos remover as entradas para a chave
            long max_version = -1;
            for(auto& entry : it_new->second){
                if(entry.first > max_version){
                    max_version = entry.first;
                }
            }
            res = std::make_unique<long>(max_version);
            this->get_replies.erase(it_new);

            // não é necessário acordar possiveis threads presas na cond variable
            // porque é certo que apenas uma thread podes estar à espera de uma mesma key
            auto it_req_id = this->get_mutexes.find(req_id);
            lock_key.unlock(); // é estritamente necessário fazer free porque se não ao removermos
            // como sai do scope ele vai tentar faazer free num mutex inexistente (SIGSEV)
            this->get_mutexes.erase(it_req_id);
        }
    }

    lock.unlock();

    return std::move(res);
}

void client_reply_handler::stop() {
    this->running = false;

    //send special message to awake main thread
    struct sockaddr_in serverAddr;
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&serverAddr, '\0', sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(this->port);
    serverAddr.sin_addr.s_addr = inet_addr(this->ip.c_str());

    char* buf[1];

    sendto(sockfd, buf, 1, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    close(sockfd);
}
