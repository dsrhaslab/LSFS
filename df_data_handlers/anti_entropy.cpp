//
// Created by danielsf97 on 1/30/20.
//

#include <df_serializer/capnp/capnp_serializer.h>
#include <df_tcp_client_server_connection/tcp_client_server_connection.h>
#include <thread>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "anti_entropy.h"

anti_entropy::anti_entropy(std::string ip, int respond_to_port, long id, pss *pss_ptr,
        std::shared_ptr<kv_store<std::string>> store, long sleep_interval): ip(std::move(ip)), respond_to_port(respond_to_port), id(id), pss_ptr(pss_ptr),
        store(std::move(store)), sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
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
        serverAddr.sin_port = htons(peer.port + 1);
        serverAddr.sin_addr.s_addr = inet_addr(peer.ip.c_str());

        try {
            int res = sendto(this->sender_socket, buf_data, buf_size, 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

            if(res == -1){printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));}
        }catch(...){std::cout <<"=============================== NÂO consegui enviar =================" << std::endl;}
    }
}

void anti_entropy::operator()() {
    this->running = true;

    while(this->running){
        std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
        if(this->running){
            std::vector<peer_data> slice_view = pss_ptr->get_slice_local_view();

            proto::kv_message message;
            auto *message_content = new proto::anti_entropy_message();
            message_content->set_ip(this->ip);
            message_content->set_port(this->respond_to_port);
            message_content->set_id(this->id);
            for(auto& key : this->store->get_keys()){
                proto::kv_store_key* kv_key = message_content->add_keys();
                kv_key->set_key(key.key);
                kv_key->set_version(key.version);
            }
            message.set_allocated_anti_entropy_msg(message_content);
            this->send_peer_keys(slice_view, message);
        }
    }

    close(this->sender_socket);
}

void anti_entropy::stop_thread() {
    this->running = false;
}

