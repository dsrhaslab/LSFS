//
// Created by danielsf97 on 1/16/20.
//

#define LOG(X) std::cout << X << std::endl;
#include "data_handler_listener.h"
#include "../communication/udp_async_server.h"
#include <kv_message.pb.h>

#include <utility>
#include <cstdlib>
#include <ctime>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

//TODO Se calhar preciso de sincronizar o socket send

class data_handler_listener_worker : public udp_handler{
private:
    std::string ip;
    int port;
    long id;
    std::shared_ptr<kv_store> store;
    pss* pss_ptr;
    float chance;
    int socket_send;
    bool smart_forward;

public:
    data_handler_listener_worker(std::string ip, int port, long id,std::shared_ptr<kv_store> store, pss *pssPtr, float chance, int socket_send, bool smart_forward)
        : ip(std::move(ip)), port(port), id(id), chance(chance), pss_ptr(pssPtr), store(std::move(store)), socket_send(socket_send), smart_forward(smart_forward) {
        srand (static_cast <unsigned> (time(nullptr))); //random seed
    }

    void handle_function(const char *data, size_t size) override {
        try {
            std::cout << "Received KV store request" << std::endl;
            this->store->print_store();

            proto::kv_message msg;
            msg.ParseFromArray(data, size);

            if(msg.has_get_msg()){
                std::cout << "IS A GET MESSAGE" << std::endl;
                this->process_get_message(msg);
            }else if(msg.has_get_reply_msg()){
                std::cout << "IS A GET REPLY MESSAGE" << std::endl;
                this->process_get_reply_message(msg);
            }else if(msg.has_put_msg()){
                std::cout << "IS A PUT MESSAGE" << std::endl;
                this->process_put_message(msg);
            }else if(msg.has_put_reply_msg()){
                std::cout << "IS A PUT REPLY MESSAGE" << std::endl;
                this->process_put_reply_message(msg);
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

            std::cout << "BUFFER SIZE " << buf.size() << std::endl;

            int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

            if(res == -1){printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));}
            else{
                std::cout << "Reply sucessfully sent to " << sender_port << std::endl;
            }
        }catch(...){std::cout <<"=============================== Não consegui enviar =================" << std::endl;}

    }

    void forward_message(std::vector<peer_data> view_to_send, proto::kv_message& message){
        for(peer_data& peer: view_to_send){
            try {
                struct sockaddr_in serverAddr;
                memset(&serverAddr, '\0', sizeof(serverAddr));

                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(peer.port + 1); // +1 porque as portas da vista são do pss
                serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());

                std::string buf;
                message.SerializeToString(&buf);

                int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

                if(res == -1){printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));}
            }catch(...){std::cout <<"=============================== NÂO consegui enviar =================" << std::endl;}
        }
    }

//    void process_get_message(const proto::kv_message &msg) {
//        proto::get_message message = msg.get_msg();
//        std::string sender_ip = message.ip();
//        int sender_port = message.port();
//        long key = message.key();
//        long version = message.version();
//        std::string req_id = message.reqid();
//        std::unique_ptr<const char*> data(new const char*()); //*data = undefined
//
//        //se o pedido ainda não foi processado e não é um pedido interno (não começa com intern)
//        if(!this->store->in_log(req_id) && req_id.rfind("intern", 0) != 0){
//            this->store->log_req(req_id);
//            *data = this->store->get({key, version});
//            if(*data != nullptr){
//                //se tenho o conteudo da chave
//                float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
//                if(achance <= this->chance){
//                    //a probabilidade ditou para responder à mensagem com o conteudo para a chave
//                    proto::kv_message reply_message;
//                    auto* message_content = new proto::get_reply_message();
//                    message_content->set_ip(this->ip);
//                    message_content->set_port(this->port);
//                    message_content->set_id(this->id);
//                    message_content->set_key(key);
//                    message_content->set_version(version);
//                    message_content->set_reqid(req_id);
//                    message_content->set_data(*data);
//                    reply_message.set_allocated_get_reply_msg(message_content);
//
//                    this->reply_client(reply_message, sender_ip, sender_port);
//                }else{
//                    //a probabilidade ditou para fazer forward da mensagem
//                    int obj_slice = this->store->get_slice_for_key(key);
//                    std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
//                    if(!slice_peers.empty() && this->smart_forward){
//                        this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
//                    }else{
//                        std::vector<peer_data> view = this->pss_ptr->get_view();
//                        this->forward_message(view, const_cast<proto::kv_message &>(msg));
//                    }
//                }
//            }else{
//                //se não tenho o conteudo da chave -> fazer forward
//                int obj_slice = this->store->get_slice_for_key(key);
//                std::vector<peer_data> slice_peers = this->pss_ptr->have_peer_from_slice(obj_slice);
//                if(!slice_peers.empty() && this->smart_forward){
//                    this->forward_message(slice_peers, const_cast<proto::kv_message &>(msg));
//                }else{
//                    std::vector<peer_data> view = this->pss_ptr->get_view();
//                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
//                }
//            }
//        }else{
//            //caso seja um get interno (antientropy)
//            if(req_id.rfind("intern", 0) == 0){
//                //TODO Tratar dos pedidos de anti entropia
//            }
//        }
//
//    }

    void process_get_message(const proto::kv_message &msg) {
        proto::get_message message = msg.get_msg();
        std::string sender_ip = message.ip();
        int sender_port = message.port();
        long key = message.key();
        long version = message.version();
        std::string req_id = message.reqid();
        std::shared_ptr<const char[]> data(nullptr); //*data = undefined

        //se o pedido ainda não foi processado e não é um pedido interno (não começa com intern)
        if(!this->store->in_log(req_id) && req_id.rfind("intern", 0) != 0){
            this->store->log_req(req_id);
            data = this->store->get({key, version});
            if(data != nullptr){
                //se tenho o conteudo da chave
                float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
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
                    message_content->set_data(data.get());
                    reply_message.set_allocated_get_reply_msg(message_content);

                    std::cout << "sending " << data.get() << " to client" << std::endl;

                    this->reply_client(reply_message, sender_ip, sender_port);
                }else{
                    //a probabilidade ditou para fazer forward da mensagem
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
                //TODO Tratar dos pedidos de anti entropia
            }
        }

    }

    void process_get_reply_message(const proto::kv_message &msg) {
        proto::get_reply_message message = msg.get_reply_msg();
        long key = message.key();
        long version = message.version();
        if(!this->store->have_seen(key, version)){
            this->store->put(key, version, message.data().c_str());
        }
    }

    void process_put_message(const proto::kv_message &msg) {
        std::cout << "Processing Put Message" << std::endl;
        proto::put_message message = msg.put_msg();
        std::string sender_ip = message.ip();
        int sender_port = message.port();
        long key = message.key();
        long version = message.version();
        const char *data = message.data().c_str();

        std::cout << data << std::endl;

        if (!this->store->have_seen(key, version)) {
            std::cout << "Store haven't seen this key-version" << std::endl;
            bool stored = this->store->put(key, version, data);
            if (stored) {
                float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                std::vector<peer_data> view = this->pss_ptr->get_view();
                std::cout << "checking if " << achance << "<=" << this->chance << std::endl;
                if (achance <= this->chance) {
                    proto::kv_message reply_message;
                    auto *message_content = new proto::put_reply_message();
                    message_content->set_ip(this->ip);
                    message_content->set_port(this->port);
                    message_content->set_id(this->id);
                    message_content->set_key(key);
                    message_content->set_version(version);
                    reply_message.set_allocated_put_reply_msg(message_content);
                    std::cout << "Replying Client" << std::endl;

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
            std::cout << "Store have seen this key-version - discarding put" << std::endl;
            //Worker ignored put operation for key
            //#############################################################################################
            //TODO Remover só para um cliente não encravar no put caso a chave já exista
            float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
            if (achance <= this->chance) {
                proto::kv_message reply_message;
                auto *message_content = new proto::put_reply_message();
                message_content->set_ip(this->ip);
                message_content->set_port(this->port);
                message_content->set_id(this->id);
                message_content->set_key(key);
                message_content->set_version(version);
                reply_message.set_allocated_put_reply_msg(message_content);
                std::cout << "Replying Client" << std::endl;

                this->reply_client(reply_message, sender_ip, sender_port);
            }
            //#############################################################################################
        }
    }

    void process_put_reply_message(const proto::kv_message &msg) {
        //Este caso não vai acontecer porque os peers não deveriam receber mensagens de reply a um put
    }
};

data_handler_listener::data_handler_listener(std::string ip, int port, long id, float chance, pss *pss, std::shared_ptr<kv_store> store, bool smart)
    : ip(std::move(ip)), port(port), id(id), chance(chance), pss_ptr(pss), store(std::move(store)), socket_send(socket(PF_INET, SOCK_DGRAM, 0)), smart(smart) {}

void data_handler_listener::operator()() {
    try {
        data_handler_listener_worker worker(this->ip, this->port, this->id, this->store, this->pss_ptr, this->chance, this->socket_send, this->smart);
        udp_async_server server(this->io_service, this->port, (udp_handler*) &worker);

        std::cout << "Waiting for KV store requests" << std::endl;

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

