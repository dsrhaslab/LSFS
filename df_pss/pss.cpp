//
// Created by danielsf97 on 10/8/19.
//

#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include "pss.h"
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"
#include "pss_message.pb.h"
#include <thread>
#include <chrono>         // std::chrono::seconds
#include <algorithm>    // std::random_shuffle
#include <random>      // std::rand, std::srand
#include <mutex>
#include <nlohmann/json.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <df_core/peer.h>


using json = nlohmann::json;
#define LOG(X) std::cout << X << std::endl;

pss::pss(const char *boot_ip, std::string my_ip, int kv_port, int pss_port, int recover_port, long my_id, double my_pos)
{
    bool recovered = false;

    while(!recovered){
        try {
            tcp_client_server_connection::tcp_client_connection connection(boot_ip, peer::boot_port);

            proto::pss_message pss_announce_msg;
            pss_announce_msg.set_type(proto::pss_message_Type::pss_message_Type_ANNOUNCE);
            pss_announce_msg.set_sender_ip(my_ip);
            pss_announce_msg.set_sender_pss_port(pss_port);
            pss_announce_msg.set_sender_pos(my_pos); // not used

            proto::peer_data* peer_data = pss_announce_msg.add_view();
            peer_data->set_ip(my_ip);
            peer_data->set_kv_port(kv_port);
            peer_data->set_pss_port(pss_port);
            peer_data->set_recover_port(recover_port);
            peer_data->set_age(0);
            peer_data->set_id(my_id);
            peer_data->set_pos(my_pos);
            peer_data->set_nr_slices(0); // not used
            peer_data->set_slice(0); // not used

            std::string buf;
            pss_announce_msg.SerializeToString(&buf);

            connection.send_msg(buf.data(), buf.size());

            //receiving view from df_bootstrapper
            bool view_recv = false;
            proto::pss_message pss_view_msg_rcv;
            char rcv_buf [65500];

            while (!view_recv) {
                int bytes_rcv = connection.recv_msg(rcv_buf); //throw exception

                pss_view_msg_rcv.ParseFromArray(rcv_buf, bytes_rcv);

                if (pss_view_msg_rcv.type() == proto::pss_message_Type::pss_message_Type_NORMAL)
                    view_recv = true;
            }

            recovered = true;

            //process received view
            for (auto& peer : pss_view_msg_rcv.view()) {
                struct peer_data peer_rcv;
                peer_rcv.ip = peer.ip();
                peer_rcv.kv_port = peer.kv_port();
                peer_rcv.pss_port = peer.pss_port();
                peer_rcv.recover_port = peer.recover_port();
                peer_rcv.age = peer.age();
                peer_rcv.id = peer.id();
                peer_rcv.slice = peer.slice();
                peer_rcv.nr_slices = peer.nr_slices();
                peer_rcv.pos = peer.pos();
                this->view.insert(std::make_pair(peer.ip(), std::move(peer_rcv)));
            }

        }catch(const char* e){
            spdlog::error(e);
        }catch(...){}
    }

}

pss::pss(const char *boot_ip, std::string my_ip, int kv_port, int pss_port, int recover_port, long id, double pos, long boot_time, int view_size, int sleep, int gossip_size, group_construction* group_c):
    pss::pss(boot_ip, my_ip, kv_port, pss_port, recover_port, id, pos)
{
    this->running = true;
    this->sleep_interval = sleep;
    this->boot_time = boot_time;
    this->view_size = view_size;
    this->gossip_size = gossip_size;
    this->ip = my_ip;
    this->kv_port = kv_port;
    this->pss_port = pss_port;
    this->recover_port = recover_port;
    this->id = id;
    this->pos = pos;
    this->boot_ip = boot_ip;
    this->socket_send = socket(PF_INET, SOCK_DGRAM, 0);
    this->group_c = group_c;
}

std::vector<peer_data> pss::select_view_to_send(std::string target_ip) {
    std::vector<peer_data> res;
    peer_data myself = {
            this->ip,
            this->kv_port,
            this->pss_port,
            this->recover_port,
            0,
            this->id,
            this->group_c->get_nr_groups(),
            this->group_c->get_position(),
            this->group_c->get_my_group()
    };

    res.push_back(std::move(myself));

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

    std::vector<peer_data> my_view_elements;
    for(auto& [ip, peer]: this->view){
        if(ip != target_ip) {
            my_view_elements.push_back(peer);
        }
    }

    auto rng = std::default_random_engine {};
    std::shuffle(std::begin(my_view_elements), std::end(my_view_elements), rng);

    for(peer_data& peer: my_view_elements){
        if(res.size() < this->gossip_size){
            this->view.erase(peer.ip);
            res.push_back(std::move(peer));
        }
    }
    return res;
}

std::vector<peer_data> pss::have_peer_from_slice(int slice){
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

    std::vector<peer_data> res;
    for(auto& [ip, peer]: this->view){
        if(this->group_c->group(peer.pos) == slice){
            res.push_back(peer);
        }
    }
    return res;
}

std::vector<peer_data> pss::get_view(){
    std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);

    std::vector<peer_data> res;

    for(auto& [ip, peer]: this->view){
        res.push_back(peer);
    }
    for(auto& peer: this->last_sent_view){
        res.push_back(peer);
    }
    return res;
}

void pss::forward_pss_msg(const std::string& target_ip, const int target_pss_port, const proto::pss_message& pss_message) {
    try {
        struct sockaddr_in serverAddr;
        socklen_t addr_size;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_pss_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_ip.c_str());

        std::string buf;
        pss_message.SerializeToString(&buf);

        //socket synchronization
        std::scoped_lock<std::recursive_mutex> lk (this->socket_send_mutex);
        int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        }
    }catch(...){
        spdlog::error("====================== Não consegui enviar ======================");
    }
}

void pss::send_pss_msg(const std::string& target_ip, const int target_pss_port, std::vector<peer_data>& view_to_send, proto::pss_message_Type msg_type){
    try {
        struct sockaddr_in serverAddr;
        socklen_t addr_size;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_pss_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_ip.c_str());

        proto::pss_message pss_message;
        pss_message.set_sender_ip(this->ip);
        pss_message.set_sender_pss_port(this->pss_port);
        pss_message.set_sender_pos(this->pos);
        pss_message.set_type(msg_type);

        for(auto& peer: view_to_send){
            proto::peer_data* peer_data = pss_message.add_view();
            peer_data->set_ip(peer.ip);
            peer_data->set_kv_port(peer.kv_port);
            peer_data->set_pss_port(peer.pss_port);
            peer_data->set_recover_port(peer.recover_port);
            peer_data->set_age(peer.age);
            peer_data->set_id(peer.id);
            peer_data->set_pos(peer.pos);
            peer_data->set_nr_slices(peer.nr_slices);
            peer_data->set_slice(peer.slice);
        }

        std::string buf;
        pss_message.SerializeToString(&buf);

        //socket synchronization
        std::scoped_lock<std::recursive_mutex> lk (this->socket_send_mutex);
        int res = sendto(this->socket_send, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        }
    }catch(...){
        std::cout <<"====================== Não consegui enviar ======================" << std::endl;
    }
}

void pss::operator()() {
    this->running = true;
    long time = 0; //for logging purposes

    std::this_thread::sleep_for (std::chrono::seconds(this->boot_time));

    int cycles = 0;
    while(this->running){
        std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
        if(this->running){  //if the peer didnt stop while df_pss was sleeping
            time = time + this->sleep_interval;
            cycles = cycles + 1;
            this->group_c->set_cycle(cycles);

            std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);

            this->incorporate_last_sent_view();
            this->age_view_members();
            peer_data* target_ptr = this->get_older_from_view();

            if(target_ptr != nullptr){
                peer_data target = *target_ptr;
                this->view.erase(target.ip);
                std::vector<peer_data> view_to_send = this->select_view_to_send(target.ip);

                this->last_sent_view = std::vector<peer_data>();
                for(peer_data& peer : view_to_send){
                    if(peer.ip != this->ip)
                        this->last_sent_view.push_back(peer);
                }

                this->send_pss_msg(target.ip, target.pss_port, view_to_send, proto::pss_message_Type::pss_message_Type_NORMAL);
            }
        }
    }
}

void pss::print_view() {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    std::cout << "View: ";
    for(auto const& [key, peer] : this->view){
        std::cout << peer.id << " ";
    }
    std::cout << std::endl;
}

int pss::get_my_group() {
    return this->group_c->get_my_group();
}

int pss::get_nr_groups(){
    return this->group_c->get_nr_groups();
}

void pss::age_view_members() {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto& [ip, peer]: this->view){
        peer.age += 1;
    }
}

peer_data* pss::get_older_from_view() {
    peer_data* older_peer = nullptr;

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto& [ip, peer] : this->view){
        if(older_peer == nullptr || peer.age > (*older_peer).age){
            older_peer = &peer;
        }
    }

    return older_peer;
}

void pss::process_msg(const proto::pss_message& pss_msg){

    if(pss_msg.type() == proto::pss_message_Type::pss_message_Type_REQUEST_LOCAL){

        int slice = this->group_c->group(pss_msg.sender_pos());
        if(this->group_c->get_my_group() == slice){
            this->group_c->send_local_message(const_cast<std::string &>(pss_msg.sender_ip()), pss_msg.sender_pss_port());
        }else{
            std::vector<peer_data> peers_from_slice = have_peer_from_slice(slice);
            for(peer_data& peer: peers_from_slice){
                this->forward_pss_msg(peer.ip, peer.pss_port, pss_msg);
            }
        }
        return;
    }

    std::vector<peer_data> recv_view;
    for(auto& peer: pss_msg.view()){
        peer_data peer_data;
        peer_data.ip = peer.ip();
        peer_data.kv_port = peer.kv_port();
        peer_data.pss_port = peer.pss_port();
        peer_data.age = peer.age();
        peer_data.id = peer.id();
        peer_data.slice = peer.slice();
        peer_data.nr_slices = peer.nr_slices();
        peer_data.pos = peer.pos();
        recv_view.push_back(peer_data);
    }

    if(pss_msg.type() == proto::pss_message_Type::pss_message_Type_LOCAL){
        this->group_c->receive_local_message(recv_view);
    }else if(pss_msg.type() == proto::pss_message_Type::pss_message_Type_LOADBALANCE){
        std::vector<peer_data> current_view = this->get_view();

        peer_data myself = {
                this->ip,
                this->kv_port,
                this->pss_port,
                0,
                this->id,
                this->group_c->get_nr_groups(),
                this->group_c->get_position(),
                this->group_c->get_my_group()
        };

        current_view.push_back(std::move(myself));

        this->send_pss_msg(pss_msg.sender_ip(), pss_msg.sender_pss_port(), current_view, proto::pss_message_Type::pss_message_Type_LOADBALANCE);
    }else if(pss_msg.type() == proto::pss_message_Type::pss_message_Type_LOADBALANCE_LOCAL){
        std::string target_ip = pss_msg.sender_ip();
        this->group_c->send_local_message(target_ip, pss_msg.sender_pss_port());
    }else if(pss_msg.type() == proto::pss_message_Type::pss_message_Type_NORMAL){
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
        this->incorporate_last_sent_view();

        // 1- select a view to send and remove selected nodes from current node view
        // because we need to make space for the received nodes
        std::vector<peer_data> view_to_send = this->select_view_to_send(pss_msg.sender_ip());

        // 2- from the selected nodes to send, copy them as they may be useful to complete
        // the node view in case of receiving less nodes than the sent ones
        std::vector<peer_data> to_fill_view;
        for(peer_data peer: view_to_send){
            if(peer.ip != this->ip)
                to_fill_view.push_back(peer);
        }

        // 3- incorporate views nodes, starting from the first received view
        this->incorporate_in_view(recv_view);
        this->incorporate_in_view(to_fill_view);

        // 4- send reply message
        this->send_pss_msg(pss_msg.sender_ip(), pss_msg.sender_pss_port(), view_to_send, proto::pss_message_Type::pss_message_Type_RESPONSE);

        // 5- passing received peers to group construction
        this->group_c->receive_message(recv_view);

    }else if (pss_msg.type() == proto::pss_message_Type::pss_message_Type_RESPONSE){
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
        this->incorporate_in_view(recv_view);
        this->incorporate_last_sent_view();
        this->last_sent_view = std::vector<peer_data>();
        this->group_c->receive_message(recv_view);
    }
}

void pss::incorporate_in_view(std::vector<peer_data> source) {
    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);

    while(this->view.size() < this->view_size && !source.empty()){
        peer_data tmp = source.front();
        source.erase(source.begin());
        if(tmp.ip != this->ip){
            auto current_it = this->view.find(tmp.ip);
            if(current_it != this->view.end()){
                int current_age = current_it->second.age;
                if(current_age > tmp.age) {
                    current_it->second = tmp;
                }
            }else{
                this->view.insert(std::make_pair(tmp.ip, tmp));
            }
        }
    }
}

void pss::complete_view_with_last_sent() {
    // while the node view is not completed and still exists nodes from the last sent view
    std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
    auto it = this->last_sent_view.begin();

    while(this->view.size() < this->view_size && this->last_sent_view.size() > 0){

        this->view.insert(std::make_pair(it->ip, *it));
        it = this->last_sent_view.erase(it);
    }

}

void pss::incorporate_last_sent_view() {
    std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
    while(this->view.size() < this->view_size && this->last_sent_view.size() > 0){
        peer_data tmp = this->last_sent_view.front();
        this->view.insert(std::make_pair(tmp.ip, tmp));
        this->last_sent_view.erase(this->last_sent_view.begin());
    }
    print_view();
}

std::vector<long> pss::get_peers_from_view() {
    std::vector<long> res;

    std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->view_mutex, this->last_view_mutex);
    for (auto&[ip, peer]: this->view) {
        res.push_back(/*port*/ peer.id);
    }

    auto it = this->last_sent_view.begin();
    while (res.size() < this->view_size && it != this->last_sent_view.end()) {
        res.push_back(it->id);
        ++it;
    }

    return std::move(res);
}

std::vector<long> pss::get_group_view() {
    auto peers =  this->group_c->get_local_view();
    std::vector<long> res;
    for(auto& peer: peers)
        res.push_back(peer.id);
    return std::move(res);
}

void pss::stop_thread() {
    this->running = false;
    this->incorporate_last_sent_view();
    close(this->socket_send);
    this->bootstrapper_termination_alerting();
}

void pss::bootstrapper_termination_alerting() {
    try {
        tcp_client_server_connection::tcp_client_connection connection(this->boot_ip, peer::boot_port);

        //sending termination msg
        proto::pss_message msg_to_send;
        msg_to_send.set_type(proto::pss_message_Type::pss_message_Type_TERMINATION);
        msg_to_send.set_sender_ip(this->ip);
        msg_to_send.set_sender_pos(0); // not used

        std::string buf;
        msg_to_send.SerializeToString(&buf);

        connection.send_msg(buf.data(), buf.size());
    }catch(const char* e){
        spdlog::error(e);
    }
}

double pss::get_position() {
    return this->group_c->get_position();
}

std::vector<peer_data> pss::get_slice_local_view() {
    return std::move(this->group_c->get_local_view());
}


