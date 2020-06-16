//
// Created by danielsf97 on 4/5/20.
//

#include <spdlog/spdlog.h>
#include "smart_load_balancer.h"
#include "df_serializer/capnp/capnp_serializer.h"
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/yaml.h>
#include <boost/concept_check.hpp>
#include <df_client/client.h>

smart_load_balancer::smart_load_balancer(std::string boot_ip/*, int boot_port*/, std::string ip/*, int port*/,long sleep_interval):
        ip(ip)/*, port(port)*/, sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
{
    std::random_device rd;     // only used once to initialise (seed) engine
    this->random_eng = std::mt19937(rd());

    bool recovered = false;
    std::shared_ptr<Capnp_Serializer> capnp_serializer(new Capnp_Serializer);

    YAML::Node config = YAML::LoadFile("../scripts/conf.yaml");
    auto main_confs = config["main_confs"];
    this->replication_factor_max = main_confs["rep_max"].as<int>();
    this->replication_factor_min = main_confs["rep_min"].as<int>();
    this->max_age = main_confs["max_age"].as<int>();
    this->first_message = true;
    this->local = main_confs["local_message"].as<bool>();
    this->local_interval = 5;//main_confs["local_interval_sec"].as<int>();

    std::uniform_real_distribution<double> dist(0, 1);
    this->position = dist(random_eng);
    this->nr_groups = 1;
    this->my_group = 1;
    this->cycle = 1;


    while(!recovered){
        try {
            tcp_client_server_connection::tcp_client_connection connection(boot_ip.c_str(), client::lb_port, capnp_serializer);

            //sending announce msg
            pss_message pss_get_view_msg;
            pss_get_view_msg.sender_ip = ip;
            //pss_get_view_msg.sender_port = port;
            pss_get_view_msg.type = pss_message::Type::GetView;
            connection.send_pss_msg(pss_get_view_msg);

            //receiving view from df_bootstrapper
            bool view_recv = false;
            pss_message pss_view_msg_rcv;
            while (!view_recv) {
                connection.recv_pss_msg(pss_view_msg_rcv);

                if (pss_view_msg_rcv.type == pss_message::Type::Normal)
                    view_recv = true;
            }

            recovered = true;

            std::cout << "Things are starting" << std::endl;

            std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
            auto temp_group = std::make_unique<std::vector<peer_data>>(std::move(pss_view_msg_rcv.view));
            this->view.push_back(std::move(temp_group));
        }catch(const char* e){
            spdlog::error(e);
//            std::cout << e << std::endl;
        }catch(...){}
    }
}


int smart_load_balancer::group(double peer_pos){
    int temp = (int) ceil((static_cast<double>(this->nr_groups))*peer_pos);
    if(temp == 0){
        temp = 1;
    }
    return temp;
}

void insert_peer_data_with_order(std::unique_ptr<std::vector<peer_data>>& view, peer_data peer){
    bool inserted = false;

    for(auto it = view->begin(); it != view->end(); ++it){
        if(it->ip != peer.ip/*it->port == peer.port*/){
            if(it->age > peer.age)
                it->age = peer.age;
            return;
        }
    }

    for(auto it = view->begin(); it != view->end(); ++it){
        if(it->age >= peer.age){
            view->insert(it, std::move(peer));
            inserted = true;
            break;
        }
    }
    if(!inserted){
        view->push_back(std::move(peer));
    }
}

void smart_load_balancer::merge_groups_from_view() {
    std::vector<std::unique_ptr<std::vector<peer_data>>> next_view;
    if(this->view.size() == 1) return;
    for(auto it = this->view.begin(); it != this->view.end() && (it+1) != this->view.end(); it+=2){
        for(peer_data& peer: **(it+1)){
            insert_peer_data_with_order(*it, peer);
        }
        it->get()->resize(std::min((*it)->size() , (unsigned long) nr_saved_peers_by_group));
        next_view.push_back(std::move(*it));
    }
    this->view = std::move(next_view);
}

void smart_load_balancer::split_groups_from_view() {
    std::vector<std::unique_ptr<std::vector<peer_data>>> next_view;
    int current_group = 1;
    for(auto& it : this->view){
        auto next_group = std::make_unique<std::vector<peer_data>>();
        for(auto peer_it = it->begin(); peer_it != it->end();){
            if(group(peer_it->pos) == current_group + 1){
                insert_peer_data_with_order(next_group, *peer_it);
                peer_it = it->erase(peer_it);
            }else{
                ++peer_it;
            }
        }
        next_view.push_back(std::move(it));
        next_view.push_back(std::move(next_group));
    }
    this->view = std::move(next_view);
}


void smart_load_balancer::receive_message(std::vector<peer_data> received) {

    std::scoped_lock<std::recursive_mutex> lk_local (this->local_view_mutex);

    //When a peer boots it must keep up with peers
    int nr_groups_from_peers = 1;

    //AGING LOCAL VIEW
    for(auto& [/*port*/ ip, peer]: this->local_view){
        peer.age += 1;
    }

    std::vector<peer_data> not_added;

    //ADD RECEIVED
    for (peer_data& peer: received){
        if(this->first_message){
            if(peer.nr_slices > this->nr_groups && peer.nr_slices > nr_groups_from_peers){
                nr_groups_from_peers = peer.nr_slices;
            }
        }
        if(group(peer.pos) == this->my_group){
            auto current_it = this->local_view.find(peer.ip/*peer.port*/);
            if(current_it != this->local_view.end()){ //o elemento existe no mapa
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second = peer;

            }else{
                this->local_view.insert(std::make_pair(peer.ip /*peer.port*/, peer));
            }
        }else{
            not_added.push_back(peer);
        }
    }

    //CLEAN LOCAL VIEW
    std::vector</*int*/ std::string> to_rem;
    for (auto& [ip /*port*/,peer] : this->local_view){
        if(group(peer.pos) != this->my_group){
            to_rem.push_back(ip /*port*/);
            not_added.push_back(peer);
        }
        else{
            if(peer.age > max_age){
                to_rem.push_back(ip /*port*/);
            }
        }
    }
    for(/*int port*/ std::string ip : to_rem){
        this->local_view.erase(ip /*port*/);
    }

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

    if(this->first_message){
        this->nr_groups = nr_groups_from_peers;
        std::cout << "Nr_groups: " << this->nr_groups << std::endl;
        this->first_message = false;
        this->view.clear();
        for (int i = 0; i < nr_groups_from_peers; i++){
            this->view.push_back(std::make_unique<std::vector<peer_data>>());
        }
    }else{
        //AGING VIEW
        for(auto& group_view : this->view){
            for(auto& peer: *group_view){
                peer.age += 1;
            }
        }

        //SEARCH FOR VIOLATIONS
        int estimation = this->local_view.size(); //countEqual();

        if(estimation < this->replication_factor_min){
            if(this->nr_groups > 1){
                this->nr_groups = this->nr_groups/2;
                std::cout << "Estimation: " << estimation << "Nr_groups: " << this->nr_groups << std::endl;
                this->merge_groups_from_view();
            }
        }
        if(estimation > this->replication_factor_max){
            this->nr_groups = this->nr_groups*2;
            std::cout << "Estimation: " << estimation << "Nr_groups: " << this->nr_groups << std::endl;
            this->split_groups_from_view();
        }
    }

    this->my_group = group(this->position);

    //Check if none of the peers received or from the ones that did not belong
    //to the current group match now the group in question
    for(auto& peer : not_added){
        if(group(peer.pos) == this->my_group) {
            auto current_it = this->local_view.find(peer.ip/*peer.port*/);
            if(current_it != this->local_view.end()){ //o elemento existe no mapa
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second = peer;
            }else{
                this->local_view.insert(std::make_pair(peer.ip /*peer.port*/, peer));
            }
        }
    }

    for(auto& peer: received){
        insert_peer_data_with_order(this->view[group(peer.pos)-1], peer);
    }

    //CLEAN VIEW
    for(auto& per_group_view : this->view) {
        int nr_elem_to_keep = 0;
        for (auto it = per_group_view->begin(); it != per_group_view->end(); ++it) {
            if (it->age < max_age) {
                nr_elem_to_keep++;
            } else {
                break;
            }
        }
        per_group_view->resize(std::min(this->nr_saved_peers_by_group, nr_elem_to_keep));
    }
}

peer_data smart_load_balancer::get_random_peer() {
    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);

    bool done = false;
    std::uniform_int_distribution<int> uni(0, nr_groups - 1); // guaranteed unbiased
    while(!done) {
        int group_to_send = uni(random_eng);
        auto slice_group = this->view[group_to_send].get();
        int slice_size = slice_group->size();
        if(slice_size == 0){
            continue;
        }else if(slice_size == 1){
            return slice_group->front();
        }else{
            std::uniform_int_distribution<int> dist_slice(0, slice_size - 1);
            return slice_group->at(dist_slice(random_eng));
        }
    }
}

peer_data smart_load_balancer::get_random_local_peer() {
    std::scoped_lock<std::recursive_mutex> lk (this->local_view_mutex);
    int local_view_size = this->local_view.size();
    if(local_view_size == 0) throw "EMPTY VIEW EXCEPTION";
    std::uniform_int_distribution<int> uni(0, local_view_size - 1); // guaranteed unbiased
    int peer_idx = uni(random_eng);
    auto it = this->local_view.begin();
    for(int i = 0; i < peer_idx; i++){
        ++it;
    }
    return it->second;
}

peer_data smart_load_balancer::get_peer(const std::string& key) {
    int key_group__nr;
    if(this->nr_groups == 1) key_group__nr = 1;

    size_t max = SIZE_MAX;
    size_t min = 0;
    size_t target = std::hash<std::string>()(key);
    size_t step = (max / this->nr_groups);

    size_t current = min;
    int slice = 1;
    size_t next_current = current + step;

    while (target > next_current){
        current = next_current;
        next_current = current + step;
        if(current > 0 && next_current < 0) break; //in the case of overflow
        slice = slice + 1;
    }

    if(slice > this->nr_groups){
        slice = this->nr_groups - 1;
    }

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    auto slice_group = this->view[slice - 1].get();
    int slice_size = slice_group->size();
    if(slice_size == 0){
        return get_random_peer();
    }else if(slice_size == 1){
        //TODO meter a gerar um numero random e numa probabilidade
        //TODO de 50-50 sortear entre o slice_group->front() e o get_random_peer();
        return slice_group->front();
    }else{
        std::uniform_real_distribution<double> dist(0, slice_size - 1);
        return slice_group->at(dist(random_eng));
    }
}

void smart_load_balancer::receive_local_message(std::vector<peer_data> received) {
    std::scoped_lock<std::recursive_mutex> lk (this->local_view_mutex);
    for(peer_data& peer: received){
        if(group(peer.pos) == this->my_group){
            auto current_it = this->local_view.find(peer.ip /*peer.port*/);
            if(current_it != this->local_view.end()){ //o elemento existe no mapa
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second.age = peer.age;

            }else{
                this->local_view.insert(std::make_pair(peer.ip /*peer.port*/, peer));
            }
        }
    }
}

void smart_load_balancer::process_msg(proto::pss_message &pss_msg) {
    std::vector<peer_data> recv_view;
    for(auto& peer: pss_msg.view()){
        peer_data peer_data;
        peer_data.ip = peer.ip();
        //peer_data.port = peer.port();
        peer_data.age = peer.age();
        peer_data.id = peer.id();
        peer_data.slice = peer.slice();
        peer_data.nr_slices = peer.nr_slices();
        peer_data.pos = peer.pos();
        recv_view.push_back(peer_data);
    }

    if(pss_msg.type() == proto::pss_message_Type::pss_message_Type_LOCAL){
        this->receive_local_message(std::move(recv_view));
    }else{
        this->receive_message(std::move(recv_view));
    }
}

void smart_load_balancer::operator()() {
    this->running = true;

    while(this->running){
        std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
        this->cycle++;
        if(this->running){
            try{
                if(this->local && (this->cycle % this->local_interval == 0)){
                    //send local message
                    peer_data target_peer = this->get_random_local_peer(); //pair (port, age)

                    proto::pss_message pss_message;
                    pss_message.set_sender_ip(this->ip);
                    pss_message.set_sender_pos(this->position);
                    pss_message.set_type(proto::pss_message::Type::pss_message_Type_LOADBALANCE_LOCAL);
                    this->send_msg(target_peer, pss_message);
                }else{
                    //send loadbalance message
                    peer_data target_peer = this->get_random_peer(); //pair (port, age)

                    proto::pss_message pss_message;
                    pss_message.set_sender_ip(this->ip);
                    pss_message.set_sender_pos(this->position);
                    pss_message.set_type(proto::pss_message::Type::pss_message_Type_LOADBALANCE);
                    this->send_msg(target_peer, pss_message);
                }
            }catch(const char* msg){
                // empty view -> nothing to do
            }
        }
    }

    close(this->sender_socket);
}

void smart_load_balancer::stop() {
    this->running = false;
}

void smart_load_balancer::send_msg(peer_data &target_peer, proto::pss_message &msg) {
    try {

        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(client::lb_port/*target_peer.port*/);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        std::string buf;
        msg.SerializeToString(&buf);

        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));

//                printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        }
    }catch(...){
        spdlog::error("=============================== Não consegui enviar =================");
//            std::cout <<"=============================== Não consegui enviar =================" << std::endl;
    }
}


