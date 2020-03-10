//
// Created by danielsf97 on 3/8/20.
//

#include "client_reply_handler_mt.h"
#include "../kv_message.pb.h"
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <df_communication/udp_async_server.h>
#include "../exceptions/custom_exceptions.h"

/* =====================================================================================================================
* ======================================== Worker Class ================================================================
* =====================================================================================================================*/

class client_reply_handler_listener_worker : public udp_handler {
    //Esta classe é partilhada por todas as threads
private:
    client_reply_handler_mt *reply_handler;

public:
    client_reply_handler_listener_worker(client_reply_handler_mt* reply_handler): reply_handler(reply_handler){}

    void handle_function(const char *data, size_t size) override {
        try {
            proto::kv_message message;
            message.ParseFromArray(data, size);
            if (message.has_get_reply_msg()) {

                const proto::get_reply_message &msg = message.get_reply_msg();
                reply_handler->process_get_reply_msg(msg);
            } else if (message.has_put_reply_msg()) {

                const proto::put_reply_message &msg = message.put_reply_msg();
                reply_handler->process_put_reply_msg(msg);
            }
        }
        catch (const char *e) {
            std::cerr << e << std::endl;
        }
        catch (...) {}
    }
};

/*=====================================================================================================================*/

client_reply_handler_mt::client_reply_handler_mt(std::string ip, int port, int nr_puts_required, long wait_timeout):
        ip(ip), port(port), nr_puts_required(nr_puts_required), wait_timeout(wait_timeout), socket_rcv(socket(PF_INET, SOCK_DGRAM, 0))
{}

client_reply_handler_mt::~client_reply_handler_mt()
{
    close(this->socket_rcv);
}

void client_reply_handler_mt::operator()() {
    try {
        client_reply_handler_listener_worker worker(this);
        udp_async_server server(this->io_service, this->port, (udp_handler*) &worker);

        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));

        this->thread_pool.join_all();
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

long client_reply_handler_mt::register_put(std::string key, long version) {
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

std::unique_ptr<std::set<long>> client_reply_handler_mt::wait_for_put(kv_store_key<std::string> key){
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

void client_reply_handler_mt::register_get(std::string req_id) {
    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(req_id);
    if(it == this->get_replies.end()){
        // a chave não existe
        this->get_replies.emplace(req_id, std::shared_ptr<std::string>(nullptr));
        this->get_mutexes.emplace(req_id, std::make_pair(std::make_unique<std::mutex>(), std::make_unique<std::condition_variable>()));
    }else{
        //É impossível isto acontecer (registar uma mesma key)
    }

    lock.unlock();
}

std::shared_ptr<std::string> client_reply_handler_mt::wait_for_get(std::string req_id) {
    std::shared_ptr<std::string> res (nullptr);

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    auto it = this->get_replies.find(req_id);
    if(it != this->get_replies.end()) {
        //se existe entrada para a chave, fazer lock dessa entrada
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> lock_key(*sync_pair.first);

        if(it->second == nullptr){
            // caso ainda não tenhamos a resposta

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
        if(it_new->second != nullptr){

            // se já temos a resposta, como ainda temos os locks
            // podemos remover as entradas para a chave
            res = it_new->second;
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

void client_reply_handler_mt::stop() {
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

void client_reply_handler_mt::process_get_reply_msg(const proto::get_reply_message &msg) {
    std::string req_id = msg.reqid();
    long replier_id = msg.id();
    std::string data = msg.data();

    std::unique_lock<std::mutex> lock(this->get_global_mutex);

    if (this->get_replies.find(req_id) != this->get_replies.end()) {
        // a chave existe
        auto &sync_pair = this->get_mutexes.find(req_id)->second;
        std::unique_lock<std::mutex> reqid_lock(*sync_pair.first);
        lock.unlock(); //free global get lock

        this->get_replies.insert_or_assign(req_id, std::make_shared<std::string>(data));
        sync_pair.second->notify_all();
    } else {
        std::cout << "GET REPLY IGNORED - NON EXISTENT KEY" << std::endl;
    }
}

void client_reply_handler_mt::process_put_reply_msg(const proto::put_reply_message &msg) {
    std::string key = msg.key();
    long version = msg.version();
    kv_store_key<std::string> comp_key = {key, version};
    long replier_id = msg.id();

    std::unique_lock<std::mutex> lock(this->put_global_mutex);
    auto it = this->put_replies.find(comp_key);
    if (it != this->put_replies.end()) {
        // a chave existe
        auto &sync_pair = this->put_mutexes.find(comp_key)->second;
        std::unique_lock<std::mutex> key_lock(*sync_pair.first);

        lock.unlock(); //free global put lock

        it->second.emplace(replier_id);
        sync_pair.second->notify_all();
        key_lock.unlock();
    } else {
        std::cout << "PUT REPLY IGNORED - NON EXISTENT KEY" << std::endl;
    }
}