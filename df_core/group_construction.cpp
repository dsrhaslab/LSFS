//
// Created by danielsf97 on 12/16/19.
//

#include "group_construction.h"
#include "peer.h"
#include <netinet/in.h>
#include <vector>
#include <math.h>
#include <pss_message.pb.h>
#include <arpa/inet.h>
#include <spdlog/spdlog.h>

#define LOG(X) std::cout << X << std::endl;

group_construction::group_construction(std::string ip, int kv_port, int pss_port, int recover_port, long id, double position, int replication_factor_min,
        int replication_factor_max, int max_age, bool local, int local_interval, std::shared_ptr<kv_store<std::string>> store, std::shared_ptr<spdlog::logger> logger){
    this->recovering_local_view = true;
    this->id = id;
    this->position = position;
    this->nr_groups = 1;
    this->my_group = 1;
    this->replication_factor_min = replication_factor_min;
    this->replication_factor_max = replication_factor_max;
    this->max_age = max_age;
    this->local = local;
    this->local_interval = local_interval;
    this->cycle = 1;
    this->ip = ip;
    this->kv_port = kv_port;
    this->pss_port = pss_port;
    this->recover_port = recover_port;
    this->store = std::move(store);
    this->logger = std::move(logger);

    srand( time(NULL) ); //seeding for the first time only!

    if ((this->sender_socket = socket(PF_INET, SOCK_DGRAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
}

std::vector<peer_data> group_construction::get_local_view(){
    std::vector<peer_data> res;

    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    for (auto [id, peer]: this->local_view) {
        res.push_back(peer);
    }

    return std::move(res);
}

int group_construction::get_cycle() {
    return cycle;
}

void group_construction::set_cycle(int cycle) {
    this->cycle = cycle;
}

double group_construction::get_position(){
    return this->position;
}


int group_construction::get_nr_groups() {
    return nr_groups;
}


void group_construction::set_nr_groups(int ngroups) {
    this->nr_groups = ngroups;

}

int group_construction::get_my_group() {
    return my_group;
}


void group_construction::set_my_group(int group) {
    this->my_group = group;
}

int group_construction::group(double peer_pos){
    int temp = (int) ceil((static_cast<double>(this->get_nr_groups()))*peer_pos);
    if(temp == 0){
        temp = 1;
    }
    return temp;
}

void group_construction::incorporate_local_peers(const std::vector<peer_data>& received) {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(const peer_data& peer: received){
        if(group(peer.pos) == this->my_group && peer.id != this->id){
            auto current_it = this->local_view.find(peer.id);
            if(current_it != this->local_view.end()){ //the element exists in the map
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second.age = peer.age;
            }else{
                this->local_view.insert(std::make_pair(peer.id, peer));
            }
        }
    }
}

void group_construction::receive_local_message(const std::vector<peer_data>& received) {

    if(this->recovering_local_view) {
        recover_local_view(received);
        return;
    }

    this->incorporate_local_peers(received);
}

void group_construction::request_local_message(const std::vector<peer_data>& received){

    proto::pss_message pss_message;
    pss_message.set_sender_ip(this->ip);
    pss_message.set_sender_pss_port(this->pss_port);
    pss_message.set_sender_pos(this->position);
    pss_message.set_type(proto::pss_message_Type::pss_message_Type_REQUEST_LOCAL);

    std::string buf;
    pss_message.SerializeToString(&buf);

    for(const peer_data& peer : received){
        if(peer.id != this->id){
            //send message
            this->send_pss_msg(peer.ip, peer.pss_port, buf);
        }
    }
}

bool group_construction::has_recovered(){
    return !this->recovering_local_view;
}

void group_construction::clean_local_view(){
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

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

peer_data group_construction::get_random_peer_from_local_view(){
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

    int idx = rand() % this->local_view.size();
    for(auto& peer: this->local_view){
        if(idx == 0){
            return peer.second;
        }
        idx--;
    }
}

void group_construction::recover_local_view(const std::vector<peer_data>& received){

    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);

    int nr_groups_from_peers = 1;

    for (const peer_data& peer: received) {
        if (peer.nr_slices > nr_groups_from_peers) {
            nr_groups_from_peers = peer.nr_slices;
        }
    }

    // Incorporate from received view nodes that belong to my group
    this->incorporate_local_peers(received);

    int first_estimation = this->local_view.size(); //countEqual();
    // If my perception on the number of groups is greater than the received one and i know
    // sufficient nodes to sustain it
    if(this->nr_groups > nr_groups_from_peers && (first_estimation + 1) >= this->replication_factor_min){
        if((first_estimation + 1) > this->replication_factor_max){
            this->set_nr_groups(this->nr_groups*2);
            this->set_my_group(group(this->position));
            this->store->update_partition(this->my_group, this->nr_groups);
            this->clean_local_view();
        }
        this->recovering_local_view = false;
        return;
    }

    this->set_nr_groups(nr_groups_from_peers);
    this->set_my_group(group(this->position));
    this->store->update_partition(this->my_group, this->nr_groups);

    // Clean local view
    this->clean_local_view();

    // At system start all peers have the same estimation of 1
    // There is nothing to recover
    if(nr_groups_from_peers == 1){
        this->recovering_local_view = false;
        return;
    }

    // Check if Local view was sucessfully recovered
    int estimation = this->local_view.size(); //countEqual();
    // +1 because we have to include the node itself
    if((estimation + 1) >= this->replication_factor_min && (estimation + 1) <= this->replication_factor_max){
        this->recovering_local_view = false;
    }else{
        this->request_local_message(received);
    }
}

void group_construction::print_view() {
    spdlog::debug("====== My View[" + this->ip + std::to_string(this->pss_port) + "] ====");
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto const& [id, peer] : this->local_view){
        spdlog::debug(peer.ip + "(kv-" + std::to_string(peer.kv_port) + ", pss-" + std::to_string(peer.pss_port) + ")" + " : " + std::to_string(peer.age)  + " -> " + std::to_string(peer.pos) + ":" + std::to_string(group(peer.pos)));
    }
    spdlog::debug("==========================");
}


void group_construction::receive_message(const std::vector<peer_data>& received) {

    // If its recovering local view, discard group construction messages
    if(this->recovering_local_view) {
        recover_local_view(received);
        return;
    }

    //aging view
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto& [id, peer]: this->local_view){
        peer.age += 1;
    }

    std::vector<peer_data> not_added;

    //add received
    for (const peer_data& peer: received){
        if(group(peer.pos) == this->my_group && peer.id != this->id){
            auto current_it = this->local_view.find(peer.id);
            if(current_it != this->local_view.end()){ // the element exists in the map
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second = peer;

            }else{
                this->local_view.insert(std::make_pair(peer.id, peer));
            }
        }else{
            not_added.push_back(peer);
        }
    }

    // clean local view
    std::vector<long> to_rem;
    for (auto& [id, peer] : this->local_view){
        if(group(peer.pos) != this->my_group){
            to_rem.push_back(id);
            not_added.push_back(peer);
        }
        else{
            if(peer.age > max_age){
                to_rem.push_back(id);
            }
        }
    }
    for(long id : to_rem){
        this->local_view.erase(id);
    }

    //SEARCH FOR VIOLATIONS
    int estimation = this->local_view.size(); //countEqual();

    // +1 because we have to include the node itself
    if((estimation + 1) < this->replication_factor_min){
        if(this->nr_groups > 1){
            this->set_nr_groups(this->nr_groups/2);
        }
    }
    if((estimation + 1) > this->replication_factor_max){
        this->set_nr_groups(this->nr_groups*2);
    }

    this->set_my_group(group(this->position));
    this->store->update_partition(this->my_group, this->nr_groups);

    std::string local_view_str = "{";
    for(auto& [id, peer] : this->local_view){
        local_view_str += peer.ip+ ":" + std::to_string(peer.age) + ", ";
    }
    local_view_str +=  "} -> {";

    //Check if none of the peers received or from the ones that did not belong
    //to the current group match now the group in question
    for(auto& peer : not_added){
        if(group(peer.pos) == this->my_group && peer.id != this->id) {
            this->local_view.insert(std::make_pair(peer.id, peer));
        }
    }


    for(auto& [id, peer] : this->local_view){
        local_view_str +=  peer.ip + ":" + std::to_string(peer.age) + ", ";
    }
    this->logger->info("[Group Construction] Received Message " + local_view_str + "}");

    //SEND LOCAL VIEW TO NEIGHBORS
    if(this->local && (this->cycle % this->local_interval == 0)){
        std::vector<peer_data> to_send;
        peer_data myself = {
                this->ip,
                this->kv_port,
                this->pss_port,
                this->recover_port,
                0,
                this->id,
                this->nr_groups,
                this->position,
                this->my_group,
        };

        to_send.push_back(myself);
        for(auto [id, peer]: this->local_view){
            to_send.push_back(peer);
        }

        proto::pss_message pss_message;
        pss_message.set_sender_ip(this->ip);
        pss_message.set_sender_pss_port(this->pss_port);
        pss_message.set_sender_pos(this->position);
        pss_message.set_type(proto::pss_message_Type::pss_message_Type_LOCAL);

        for(auto& peer: to_send){
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

        for(peer_data& peer : to_send){
            if(peer.id != this->id){
                //send message
                this->send_pss_msg(peer.ip, peer.pss_port, buf);
            }
        }
    }
}

void group_construction::send_pss_msg(const std::string& target_ip, const int target_pss_port, const std::string &msg_string){
    try {
        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_pss_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_ip.c_str());

        int res = sendto(this->sender_socket, msg_string.data(), msg_string.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        }
    }catch(...){
        spdlog::error("===================== Unable to send =================");
    }
}

void group_construction::send_local_message(std::string& target_ip, int target_pss_port) {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    std::vector<peer_data> to_send;
    peer_data myself = {
            this->ip,
            this->kv_port,
            this->pss_port,
            this->recover_port,
            0,
            this->id,
            this->nr_groups,
            this->position,
            this->my_group,
    };

    to_send.push_back(myself);
    for(auto [id, peer]: this->local_view){
        to_send.push_back(peer);
    }

    proto::pss_message pss_message;
    pss_message.set_sender_ip(this->ip);
    pss_message.set_sender_pss_port(this->pss_port);
    pss_message.set_sender_pos(this->position);
    pss_message.set_type(proto::pss_message_Type::pss_message_Type_LOCAL);

    for(auto& peer: to_send){
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

    this->send_pss_msg(target_ip, target_pss_port, buf);
}




