//
// Created by danielsf97 on 4/5/20.
//

#include <spdlog/spdlog.h>
#include "smart_load_balancer.h"
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
#include <df_util/randomizer.h>

smart_load_balancer::smart_load_balancer(std::string boot_ip, std::string ip, int pss_port, long sleep_interval, std::string& config_filename):
        ip(ip), pss_port(pss_port), sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
{
    std::random_device rd; // only used once to initialise (seed) engine
    this->random_eng = std::mt19937(rd());

    bool recovered = false;

    YAML::Node config = YAML::LoadFile(config_filename);
    auto main_confs = config["main_confs"];
    this->replication_factor_max = main_confs["rep_max"].as<int>();
    this->replication_factor_min = main_confs["rep_min"].as<int>();
    this->max_age = main_confs["max_age"].as<int>();
    this->max_smart_view_age = main_confs["max_smart_view_age"].as<int>(); 
    this->recovering_local_view = true;
    this->local = main_confs["local_message"].as<bool>();
    this->local_interval = main_confs["local_interval_sec"].as<int>();
    this->nr_saved_peers_by_group = main_confs["smart_load_balancer_group_knowledge"].as<int>();

    this->position = random_double(0.0, 1.0);
    this->nr_groups = 1;
    this->my_group = 1;
    this->cycle = 1;

    srand( time(NULL) ); //seeding for the first time only!

    while(!recovered){
        try {
            //tcp_client_server_connection::tcp_client_connection connection(boot_ip.c_str(), client::lb_port);
            tcp_client_server_connection::tcp_client_connection connection(boot_ip.c_str(), client::boot_port);

            //sending getview msg
            proto::pss_message msg_to_send;
            msg_to_send.set_type(proto::pss_message_Type::pss_message_Type_GETVIEW);
            msg_to_send.set_sender_ip(ip);
            msg_to_send.set_sender_pss_port(pss_port);
            msg_to_send.set_sender_pos(0); // not used

            std::string buf;
            msg_to_send.SerializeToString(&buf);

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

            std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
            std::vector<peer_data> view_rcv;
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
                view_rcv.emplace_back(std::move(peer_rcv));
            }
            auto temp_group = std::make_unique<std::vector<peer_data>>(std::move(view_rcv));
            this->view.push_back(std::move(temp_group));
        }catch(const char* e){
            std::cout << e << std::endl;
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

int smart_load_balancer::group(double peer_pos, int nr_groups){
    int temp = (int) ceil((static_cast<double>(nr_groups))*peer_pos);
    if(temp == 0){
        temp = 1;
    }
    return temp;
}

void smart_load_balancer::print_view(){
    std::cout << "================= View ===========" << std::endl;
    int i = 1;
    for(auto& view_group : view){
        std::cout << "Group " << i << ": ";
        for(auto& peer: *view_group){
            std::cout << peer.id << "[ " << peer.pos << " " << peer.age << " ] ";
        }
        std::cout << std::endl;
        i++;
    }
    std::cout << "==================================" << std::endl;
}


void insert_peer_data_with_order(std::unique_ptr<std::vector<peer_data>>& view, peer_data peer){
    bool inserted = false;

    for(auto it = view->begin(); it != view->end(); ++it){
        if(it->ip == peer.ip){
            if(it->age > peer.age){
                view->erase(it);
                insert_peer_data_with_order(view, peer);
            }
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

void smart_load_balancer::incorporate_local_peers(const std::vector<peer_data>& received) {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(const peer_data& peer: received){
        if(group(peer.pos) == this->my_group){
            auto current_it = this->local_view.find(peer.ip);
            if(current_it != this->local_view.end()){ //element exists in map
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second.age = peer.age;
            }else{
                this->local_view.insert(std::make_pair(peer.ip, peer));
            }
        }
    }
}

void smart_load_balancer::clean_local_view(){
    std::scoped_lock<std::recursive_mutex> lk (this->local_view_mutex);

    for(auto it = this->local_view.begin(); it != this->local_view.end();){
        const peer_data& peer = it->second;
        if(group(peer.pos) != this->my_group){
            this->local_view.erase(it++);
        }
        else{
            if(peer.age > max_age){
                this->local_view.erase(it++);
            }else{
                ++it;
            }
        }
    }
}

void smart_load_balancer::incorporate_peers_in_view(const std::vector<peer_data>& received){
    for(auto& peer: received){
        insert_peer_data_with_order(this->view[group(peer.pos)-1], peer);
    }

    //CLEAN VIEW
    for(auto& per_group_view : this->view) {
        int nr_elem_to_keep = 0;
        for (auto it = per_group_view->begin(); it != per_group_view->end(); ++it) {
            if (it->age < max_smart_view_age) {
                nr_elem_to_keep++;
            } else {
                break;
            }
        }
        per_group_view->resize(std::min(this->nr_saved_peers_by_group, nr_elem_to_keep));
    }
}

void smart_load_balancer::reconfigure_view_if_needed(){
    int current_groups = this->nr_groups;
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    int view_nr_groups = this->view.size();

    if(view_nr_groups == current_groups){
        return;
    }

    if(view_nr_groups > current_groups){
        while(view_nr_groups != current_groups){
            this->merge_groups_from_view();
            view_nr_groups = view_nr_groups/2;
        }
    }else{
        while(view_nr_groups != current_groups){
            this->split_groups_from_view();
            view_nr_groups = view_nr_groups*2;
        }
    }
}

void smart_load_balancer::recover_local_view(const std::vector<peer_data>& received){

    std::scoped_lock<std::recursive_mutex> lk (this->local_view_mutex);

    int nr_groups_from_peers = 1;

    for (const peer_data& peer: received) {
        if (peer.nr_slices > nr_groups_from_peers) {
            nr_groups_from_peers = peer.nr_slices;
        }
    }

    // Incorporate from received view nodes that belong to my group
    this->incorporate_local_peers(received);

    int first_estimation = this->local_view.size(); //countEqual();
    // Se a minha perceção do número de grupos é maior que a recebida e tenho nodos
    // suficientes para a sustentar

    // If my perception of the number of groups is greater than the received one and
    // i have enough nodes to support it
    if(this->nr_groups > nr_groups_from_peers && first_estimation >= this->replication_factor_min){
        if(first_estimation > this->replication_factor_max){
            this->nr_groups = this->nr_groups*2;
            this->my_group = group(this->position);
            this->reconfigure_view_if_needed();
            this->incorporate_peers_in_view(received);
            this->clean_local_view();
        }
        this->recovering_local_view = false;
        return;
    }

    this->nr_groups = nr_groups_from_peers;
    this->my_group = group(this->position);
    this->reconfigure_view_if_needed();
    this->incorporate_peers_in_view(received);

    // Clean local view
    this->clean_local_view();

    // At system start all peers have the same estimation of 1
    // There is nothing to recover
    if(nr_groups_from_peers == 1){
        this->recovering_local_view = false;
    }

    // Check if Local view was successfully recovered
    int estimation = this->local_view.size(); //countEqual();
    if(estimation >= this->replication_factor_min && estimation <= this->replication_factor_max){
        this->recovering_local_view = false;
    }else{
        this->request_local_message(received);
    }
}

void smart_load_balancer::merge_groups_from_view() {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
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
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    int view_next_size = this->view.size() * 2;
    std::vector<std::unique_ptr<std::vector<peer_data>>> next_view;
    int current_group = 1;
    for(auto& it : this->view){
        auto next_group = std::make_unique<std::vector<peer_data>>();
        for(auto peer_it = it->begin(); peer_it != it->end();){
            if(group(peer_it->pos, view_next_size) == current_group + 1){
                insert_peer_data_with_order(next_group, *peer_it);
                peer_it = it->erase(peer_it);
            }else{
                ++peer_it;
            }
        }
        next_view.push_back(std::move(it));
        next_view.push_back(std::move(next_group));
        current_group += 2;
    }
    this->view = std::move(next_view);
}

void smart_load_balancer::receive_message(std::vector<peer_data> received) {

    // If its recovering local view, discard group construction messages
    if(this->recovering_local_view) {
        recover_local_view(received);
        return;
    }

    std::scoped_lock<std::recursive_mutex> lk_local (this->local_view_mutex);

    // aging local view
    for(auto& [ip, peer]: this->local_view){
        peer.age += 1;
    }

    std::vector<peer_data> not_added;

    // add received nodes
    for (peer_data& peer: received){
        if(group(peer.pos) == this->my_group){
            auto current_it = this->local_view.find(peer.ip);
            if(current_it != this->local_view.end()){ //o elemento existe no mapa
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second = peer;

            }else{
                this->local_view.insert(std::make_pair(peer.ip, peer));
            }
        }else{
            not_added.push_back(peer);
        }
    }

    // clean local view
    std::vector<std::string> to_rem;
    for (auto& [ip,peer] : this->local_view){
        if(group(peer.pos) != this->my_group){
            to_rem.push_back(ip);
            not_added.push_back(peer);
        }
        else{
            if(peer.age > max_age){
                to_rem.push_back(ip);
            }
        }
    }
    for(std::string ip : to_rem){
        this->local_view.erase(ip);
    }

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

    //AGING VIEW
    for(auto& group_view : this->view){
        for(auto& peer: *group_view){
            peer.age += 1;
        }
    }

    //SEARCH FOR VIOLATIONS
    int estimation = this->local_view.size();

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


    this->my_group = group(this->position);

    //Check if none of the peers received or from the ones that did not belong
    //to the current group match now the group in question
    for(auto& peer : not_added){
        if(group(peer.pos) == this->my_group) {
            auto current_it = this->local_view.find(peer.ip);
            if(current_it != this->local_view.end()){ // element exists in map
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second = peer;
            }else{
                this->local_view.insert(std::make_pair(peer.ip, peer));
            }
        }
    }

    this->incorporate_peers_in_view(received);

}

peer_data smart_load_balancer::get_random_local_peer() {
    std::scoped_lock<std::recursive_mutex> lk (this->local_view_mutex);
    int local_view_size = this->local_view.size();
    if(local_view_size == 0) throw "EMPTY VIEW EXCEPTION";
    int peer_idx = random_int(0, local_view_size - 1);
    auto it = this->local_view.begin();
    for(int i = 0; i < peer_idx; i++){
        ++it;
    }
    return it->second;
}

peer_data smart_load_balancer::get_random_peer() {
    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);

    bool done = false;
    while(!done) {
        int group_to_send = random_int(0, nr_groups - 1);
        auto slice_group = this->view[group_to_send].get();
        int slice_size = slice_group->size();
        if(slice_size == 0){
            continue;
        }else if(slice_size == 1){
            return slice_group->front();
        }else{
            int slice_idx = random_int(0, slice_size - 1);
            return slice_group->at(slice_idx);
        }
    }
}

peer_data smart_load_balancer::get_peer(const std::string& key) {
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
        if(current > 0 && next_current < 0) break; //in case of overflow
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
        return slice_group->front();
    }else{
        int slice_idx = random_int(0, slice_size - 1);
        return slice_group->at(slice_idx);
    }
}

std::vector<peer_data> smart_load_balancer::get_n_random_peers(int nr_peers) {
    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    std::set<peer_data> temp;
    std::vector<peer_data> res;

    bool done = false;
    int max_equal_insertions_before_giveup = 3;
    int nr_insertions = 0;
    while(!done) {
        int group_to_send = random_int(0, nr_groups - 1);
        auto slice_group = this->view[group_to_send].get();
        int slice_size = slice_group->size();
        if(slice_size == 0){
            continue;
        }else if(slice_size == 1){
            temp.insert(slice_group->front());
            if(temp.size() == nr_insertions){
                max_equal_insertions_before_giveup--;
                if(max_equal_insertions_before_giveup == 0) {
                    done = true;
                }
            }else{
                nr_insertions += 1;
            }
        }else{
            int slice_idx = random_int(0, slice_size - 1);
            temp.insert(slice_group->front());
            if(temp.size() == nr_insertions){
                max_equal_insertions_before_giveup--;
                if(max_equal_insertions_before_giveup == 0) {
                    done = true;
                }
            }else{
                nr_insertions += 1;
            }
        }
    }

    res.reserve(temp.size());
    for (auto it = temp.begin(); it != temp.end(); ) {
        res.push_back(std::move(temp.extract(it++).value()));
    }

    return std::move(res);
}

std::vector<peer_data> smart_load_balancer::get_n_peers(const std::string& key, int nr_peers){
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
        return this->get_n_random_peers(nr_peers);
    }

    std::vector<peer_data> res;
    int nr_elements_to_sample = std::min(slice_size, nr_peers);
    std::sample(
            slice_group->begin(),
            slice_group->end(),
            std::back_inserter(res),
            nr_elements_to_sample,
            random_eng
    );

    return std::move(res);
}

void smart_load_balancer::receive_local_message(std::vector<peer_data> received) {

    if(this->recovering_local_view) {
        recover_local_view(received);
        return;
    }

    this->incorporate_local_peers(received);
}

void smart_load_balancer::request_local_message(const std::vector<peer_data>& received){

    proto::pss_message pss_message;
    pss_message.set_sender_ip(this->ip);
    pss_message.set_sender_pss_port(this->pss_port);
    pss_message.set_sender_pos(this->position);
    pss_message.set_type(proto::pss_message_Type::pss_message_Type_REQUEST_LOCAL);

    for(const peer_data& peer : received){
        send_msg(peer, pss_message);
    }
}

bool smart_load_balancer::has_recovered(){
    return !this->recovering_local_view;
}

void smart_load_balancer::process_msg(proto::pss_message &pss_msg) {
    std::vector<peer_data> recv_view;
    for(auto& peer: pss_msg.view()){
        peer_data peer_data;
        peer_data.ip = peer.ip();
        peer_data.kv_port = peer.kv_port();
        peer_data.pss_port = peer.pss_port();
        peer_data.recover_port = peer.recover_port();
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
                    peer_data target_peer = this->get_random_local_peer();

                    proto::pss_message pss_message;
                    pss_message.set_sender_ip(this->ip);
                    pss_message.set_sender_pss_port(this->pss_port);
                    pss_message.set_sender_pos(this->position);
                    pss_message.set_type(proto::pss_message::Type::pss_message_Type_LOADBALANCE_LOCAL);
                    this->send_msg(target_peer, pss_message);
                }else{
                    //send loadbalance message
                    peer_data target_peer = this->get_random_peer();

                    proto::pss_message pss_message;
                    pss_message.set_sender_ip(this->ip);
                    pss_message.set_sender_pss_port(this->pss_port);
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

void smart_load_balancer::send_msg(const peer_data &target_peer, proto::pss_message &msg) {
    try {

        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_peer.pss_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        std::string buf;
        msg.SerializeToString(&buf);

        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        }
    }catch(...){
        spdlog::error("====================== Não consegui enviar =================");
    }
}


