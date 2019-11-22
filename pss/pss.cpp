//
// Created by danielsf97 on 10/8/19.
//

#include <iostream>
#include <fstream>
#include "pss.h"
#include "../tcp_client_server_connection/tcp_client_server_connection.h"
#include "../serializer/capnp/capnp_serializer.h"
#include "pss_message.pb.h"
#include <thread>
#include <chrono>         // std::chrono::seconds
#include <algorithm>    // std::random_shuffle
#include <random>      // std::rand, std::srand
#include <mutex>
#include <nlohmann/json.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>

using json = nlohmann::json;
#define LOG(X) std::cout << X << std::endl;


//my_port é necessário para poder enviar a nossa identidade (que é a porta)
//no futuro, com endereços, não será necessário.
pss::pss(const char *boot_ip, int boot_port, std::string my_ip, int my_port)
{
    //TODO acrescentar loop para tentar reconexão caso falhe


//    try {
        std::unique_ptr<Capnp_Serializer> capnp_serializer(new Capnp_Serializer);
        tcp_client_server_connection::tcp_client_connection connection(boot_ip, boot_port, std::move(capnp_serializer));

        //sending announce msg
        pss_message pss_announce_msg;
        pss_announce_msg.sender_ip = my_ip;
        pss_announce_msg.sender_port = my_port;
        pss_announce_msg.type = pss_message::Type::Announce;
        connection.send_pss_msg(pss_announce_msg);

        //receiving view from bootstrapper
        bool view_recv = false;
        pss_message pss_view_msg_rcv;
        while (!view_recv) {
            connection.recv_pss_msg(pss_view_msg_rcv);

            if (pss_view_msg_rcv.type == pss_message::Type::Normal)
                view_recv = true;
        }

        //process received view
        for (peer_data &peer : pss_view_msg_rcv.view) {
            this->view.insert(std::make_pair(peer.port, std::move(peer)));
        }

//    }catch(const char* e){
//        std::cerr << "############################################" <<std::endl;
//        std::cerr << e << std::endl;
//    }

}

pss::pss(const char *boot_ip, int boot_port, std::string my_ip, int my_port, long boot_time, int view_size, int sleep, int gossip_size):
    pss::pss(boot_ip, boot_port, my_ip, my_port)
{
    this->running = true;
    this->sleep_interval = sleep;
    this->boot_time = boot_time;
    this->view_size = view_size;
    this->gossip_size = gossip_size;
    this->ip = my_ip;
    this->port = my_port;
    this->boot_ip = boot_ip;
    this->boot_port = boot_port;
}

std::vector<peer_data> pss::select_view_to_send(int target_port) {
    std::vector<peer_data> res;
    peer_data myself = {
            this->ip,
            this->port,
            0
    };

    res.push_back(std::move(myself));

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

    std::vector<peer_data> my_view_elements;
    for(auto& [port, peer]: this->view){
        if(port != target_port) {
            my_view_elements.push_back(peer);
        }
    }

    auto rng = std::default_random_engine {};
    std::shuffle(std::begin(my_view_elements), std::end(my_view_elements), rng);

    for(peer_data& peer: my_view_elements){
        if(res.size() < this->gossip_size){
            res.push_back(std::move(peer));
            this->view.erase(peer.port);
        }
    }

    return res;
}

void pss::send_pss_msg(int target_port, std::vector<peer_data>& view_to_send, proto::pss_message_Type msg_type){
    //TODO lidar com o caso de não conseguir conexão
    try {
//        std::cerr << "[pss] function: send_response_msg [Creating Connection]" << std::endl;

        struct sockaddr_in serverAddr;
        socklen_t addr_size;
        int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_port);
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        proto::pss_message pss_message;
        pss_message.set_sender_ip(this->ip);
        pss_message.set_sender_port(this->port);
        pss_message.set_type(msg_type);

        for(auto& peer: view_to_send){
            proto::peer_data* peer_data = pss_message.add_view();
            peer_data->set_ip(peer.ip);
            peer_data->set_port(peer.port);
            peer_data->set_age(peer.age);
        }

        std::string buf;
        pss_message.SerializeToString(&buf);

        sendto(sockfd, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    }catch(...){std::cout <<"=============================== NÂO consegui enviar =================" << std::endl;}
}

void pss::operator()() {
    this->running = true;
    long time = 0; //for logging purposes

    std::this_thread::sleep_for (std::chrono::seconds(this->boot_time));

    while(this->running){
        std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
        if(this->running){  //if the peer didnt stop while pss was sleeping
            time = time + this->sleep_interval;

            std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);

            this->incorporate_last_sent_view();
            this->age_view_members();
            peer_data* target_ptr = this->get_older_from_view(); //pair (port, age)

            if(target_ptr != nullptr){
                peer_data target = *target_ptr;
                this->view.erase(target.port);
                std::vector<peer_data> view_to_send = this->select_view_to_send(target.port); //older_member.first = target_port

                this->last_sent_view = std::vector<peer_data>();
                for(peer_data& peer : view_to_send){
                    if(peer.port != this->port)
                        this->last_sent_view.push_back(peer);
                }

                this->send_pss_msg(target.port, view_to_send, proto::pss_message_Type::pss_message_Type_NORMAL);
            }
        }
    }
    LOG("END PSS thread")
}

void pss::print_view() {
    std::cout << "====== My View[" + std::to_string(this->port) + "] ====" << std::endl;
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto const& [key, peer] : this->view){
        std::cout << peer.ip << "(" << peer.port << ") : " << peer.age << std::endl;
    }
    std::cout << "==========================" << std::endl;
}

//void pss::write_view_to_file(){
//    std::string filename = "../logging/" + std::to_string(this->port) + ".json";
//    std::ofstream file;
//    file.open(filename);
//    if(file.is_open()){
//        std::vector<int> peers_from_view;
//        for(auto& [port, peer]: this->view)
//            peers_from_view.push_back(port);
//        json j = {};
//        j["peer"] = this->port;
//        j["view"] = peers_from_view;
//        file << j;
//        file.close();
//    }
//}

void pss::age_view_members() {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto& [port, peer]: this->view){
        peer.age += 1;
    }
}

peer_data* pss::get_older_from_view() {
    peer_data* older_peer = nullptr;

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto& [port, peer] : this->view){
        if(older_peer == nullptr || peer.age > (*older_peer).age){
            older_peer = &peer;
        }
    }

    return older_peer;
}

void pss::process_msg(proto::pss_message pss_msg){

    std::vector<peer_data> recv_view;
    for(auto& peer: pss_msg.view()){
        peer_data peer_data;
        peer_data.ip = peer.ip();
        peer_data.port = peer.port();
        peer_data.age = peer.age();
        recv_view.push_back(peer_data);
    }

    if(pss_msg.type() == proto::pss_message_Type::pss_message_Type_NORMAL){
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
        this->incorporate_last_sent_view();
        //1- seleciona uma vista para enviar (removendo tais nodos da sua vista, isto
        // porque desta forma consegue arranjar espaço para acomodar os nodos que recebeu)
        std::vector<peer_data> view_to_send = this->select_view_to_send(pss_msg.sender_port());
        //2- desses nodos que selecionou para enviar armazena os diferentes do proprio porque pode ser necessário
        //completar a vista, dado que pode ter recebido menos nodos dos que enviou
        std::vector<peer_data> to_fill_view;
        for(peer_data peer: view_to_send){
            if(peer.port != this->port)
                to_fill_view.push_back(peer);
        }
        //3- insere as vistas, começando pela que recebeu primeiro
        this->incorporate_in_view(recv_view);
        this->incorporate_in_view(to_fill_view);

//        lk.unlock();

        //4- envia msg de resposta
        this->send_pss_msg(pss_msg.sender_port(), view_to_send, proto::pss_message_Type::pss_message_Type_RESPONSE);
        this->print_view();

    }else if (pss_msg.type() == proto::pss_message_Type::pss_message_Type_RESPONSE){
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);

        this->incorporate_in_view(recv_view);
        this->incorporate_last_sent_view();
        this->print_view();

// penso que é desnecessário
//        while(this->view.size() > this->view_size){
//            this->remove_older_from_view();
//        }

        this->last_sent_view = std::vector<peer_data>();
    }
}

void pss::incorporate_in_view(std::vector<peer_data>& source) {
    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    while(this->view.size() < this->view_size && source.size() > 0){
        peer_data tmp = source.front();
        source.erase(source.begin());
        if(tmp.port != this->port){
            auto current_it = this->view.find(tmp.port);
            if(current_it != this->view.end()){ //o elemento existe no mapa
                int current_age = current_it->second.age;
                if(current_age > tmp.age)
                    current_it->second = tmp;

            }else{
                this->view.insert(std::make_pair(tmp.port, tmp));
            }
        }
    }
}

void pss::complete_view_with_last_sent() {
    //enquanto que a vista não estiver completa e ainda houver elementos na ultima vista enviada

    std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
    auto it = this->last_sent_view.begin();

    while(this->view.size() < this->view_size && this->last_sent_view.size() > 0){

        this->view.insert(std::make_pair(it->port, *it));
        it = this->last_sent_view.erase(it);
    }

}

void pss::incorporate_last_sent_view() {
    std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
    while(this->view.size() < this->view_size && this->last_sent_view.size() > 0){
        peer_data tmp = this->last_sent_view.front();
        this->view.insert(std::make_pair(tmp.port, tmp));
        this->last_sent_view.erase(this->last_sent_view.begin());
    }
}

std::vector<int> pss::get_peers_from_view() {
    std::vector<int> res;

    std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
    for (auto&[port, peer]: this->view) {
        res.push_back(port);
    }

    auto it = this->last_sent_view.begin();
    while (res.size() < this->view_size && it != this->last_sent_view.end()) {
        res.push_back(it->port);
        ++it;
    }

    return std::move(res);
}

void pss::stop_thread() {
    LOG("Stopping PSS thread");
    this->running = false;
    this->incorporate_last_sent_view();
}

void pss::bootstrapper_termination_alerting() {
    try {
        std::unique_ptr<Capnp_Serializer> capnp_serializer(new Capnp_Serializer);
        tcp_client_server_connection::tcp_client_connection connection(this->boot_ip, this->boot_port,
                                                                       std::move(capnp_serializer));

        //sending announce msg
        pss_message pss_termination_msg;
        pss_termination_msg.sender_ip = this->ip;
        pss_termination_msg.sender_port = this->port;
        pss_termination_msg.type = pss_message::Type::Termination;
        connection.send_pss_msg(pss_termination_msg);
    }catch(const char* e){
        std::cerr << "Erro ao alertar o bootstrapper =========================================================================================================================";
        LOG(e);
    }
}


