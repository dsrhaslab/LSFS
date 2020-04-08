//
// Created by danielsf97 on 12/16/19.
//

#include "group_construction.h"
#include <netinet/in.h>
#include <vector>
#include <math.h>
#include <pss_message.pb.h>
#include <arpa/inet.h>
#include <spdlog/spdlog.h>

#define LOG(X) std::cout << X << std::endl;

//ou tem de se esperar mais tempo ou o numero de grupos n está a ser calculado corretamente

group_construction::group_construction(std::string ip,int port, long id, double position, int replication_factor_min,
        int replication_factor_max, int max_age, bool local, int local_interval, std::shared_ptr<kv_store<std::string>> store, std::shared_ptr<spdlog::logger> logger){
    this->first_message = true;
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
    this->port = port;
    this->store = std::move(store);
    this->logger = std::move(logger);

    if ((this->sender_socket = socket(PF_INET, SOCK_DGRAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
}

std::vector<peer_data> group_construction::get_local_view(){
    std::vector<peer_data> res;

    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    for (auto [port, peer]: this->local_view) {
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

void group_construction::receive_local_message(std::vector<peer_data> received) {
    this->logger->info("[Group Construction] Received LOCAL Message");
    for(peer_data& peer: received){
        if(group(peer.pos) == this->my_group && !(peer.id == this->id)){
            auto current_it = this->local_view.find(peer.port);
            if(current_it != this->local_view.end()){ //o elemento existe no mapa
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second.age = peer.age;

            }else{
                this->local_view.insert(std::make_pair(peer.port, peer));
            }
        }
    }
}

void group_construction::print_view() {
    spdlog::debug("====== My View[" + std::to_string(this->port) + "] ====");
//    std::cout << "====== My View[" + std::to_string(this->port) + "] ====" << std::endl;
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto const& [key, peer] : this->local_view){
        spdlog::debug(peer.ip + "(" + std::to_string(peer.port) + ") : " + std::to_string(peer.age)  + " -> " + std::to_string(peer.pos) + ":" + std::to_string(group(peer.pos)));
//        std::cout << peer.ip << "(" << peer.port << ") : " << peer.age  << " -> " << peer.pos << ":" << group(peer.pos) << std::endl;
    }
    spdlog::debug("==========================");
//    std::cout << "==========================" << std::endl;
}


void group_construction::receive_message(std::vector<peer_data> received) {

    //When a peer boots it must keep up with peers
    int nr_groups_from_peers = 1;

    //AGING VIEW
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    for(auto& [port, peer]: this->local_view){
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
        if(group(peer.pos) == this->my_group && peer.id != this->id){
            auto current_it = this->local_view.find(peer.port);
            if(current_it != this->local_view.end()){ //o elemento existe no mapa
                int current_age = current_it->second.age;
                if(current_age > peer.age)
                    current_it->second = peer;

            }else{
                this->local_view.insert(std::make_pair(peer.port, peer));
            }
        }else{
            not_added.push_back(peer);
        }
    }

    //CLEAN VIEW
    std::vector<int> to_rem;
    for (auto& [port,peer] : this->local_view){
        if(group(peer.pos) != this->my_group){
            to_rem.push_back(port);
            not_added.push_back(peer);
        }
        else{
            if(peer.age > max_age){
                to_rem.push_back(port);
            }
        }
    }
    for(int port : to_rem){
        this->local_view.erase(port);
    }

    //SEARCH FOR VIOLATIONS
    int estimation = this->local_view.size(); //countEqual();

    // +1 porque temos de incluir o próprio
    if((estimation + 1) < this->replication_factor_min){
        if(this->nr_groups > 1){
            this->set_nr_groups(this->nr_groups/2);
        }
    }
    if((estimation + 1) > this->replication_factor_max){
        if(this->nr_groups*2 > 16) this->print_view();
        this->set_nr_groups(this->nr_groups*2);
    }
    if(this->first_message){
        this->set_nr_groups(nr_groups_from_peers);
        this->first_message = false;
    }
    this->set_my_group(group(this->position));
    this->store->update_partition(this->my_group, this->nr_groups);

    std::string local_view_str = "{";
    for(auto& [port, peer] : this->local_view){
        local_view_str += std::to_string(peer.port) + ":" + std::to_string(peer.age) + ", ";
    }
    local_view_str +=  "} -> {";

    //Check if none of the peers received or from the ones that did not belong
    //to the current group match now the group in question
    for(auto& peer : not_added){
        if(group(peer.pos) == this->my_group && peer.id != this->id) {
            this->local_view.insert(std::make_pair(peer.port, peer));
        }
    }


    for(auto& [port, peer] : this->local_view){
        local_view_str += std::to_string(peer.port) + ":" + std::to_string(peer.age) + ", ";
    }
    this->logger->info("[Group Construction] Received Message " + local_view_str + "}");

    //SEND LOCAL VIEW TO NEIGHBORS
    if(this->local && (this->cycle % this->local_interval == 0)){
        std::vector<peer_data> to_send;
        peer_data myself = {
                this->ip,
                this->port,
                0,
                this->id,
                this->nr_groups,
                this->position,
                this->my_group,
        };

        to_send.push_back(myself);
        for(auto [port, peer]: this->local_view){
            to_send.push_back(peer);
        }

        proto::pss_message pss_message;
        pss_message.set_sender_ip(this->ip);
        pss_message.set_sender_port(this->port);
        pss_message.set_type(proto::pss_message_Type::pss_message_Type_LOCAL);

        for(auto& peer: to_send){
            proto::peer_data* peer_data = pss_message.add_view();
            peer_data->set_ip(peer.ip);
            peer_data->set_port(peer.port);
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
                //SEND MESSAGE
                this->send_pss_msg(peer.ip , peer.port, buf);
            }
        }
    }
}

//void group_construction::receive_message(std::vector<peer_data> received) {
//    //When a peer boots it must keep up with peers
//    int nr_groups_from_peers = 1;
//
//    //AGING VIEW
//    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
//
//    for(auto& [port, peer]: this->local_view){
//        peer.age += 1;
//    }
//
//    //ADD RECEIVED
//    for (peer_data& peer: received){
//        if(this->first_message){
//            if(peer.nr_slices > this->nr_groups && peer.nr_slices > nr_groups_from_peers){
//                nr_groups_from_peers = peer.nr_slices;
//            }
//        }
//        if(group(peer.pos) == this->my_group && peer.id != this->id){
//            auto current_it = this->local_view.find(peer.port);
//            if(current_it != this->local_view.end()){ //o elemento existe no mapa
//                int current_age = current_it->second.age;
//                if(current_age > peer.age)
//                    current_it->second = peer;
//
//            }else{
//                this->local_view.insert(std::make_pair(peer.port, peer));
//            }
//        }
//    }
//
//    //CLEAN VIEW
//    std::vector<int> to_rem;
//    for (auto& [port,peer] : this->local_view){
//        if(group(peer.pos) != this->my_group){
//            to_rem.push_back(port);
//        }
//        else{
//            if(peer.age > max_age){
//                to_rem.push_back(port);
//            }
//        }
//    }
//    for(int port : to_rem){
//        this->local_view.erase(port);
//    }
//
//    //SEARCH FOR VIOLATIONS
//    int estimation = this->local_view.size(); //countEqual();
//
//    if((estimation + 1) < this->replication_factor_min){
//        if(this->nr_groups > 1){
//            this->set_nr_groups(this->nr_groups/2);
//        }
//    }
//    if((estimation + 1) > this->replication_factor_max){
//        if(this->nr_groups*2 > 16) this->print_view();
//        this->set_nr_groups(this->nr_groups*2);
//    }
//    if(this->first_message){
//        this->set_nr_groups(nr_groups_from_peers);
//        this->first_message = false;
//    }
//    this->set_my_group(group(this->position));
//    this->store->update_partition(this->my_group, this->nr_groups);
//
//    //SEND LOCAL VIEW TO NEIGHBORS
//    if(this->local && (this->cycle % this->local_interval == 0)){
//        std::vector<peer_data> to_send;
//        peer_data myself = {
//                this->ip,
//                this->port,
//                0,
//                this->id,
//                this->nr_groups,
//                this->position,
//                this->my_group,
//        };
//
//        to_send.push_back(myself);
//        for(auto [port, peer]: this->local_view){
//            to_send.push_back(peer);
//        }
//
//        proto::pss_message pss_message;
//        pss_message.set_sender_ip(this->ip);
//        pss_message.set_sender_port(this->port);
//        pss_message.set_type(proto::pss_message_Type::pss_message_Type_LOCAL);
//
//        for(auto& peer: to_send){
//            proto::peer_data* peer_data = pss_message.add_view();
//            peer_data->set_ip(peer.ip);
//            peer_data->set_port(peer.port);
//            peer_data->set_age(peer.age);
//            peer_data->set_id(peer.id);
//            peer_data->set_pos(peer.pos);
//            peer_data->set_nr_slices(peer.nr_slices);
//            peer_data->set_slice(peer.slice);
//        }
//
//        std::string buf;
//        pss_message.SerializeToString(&buf);
//
//        for(peer_data& peer : to_send){
//            if(peer.id != this->id){
//                //SEND MESSAGE
//                this->send_pss_msg(peer.port, buf);
//            }
//        }
//    }
//}

void group_construction::send_pss_msg(std::string& target_ip, int target_port, std::string &msg_string){
    try {
        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_ip.c_str());

        int res = sendto(this->sender_socket, msg_string.data(), msg_string.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){
            spdlog::error("Oh dear, something went wrong with read()! %s\n", strerror(errno));

//                printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        }
    }catch(...){
        spdlog::error("=============================== Não consegui enviar =================");
//            std::cout <<"=============================== Não consegui enviar =================" << std::endl;
    }
}

void group_construction::send_local_message(std::string& target_ip, int target_port) {
    std::scoped_lock<std::recursive_mutex> lk (this->view_mutex);
    std::vector<peer_data> to_send;
    peer_data myself = {
            this->ip,
            this->port,
            0,
            this->id,
            this->nr_groups,
            this->position,
            this->my_group,
    };

    to_send.push_back(myself);
    for(auto [port, peer]: this->local_view){
        to_send.push_back(peer);
    }

    proto::pss_message pss_message;
    pss_message.set_sender_ip(this->ip);
    pss_message.set_sender_port(this->port);
    pss_message.set_type(proto::pss_message_Type::pss_message_Type_LOCAL);

    for(auto& peer: to_send){
        proto::peer_data* peer_data = pss_message.add_view();
        peer_data->set_ip(peer.ip);
        peer_data->set_port(peer.port);
        peer_data->set_age(peer.age);
        peer_data->set_id(peer.id);
        peer_data->set_pos(peer.pos);
        peer_data->set_nr_slices(peer.nr_slices);
        peer_data->set_slice(peer.slice);
    }

    std::string buf;
    pss_message.SerializeToString(&buf);

    this->send_pss_msg(target_ip, target_port, buf);
}




