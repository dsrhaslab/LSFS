//
// Created by danielsf97 on 1/16/20.
//

#define LOG(X) std::cout << X << std::endl;
#include "data_handler_listener.h"
#include "../df_communication/udp_async_server.h"
#include <kv_message.pb.h>

#include <utility>
#include <cstdlib>
#include <ctime>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

//TODO Se calhar preciso de sincronizar o socket send

class data_handler_listener_worker : public udp_handler{
    //Esta classe é partilhada por todas as threads
private:
    std::string ip;
    int port;
    long id;
    std::shared_ptr<kv_store<std::string>> store;
    pss* pss_ptr;
    float chance;
    int socket_send;
    std::recursive_mutex socket_send_mutex;
    bool smart_forward;
    std::atomic<long> anti_entropy_req_count = 0;

public:
    data_handler_listener_worker(std::string ip, int port, long id,std::shared_ptr<kv_store<std::string>> store, pss *pssPtr, float chance, bool smart_forward)
        : ip(std::move(ip)), port(port), id(id), chance(chance), pss_ptr(pssPtr), store(std::move(store)), socket_send(socket(PF_INET, SOCK_DGRAM, 0)), smart_forward(smart_forward)
    {
        srand (static_cast <unsigned int> ((time(nullptr) % 1000))*getpid()); //random seed
    }

    void handle_function(const char *data, size_t size) override {
        try {
            proto::kv_message msg;
            msg.ParseFromArray(data, size);

            if(msg.has_get_msg()){
                this->process_get_message(msg);
            }else if(msg.has_get_reply_msg()){
                this->process_get_reply_message(msg);
            }else if(msg.has_put_msg()){
                this->process_put_message(msg);
            }else if(msg.has_put_reply_msg()){
                //Este caso não vai acontecer porque os peers não deveriam receber mensagens de reply a um put
            }else if(msg.has_anti_entropy_msg()){
                this->process_anti_entropy_message(msg);
            }else if(msg.has_get_latest_version_msg()){
                this->process_get_latest_version_msg(msg);
            }else if(msg.has_get_latest_version_reply_msg()){
                //Este caso não vai acontecer porque os peers não deveriam receber mensagens de reply a um get version
            }

        }
        catch(const char* e){
            std::cout << e << std::endl;
        }
        catch(...){}
    }

private:

    void reply_client(proto::kv_message& message, const std::string& sender_ip, int sender_port){
        try{
            struct sockaddr_in serverAddr;
            memset(&serverAddr, '\0', sizeof(serverAddr));

            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(sender_port); // não é necessário +1 porque vou responder para a port de onde o pedido proveio
            serverAddr.sin_addr.s_addr = inet_addr(sender_ip.c_str());

            std::string buf;
            message.SerializeToString(&buf);

            std::scoped_lock<std::recursive_mutex> lk (socket_send_mutex);
            int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            if(res == -1){printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));}
        }catch(...){std::cout <<"=============================== Não consegui enviar =================" << std::endl;}

    }

    void forward_message(std::vector<peer_data> view_to_send, proto::kv_message& message){
        for(peer_data& peer: view_to_send){
            try {
                struct sockaddr_in serverAddr;
                memset(&serverAddr, '\0', sizeof(serverAddr));

                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(peer.port + 1); // +1 porque as portas da vista são do df_pss
                serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());

                std::string buf;
                message.SerializeToString(&buf);

                std::scoped_lock<std::recursive_mutex> lk (socket_send_mutex);
                int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
                if(res == -1){printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));}
            }catch(...){std::cout <<"=============================== NÂO consegui enviar =================" << std::endl;}
        }
    }

    void process_get_message(const proto::kv_message &msg) {
        proto::get_message message = msg.get_msg();
        std::string sender_ip = message.ip();
        int sender_port = message.port();
        std::string key = message.key();
        std::string req_id = message.reqid();
        std::shared_ptr<std::string> data(nullptr); //*data = undefined

        //se o pedido ainda não foi processado e não é um pedido interno (não começa com intern)
        if(!this->store->in_log(req_id) && req_id.rfind("intern", 0) != 0){
            this->store->log_req(req_id);

            long version;
            switch (message.version_avail_case()){
                case proto::get_message::kVersionNone:
                    // Só faz sentido questionar pela última versão conhecida da chave para
                    // peers que pertençam à mesma slice que a key
                    if(this->store->get_slice_for_key(key) == this->store->get_slice()) {
                        try {
                            data = this->store->get_latest(key, &version);
                        } catch (std::exception &e) {
                            //LevelDBException
                            e.what();
                        }
                    }
                    break;
                case proto::get_message::kVersion:
                    version = message.version();
                    data = this->store->get({key, version});
                    break;
            }

            std::cout << "<=============(" << (data != nullptr) << ")================== " << "GET (\033[1;31m" << this->id << "\033[0m) " << req_id << " " << key << " : " << version << std::endl;

            if(data != nullptr){
                //se tenho o conteudo da chave
                auto data_size = data->size();
                char buf[data_size];
                data->copy(buf, data_size);
                float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                std::cout << achance << " <= " << chance << std::endl;
                if(achance <= this->chance){
                    //a probabilidade ditou para responder à mensagem com o conteudo para a chave
                    proto::kv_message reply_message;
                    auto* message_content = new proto::get_reply_message();
                    message_content->set_ip(this->ip);
                    message_content->set_port(this->port);
                    message_content->set_id(this->id);
                    message_content->set_key(key);
                    message_content->set_version(version);
                    message_content->set_reqid(req_id);
                    message_content->set_data(buf, data_size);
                    reply_message.set_allocated_get_reply_msg(message_content);

                    std::cout << "GET REPLY (\033[1;31m" << this->id << "\033[0m) " << req_id << " ==================================>" << std::endl;

                    this->reply_client(reply_message, sender_ip, sender_port);
                    // forward to other peers from my slice if is the right slice for the key
                    // trying to speed up quorum
                    int obj_slice = this->store->get_slice_for_key(key);
                    if(this->store->get_slice() == obj_slice){
                        std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                        this->forward_message(view, const_cast<proto::kv_message &>(msg));
                    }
                }else{
                    //a probabilidade ditou para fazer forward da mensagem
                    int obj_slice = this->store->get_slice_for_key(key);
                    std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
                    if(!slice_peers.empty() && this->smart_forward){
                        std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
                    }else{
                        std::vector<peer_data> view = this->pss_ptr->get_view();
                        this->forward_message(view, const_cast<proto::kv_message &>(msg));
                    }
                }
            }else{
                std::cout << "===================== DONT HAVEEEEEE =================" << std::endl;

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
        }else{
            //caso seja um get interno (antientropy)
            if(req_id.rfind("intern", 0) == 0){
                if(!this->store->in_anti_entropy_log(req_id)){
                    long version = message.version();

                    this->store->log_anti_entropy_req(req_id);
                    data = this->store->get({key, version});

                    if(data != nullptr){
                        auto data_size = data->size();
                        char buf[data_size];
                        data->copy(buf, data_size);

                        proto::kv_message reply_message;
                        auto* message_content = new proto::get_reply_message();
                        message_content->set_ip(this->ip);
                        message_content->set_port(this->port);
                        message_content->set_id(this->id);
                        message_content->set_key(key);
                        message_content->set_version(version);
                        message_content->set_reqid(req_id);
                        message_content->set_data(buf, data_size);
                        reply_message.set_allocated_get_reply_msg(message_content);

                        this->reply_client(reply_message, sender_ip, sender_port);
                    }else{
                        //se não possuo o valor da chave, fazer forward
                        std::vector<peer_data> view = this->pss_ptr->get_view();
                        this->forward_message(view, const_cast<proto::kv_message &>(msg));
                    }
                }
            }
        }

    }

    void process_get_reply_message(const proto::kv_message &msg) {
        proto::get_reply_message message = msg.get_reply_msg();
        std::string key = message.key();
        long version = message.version();
        std::string data = message.data();

        if (!this->store->have_seen(key, version)) {
            try {
                bool stored = this->store->put(key, version, data);
            }catch(std::exception e){}
        }
    }

    void process_put_message(const proto::kv_message &msg) {
        proto::put_message message = msg.put_msg();
        std::string sender_ip = message.ip();
        int sender_port = message.port();
        std::string key = message.key();
        long version = message.version();
        std::string data = message.data();

        if (!this->store->have_seen(key, version)) {
            bool stored;
            try {
                stored = this->store->put(key, version, data);
            }catch(std::exception){
                stored = false;
            }
            std::cout << "<==========(" << std::to_string(stored) <<")============== " << "PUT (\033[1;31m" << this->id << "\033[0m) " << key << " : " << version << std::endl;
            if (stored) {
                float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                std::cout << achance << " <= " << chance << std::endl;
                std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
                if (achance <= this->chance) {
                    proto::kv_message reply_message;
                    auto *message_content = new proto::put_reply_message();
                    message_content->set_ip(this->ip);
                    message_content->set_port(this->port);
                    message_content->set_id(this->id);
                    message_content->set_key(key);
                    message_content->set_version(version);
                    reply_message.set_allocated_put_reply_msg(message_content);

                    std::cout << "PUT REPLY (\033[1;31m" << this->id << "\033[0m) " << key << " : " << version << " ==================================>" << std::endl;
                    this->reply_client(reply_message, sender_ip, sender_port);
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
        } else {
            //Worker ignored put operation for key
            //#############################################################################################
            //TODO Remover só para um cliente não encravar no put caso a chave já exista
//            float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
//            if (achance <= this->chance) {
//                proto::kv_message reply_message;
//                auto *message_content = new proto::put_reply_message();
//                message_content->set_ip(this->ip);
//                message_content->set_port(this->port);
//                message_content->set_id(this->id);
//                message_content->set_key(key);
//                message_content->set_version(version);
//                reply_message.set_allocated_put_reply_msg(message_content);
//
//                this->reply_client(reply_message, sender_ip, sender_port);
//            }
            //#############################################################################################
        }
    }

    long get_anti_entropy_req_count(){
        return this->anti_entropy_req_count += 1;
    }

    void process_anti_entropy_message(const proto::kv_message &msg) {
        proto::anti_entropy_message message = msg.anti_entropy_msg();


        try {
            std::unordered_set<kv_store_key<std::string>> keys = this->store->get_keys();

            std::unordered_set<kv_store_key<std::string>> keys_to_request;
            for (auto &key : message.keys()) {
                kv_store_key<std::string> kv_key = {key.key(), key.version()};
                if (keys.find(kv_key) == keys.end()) {
                    // não possuimos a chave
                    if (this->store->get_slice_for_key(kv_key.key) == this->store->get_slice()) {
                        //se a chave pertence à minha slice
                        keys_to_request.insert(kv_key);
                    }
                }
            }

            for (auto &key: keys_to_request) {
                proto::kv_message get_msg;
                auto *message_content = new proto::get_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->port);
                message_content->set_id(this->id);
                message_content->set_key(key.key);
                message_content->set_version(key.version);
                message_content->set_reqid(
                        "intern" + to_string(this->id) + ":" + to_string(this->get_anti_entropy_req_count()));
                get_msg.set_allocated_get_msg(message_content);

                this->reply_client(get_msg, message.ip(), message.port());
            }
        }catch (std::exception e){
            // Unable to Get Keys
            return;
        }
    }

    //TODO eu acho que a cena da chance devia ser, se for menor que o this->chance responde e faz forward, se não só responde
    void process_get_latest_version_msg(proto::kv_message msg) {
        proto::get_latest_version_message message = msg.get_latest_version_msg();
        std::string sender_ip = message.ip();
        int sender_port = message.port();
        std::string key = message.key();
        std::string req_id = message.reqid();
        std::unique_ptr<long> version(nullptr);

        //se o pedido ainda não foi processado
        if(!this->store->in_log(req_id)){
            this->store->log_req(req_id);
            //só faz sentido perguntar qual é a última versão de uma chave aos peers inseridos na slice
            //cuja chave pertence, isto porque alguns peers podem ter versões antigas da chave e como não
            //pertencem à slice atual, não possuem as mais recentes.
            if(this->store->get_slice_for_key(key) == this->store->get_slice()){
                try {
                    version = this->store->get_latest_version(key);
                    //Quando o peer era suposto ter a chave mas não tem, responde com a versão da chave -1
                    //tal impede que quando a chave não existe, se tenha de esperar pelo timeout
                    //(no caso de getattr que são muito frequentes, seria um motivo de mau desempenho)
                    if (version == nullptr) {
                        version = std::make_unique<long>(-1);
                    }
                }catch(std::exception& e){
                    //LevelDBException
                    e.what();
                }
            }

            std::cout << "<=============(" << (version != nullptr) << ")================== " << "GET (\033[1;31m" << this->id << "\033[0m) " << req_id << " " << key << std::endl;

            if(version != nullptr){
                // se a chave pertence à minha slice (versão da chave >= -1)
                float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                if(achance <= this->chance){
                    //a probabilidade ditou para fazer forward message
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
                
                //responder à mensagem
                proto::kv_message reply_message;
                auto* message_content = new proto::get_latest_version_reply_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->port);
                message_content->set_id(this->id);
                message_content->set_version(*version);
                message_content->set_reqid(req_id);
                reply_message.set_allocated_get_latest_version_reply_msg(message_content);

                this->reply_client(reply_message, sender_ip, sender_port);
                std::cout << "GET REPLY (\033[1;31m" << this->id << "\033[0m) " << req_id << " ==================================>" << std::endl;

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
};

data_handler_listener::data_handler_listener(std::string ip, int port, long id, float chance, pss *pss, std::shared_ptr<kv_store<std::string>> store, bool smart)
    : ip(std::move(ip)), port(port), id(id), chance(chance), pss_ptr(pss), store(std::move(store)), smart(smart) {}

void data_handler_listener::operator()() {
    try {
        data_handler_listener_worker worker(this->ip, this->port, this->id, this->store, this->pss_ptr, this->chance, this->smart);
        udp_async_server server(this->io_service, this->port, (udp_handler*) &worker);

        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));

        this->thread_pool.join_all();
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

void data_handler_listener::stop_thread() {
    //terminating ioService processing loop
    this->io_service.stop();
    //joining all threads of thread_loop
    this->thread_pool.join_all();
}

