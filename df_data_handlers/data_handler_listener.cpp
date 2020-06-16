//
// Created by danielsf97 on 1/16/20.
//

#define LOG(X) std::cout << X << std::endl;
#include "data_handler_listener.h"
#include "df_communication/udp_async_server.h"
#include <kv_message.pb.h>

#include <utility>
#include <cstdlib>
#include <ctime>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

data_handler_listener::data_handler_listener(std::string ip/*, int port*/, long id, float chance, pss *pss, group_construction* group_c, std::shared_ptr<kv_store<std::string>> store, bool smart)
    : ip(std::move(ip)), id(id), chance(chance), pss_ptr(pss), group_c_ptr(group_c), store(std::move(store)), smart_forward(smart), socket_send(socket(PF_INET, SOCK_DGRAM, 0)) {}

void data_handler_listener::reply_client(proto::kv_message& message, const std::string& sender_ip/*, int sender_port*/){
    try{
        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(peer::kv_port/*sender_port*/); // não é necessário +1 porque vou responder para a port de onde o pedido proveio
        serverAddr.sin_addr.s_addr = inet_addr(sender_ip.c_str());

        std::string buf;
        message.SerializeToString(&buf);

        std::unique_lock<std::mutex> lock (socket_send_mutex);
        int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        lock.unlock();

        if(res == -1){
            spdlog::error("Oh dear, something went wrong with send()! %s\n", strerror(errno));

//                printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));
        }
    }catch(...){
        spdlog::error("=============================== Não consegui enviar =================");
//            std::cout <<"=============================== Não consegui enviar =================" << std::endl;
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
            serverAddr.sin_port = htons(peer::kv_port/*peer.port + 1*/); // +1 porque as portas da vista são do df_pss
            serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());

            std::unique_lock<std::mutex> lock (socket_send_mutex);
            int res = sendto(this->socket_send, data, data_size, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            lock.unlock();

            if(res == -1){
                spdlog::error("Oh dear, something went wrong with send()! %s\n", strerror(errno));

//                printf("Oh dear, something went wrong with send()! %s\n", strerror(errno));
            }
        }catch(...){
            spdlog::error("=============================== Não consegui enviar =================");
//            std::cout <<"=============================== Não consegui enviar =================" << std::endl;
        }
    }
}

void data_handler_listener::process_get_message(const proto::kv_message &msg) {
    const proto::get_message& message = msg.get_msg();
    const std::string& sender_ip = message.ip();
    //int sender_port = message.port();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();
    std::unique_ptr<std::string> data(nullptr); //*data = undefined

    //se o pedido ainda não foi processado e não é um pedido interno (não começa com intern)
    if(!this->store->in_log(req_id) && req_id.rfind("intern", 0) != 0){

        this->store->log_req(req_id);
        kv_store_key_version version;
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
                version = kv_store_key_version(message.version());
                kv_store_key<std::string> get_key = {key, version};
                data = this->store->get(get_key);
                version.client_id = get_key.key_version.client_id;
                break;
        }


//            spdlog::debug("<=============(" + std::to_string((data != nullptr)) + ")================== GET (\033[1;31m" + std::to_string(this->id) + "\033[0m) " + req_id + " " + key + " : " + std::to_string(version.version));

//            std::cout << "<=============(" << (data != nullptr) << ")================== " << "GET (\033[1;31m" << this->id << "\033[0m) " << req_id << " " << key << " : " << version.version << std::endl;

        if(data != nullptr){
            //se tenho o conteudo da chave
            float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
//                spdlog::debug(std::to_string(achance) + " <= " + std::to_string(chance));
            if(achance <= this->chance){
                //a probabilidade ditou para responder à mensagem com o conteudo para a chave
                proto::kv_message reply_message;
                auto* message_content = new proto::get_reply_message();
                message_content->set_ip(this->ip);
                //message_content->set_port(this->port);
                message_content->set_id(this->id);
                message_content->set_key(key);
                message_content->set_version(version.version);
                message_content->set_version_client_id(version.client_id);
                message_content->set_reqid(req_id);
                message_content->set_data(data->data(), data->size());
                reply_message.set_allocated_get_reply_msg(message_content);

//                    std::cout << "GET REPLY(\033[1;31m" << this->id << "\033[0m) " << req_id << " ==================================>" << std::endl;

                this->reply_client(reply_message, sender_ip/*, sender_port*/);
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
//                spdlog::debug("===================== DONT HAVEEEEEE =================");
//                std::cout << "===================== DONT HAVEEEEEE =================" << std::endl;

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
                kv_store_key<std::string> get_key = {key, kv_store_key_version(version, message.version_client_id())};
                bool is_merge;
                data = this->store->get_anti_entropy(get_key, &is_merge);

                if(data != nullptr){
                    proto::kv_message reply_message;
                    auto* message_content = new proto::get_reply_message();
                    message_content->set_ip(this->ip);
                    //message_content->set_port(this->port);
                    message_content->set_id(this->id);
                    message_content->set_key(key);
                    message_content->set_version(version);
                    message_content->set_version_client_id(message.version_client_id());
                    message_content->set_reqid(req_id);
                    message_content->set_data(data->data(), data->size());
                    message_content->set_merge(is_merge);
                    reply_message.set_allocated_get_reply_msg(message_content);

                    this->reply_client(reply_message, sender_ip/*, sender_port*/);
                }else{
                    //se não possuo o valor da chave, fazer forward
                    std::vector<peer_data> view = this->pss_ptr->get_view();
                    this->forward_message(view, const_cast<proto::kv_message &>(msg));
                }
            }
        }
    }

}

void data_handler_listener::process_get_reply_message(const proto::kv_message &msg) {
    const proto::get_reply_message& message = msg.get_reply_msg();
    const std::string& key = message.key();
    long version = message.version();
    long client_id = message.version_client_id();
    const std::string& data = message.data();
    bool is_merge = message.merge();

//        spdlog::debug("GET REPLY(\033[1;31m" + std::to_string(this->id) + "\033[0m) " + key + ":" + std::to_string(version) + " <==================================");
//        std::cout << "GET REPLY(\033[1;31m" << this->id << "\033[0m) " << key << ":" << version << " <==================================" << std::endl;

    if (!this->store->have_seen(key, version, client_id)) {
        try {
            if(is_merge){
                bool stored = this->store->put_with_merge(key, version, client_id, data);
            }else{
                bool stored = this->store->put(key, version, client_id, data);
            }
        }catch(std::exception& e){}
    }
}

void data_handler_listener::process_put_message(const proto::kv_message &msg) {

    const auto& message = msg.put_msg();
    const std::string& sender_ip = message.ip();
    const std::string& key = message.key();
    long version = message.version();
    long client_id = message.id();
    const std::string& data = message.data();


    if (!this->store->have_seen(key, version, client_id)) {
        bool stored;
        try {
            stored = this->store->put(key, version, client_id, data);
        }catch(std::exception& e){
            stored = false;
        }

//            std::cout << "<==========(" << std::to_string(stored) <<")============== " << "PUT (\033[1;31m" << this->id << "\033[0m) " << key << " : " << version << std::endl;
        if (stored) {
            float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
//                std::cout << achance << " <= " << chance << std::endl;
//                spdlog::debug(std::to_string(achance) + " <= " + std::to_string(chance));
            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (achance <= this->chance) {
                proto::kv_message reply_message;
                auto *message_content = new proto::put_reply_message();
                message_content->set_ip(this->ip);
                //message_content->set_port(this->port);
                message_content->set_id(this->id);
                message_content->set_key(key);
                message_content->set_version(version);
                reply_message.set_allocated_put_reply_msg(message_content);

//                    std::cout << "PUT REPLY (\033[1;31m" << this->id << "\033[0m) " << key << " : " << version << " ==================================>" << std::endl;
                this->reply_client(reply_message, sender_ip/*, sender_port*/);
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

void data_handler_listener::process_put_with_merge_message(const proto::kv_message &msg) {
    const auto& message = msg.put_with_merge_msg();
    const std::string& sender_ip = message.ip();
    const std::string& key = message.key();
    long version = message.version();
    long client_id = message.id();
    const std::string& data = message.data();


    if (!this->store->have_seen(key, version, client_id)) {
        bool stored;
        try {
            stored = this->store->put_with_merge(key, version, client_id, data);
        }catch(std::exception& e){
            stored = false;
        }

//            std::cout << "<==========(" << std::to_string(stored) <<")============== " << "PUT (\033[1;31m" << this->id << "\033[0m) " << key << " : " << version << std::endl;
        if (stored) {
            float achance = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
//                std::cout << achance << " <= " << chance << std::endl;
//                spdlog::debug(std::to_string(achance) + " <= " + std::to_string(chance));
            std::vector<peer_data> view = this->pss_ptr->get_slice_local_view();
            if (achance <= this->chance) {
                proto::kv_message reply_message;
                auto *message_content = new proto::put_reply_message();
                message_content->set_ip(this->ip);
                //message_content->set_port(this->port);
                message_content->set_id(this->id);
                message_content->set_key(key);
                message_content->set_version(version);
                reply_message.set_allocated_put_reply_msg(message_content);

//                    std::cout << "PUT REPLY (\033[1;31m" << this->id << "\033[0m) " << key << " : " << version << " ==================================>" << std::endl;
                this->reply_client(reply_message, sender_ip/*, sender_port*/);
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

long data_handler_listener::get_anti_entropy_req_count(){
    return this->anti_entropy_req_count += 1;
}

void data_handler_listener::process_anti_entropy_message(const proto::kv_message &msg) {
    const proto::anti_entropy_message& message = msg.anti_entropy_msg();

    try {

        std::unordered_set<kv_store_key<std::string>> keys_to_request;
        for(auto& key: message.keys()){
            keys_to_request.insert({key.key(), kv_store_key_version(key.version(), key.client_id())});
        }
        this->store->remove_from_set_existent_keys(keys_to_request);

        std::string req_id;
        req_id.reserve(50);
        req_id.append("intern").append(to_string(this->id)).append(":").append(to_string(this->get_anti_entropy_req_count()));

        for (auto &key: keys_to_request) {
            proto::kv_message get_msg;
            auto *message_content = new proto::get_message();
            message_content->set_ip(this->ip);
            //message_content->set_port(this->port);
            message_content->set_id(this->id);
            message_content->set_key(key.key);
            message_content->set_version(key.key_version.version);
            message_content->set_version_client_id(key.key_version.client_id);
            message_content->set_reqid(req_id);
            get_msg.set_allocated_get_msg(message_content);

            this->reply_client(get_msg, message.ip()/*, message.port()*/);
        }
    }catch (std::exception& e){
        // Unable to Get Keys
        return;
    }
}

//TODO eu acho que a cena da chance devia ser, se for menor que o this->chance responde e faz forward, se não só responde
void data_handler_listener::process_get_latest_version_msg(proto::kv_message msg) {
    const proto::get_latest_version_message& message = msg.get_latest_version_msg();
    const std::string& sender_ip = message.ip();
    const std::string& key = message.key();
    const std::string& req_id = message.reqid();
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

//            std::cout << "<=============(" << (version != nullptr) << ")================== " << "GET Version (\033[1;31m" << this->id << "\033[0m) " << req_id << " " << key << std::endl;


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
            //message_content->set_port(this->port);
            message_content->set_id(this->id);
            message_content->set_version(*version);
            message_content->set_reqid(req_id);
            reply_message.set_allocated_get_latest_version_reply_msg(message_content);

            this->reply_client(reply_message, sender_ip);
//                std::cout << "GET REPLY Version(\033[1;31m" << this->id << "\033[0m) " << req_id << " ==================================>" << std::endl;

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

void data_handler_listener::process_recover_request_msg(const proto::kv_message& msg) {
    const proto::recover_request_message& message = msg.recover_request_msg();
    const std::string& sender_ip = message.ip();
    int nr_slices = message.nr_slices();
    int slice = message.slice();

    if(this->group_c_ptr->get_my_group() != slice ||
       this->group_c_ptr->get_nr_groups() != nr_slices ||
       !this->group_c_ptr->has_recovered())
    {
        return;
    }

    try {
        tcp_client_server_connection::tcp_client_connection connection(sender_ip.c_str(), peer::recover_port);

        char rcv_buf[65500];

        int bytes_rcv = connection.recv_msg(rcv_buf); //throw exception

        proto::kv_message kv_message_rcv;
        kv_message_rcv.ParseFromArray(rcv_buf, bytes_rcv);

        if(kv_message_rcv.has_recover_offset_msg()) {

            const proto::recover_offset_message& off_msg = kv_message_rcv.recover_offset_msg();
            std::vector<std::string> off_keys;
            for(auto& key: off_msg.keys()){
                off_keys.emplace_back(key);
            }

            // For Each Key greater than offset send to peer
            this->store->send_keys_gt(off_keys, connection,
                                      [](tcp_client_server_connection::tcp_client_connection& connection, const std::string& key,
                                         long version, long client_id, bool is_merge, const char* data, size_t data_size){
                                          proto::kv_message kv_message;
                                          auto* message_content = new proto::recover_data_message();
                                          message_content->set_key(key);
                                          message_content->set_version(version);
                                          message_content->set_version_client_id(client_id);
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
            auto* message_content = new proto::recover_termination_message();
            kv_message.set_allocated_recover_termination_msg(message_content);
            std::string buf;
            kv_message.SerializeToString(&buf);

            try {
                connection.send_msg(buf.data(), buf.size());
            }catch(std::exception& e){
                std::cerr << "Exception: " << e.what() << std::endl;
            }
        }

    }catch(const char* e){
        std::cerr << "Exception: " << e << std::endl;
    }catch(std::exception& e){
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

