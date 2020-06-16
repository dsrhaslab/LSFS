//
// Created by danielsf97 on 1/30/20.
//

#include "df_serializer/capnp/capnp_serializer.h"
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"
#include <thread>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <df_core/peer.h>
#include <exceptions/custom_exceptions.h>
#include "anti_entropy.h"

anti_entropy::anti_entropy(std::string ip, long id, pss *pss_ptr, group_construction* group_c,
        std::shared_ptr<kv_store<std::string>> store, long sleep_interval): ip(std::move(ip)), id(id),
        store(std::move(store)), sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0)),
        phase(anti_entropy::Phase::Starting), pss_ptr(pss_ptr), group_c(group_c)
{}

void anti_entropy::send_peer_keys(std::vector<peer_data>& target_peers, proto::kv_message &msg){
    struct sockaddr_in serverAddr;
    memset(&serverAddr, '\0', sizeof(serverAddr));

    std::string buf;
    msg.SerializeToString(&buf);
    const char * buf_data = buf.data();
    auto buf_size = buf.size();

    serverAddr.sin_family = AF_INET;

    for(auto& peer: target_peers){
        serverAddr.sin_port = htons(peer::kv_port);
        serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());

        try {
            int res = sendto(this->sender_socket, buf_data, buf_size, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

            if(res == -1){
                spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            }
        }catch(...){
            spdlog::error("=============================== Não consegui enviar =================");
        }
    }
}

int anti_entropy::send_recover_request(peer_data& target_peer){
    struct sockaddr_in serverAddr;
    memset(&serverAddr, '\0', sizeof(serverAddr));

    proto::kv_message message;
    auto *message_content = new proto::recover_request_message();
    message_content->set_ip(this->ip);
    message_content->set_nr_slices(this->group_c->get_nr_groups());
    message_content->set_slice(this->group_c->get_my_group());
    message.set_allocated_recover_request_msg(message_content);

    std::string buf;
    message.SerializeToString(&buf);
    const char * buf_data = buf.data();
    auto buf_size = buf.size();

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(peer::kv_port);
    serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

    try {
        int res = sendto(this->sender_socket, buf_data, buf_size, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            return -1;
        }
    }catch(...){
        spdlog::error("=============================== Não consegui enviar =================");
        return -1;
    }
    return 0;
}

void anti_entropy::phase_starting() {
    if(this->group_c->has_recovered()){
        this->phase = anti_entropy::Phase::Recovering;
    }
}

bool anti_entropy::recover_state(tcp_client_server_connection::tcp_server_connection& connection, int* socket){

    std::vector<std::string> keys_offset = this->store->get_last_keys_limit4();

    proto::kv_message kv_message;
    auto* message_content = new proto::recover_offset_message();
    for(std::string& key: keys_offset)
        message_content->add_keys(key);
    kv_message.set_allocated_recover_offset_msg(message_content);

    std::string buf;
    kv_message.SerializeToString(&buf);

    try {
        connection.send_msg(socket, buf.data(), buf.size());
    }catch(const char* e){
        std::cerr << "Exception: " << e << std::endl;
        return false;
    }

    bool finished_recovering = false;

    char rcv_buf [65500];
    while (!finished_recovering) {
        try {
            int bytes_rcv = connection.recv_msg(socket, rcv_buf); //throw exception

            proto::kv_message kv_message_rcv;
            kv_message_rcv.ParseFromArray(rcv_buf, bytes_rcv);

            if(kv_message_rcv.has_recover_termination_msg()){
                finished_recovering = true;
            }else if (kv_message_rcv.has_recover_data_msg()) {
                const proto::recover_data_message& message = kv_message_rcv.recover_data_msg();

                if(message.merge()){
                    this->store->put_with_merge(message.key(), message.version(), message.version_client_id(), message.data());
                }else{
                    this->store->put(message.key(), message.version(), message.version_client_id(), message.data());
                }
            }
        }catch(const char* e) {
            std::cerr << "Exception: " << e << std::endl;
        }catch(PeerDisconnectedException& e){
            std::cerr << "Exception: " << e.what() << std::endl;
            return false;
        }catch(std::exception& e){
            std::cerr << "Exception: " << e.what() << std::endl;
        }
    }


    return finished_recovering;
}

void anti_entropy::phase_recovering() {
    tcp_client_server_connection::tcp_server_connection connection =
            tcp_client_server_connection::tcp_server_connection(this->ip.c_str(), peer::recover_port, std::unique_ptr<Serializer>(new Capnp_Serializer));

    while(this->phase == anti_entropy::Phase::Recovering){

        std::vector<peer_data> local_view = this->group_c->get_local_view();

        peer_data p = this->group_c->get_random_peer_from_local_view();
        int res = this->send_recover_request(p);
        if(res == -1){
            continue;
        }
        int socket = connection.accept_connection(this->sleep_interval);
        if(socket != -1){
            bool recovered = this->recover_state(connection, &socket);
            if(recovered){
                this->phase = anti_entropy::Phase::Operating;
            }
            close(socket);
        }
    }
}

void anti_entropy::phase_operating(){
    std::vector<peer_data> slice_view = pss_ptr->get_slice_local_view();

    try {
        proto::kv_message message;
        auto *message_content = new proto::anti_entropy_message();
        message_content->set_ip(this->ip);
        message_content->set_id(this->id);
        for (auto &key : this->store->get_keys()) {
            proto::kv_store_key *kv_key = message_content->add_keys();
            kv_key->set_key(key.key);
            kv_key->set_version(key.key_version.version);
            kv_key->set_client_id(key.key_version.client_id);
        }
        message.set_allocated_anti_entropy_msg(message_content);
        this->send_peer_keys(slice_view, message);
    }catch (std::exception& e){
        // Unable to get Keys -> Do nothing
    }
}

void anti_entropy::operator()() {
    this->running = true;

    while(this->running){
        std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
        if(this->running){
            switch(this->phase){
                case anti_entropy::Phase::Starting:
                    this->phase_starting();
                    break;
                case anti_entropy::Phase::Recovering:
                    this->phase_recovering();
                    break;
                case anti_entropy::Phase::Operating:
                    this->phase_operating();
                    break;
                default:
                    break;
            }
        }
    }

    close(this->sender_socket);
}

void anti_entropy::stop_thread() {
    this->running = false;
}

