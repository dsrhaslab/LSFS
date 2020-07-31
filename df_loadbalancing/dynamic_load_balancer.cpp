//
// Created by danielsf97 on 1/27/20.
//

#include "df_tcp_client_server_connection/tcp_client_server_connection.h"
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <df_client/client.h>
#include "dynamic_load_balancer.h"

dynamic_load_balancer::dynamic_load_balancer(std::string boot_ip/*, int boot_port*/, std::string ip/*, int port*/, long sleep_interval):
    ip(ip), sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
{

    std::random_device rd;     // only used once to initialise (seed) engine
    this->random_eng = std::mt19937(rd());

    bool recovered = false;

    while(!recovered){
        try {
            tcp_client_server_connection::tcp_client_connection connection(boot_ip.c_str(), client::lb_port);

            //sending getview msg
            proto::pss_message msg_to_send;
            msg_to_send.set_type(proto::pss_message_Type::pss_message_Type_GETVIEW);
            msg_to_send.set_sender_ip(ip);
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
                peer_rcv.age = peer.age();
                peer_rcv.id = peer.id();
                peer_rcv.slice = peer.slice();
                peer_rcv.nr_slices = peer.nr_slices();
                peer_rcv.pos = peer.pos();
                view_rcv.emplace_back(std::move(peer_rcv));
            }
            this->view = std::move(view_rcv);

        }catch(const char* e){
            std::cout << e << std::endl;
        }catch(...){}
    }
}

peer_data dynamic_load_balancer::get_random_peer() {

    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    int view_size = this->view.size();
    if(view_size == 0) throw "Empty View Received";

    std::uniform_int_distribution<int> uni(0, view_size - 1); // guaranteed unbiased
    int random_int = uni(random_eng);
    return this->view.at(random_int);
}

peer_data dynamic_load_balancer::get_peer(const std::string& key = "") {
    return get_random_peer();
}

void dynamic_load_balancer::process_msg(proto::pss_message &pss_msg) {
    std::vector<peer_data> recv_view;
    for(auto& peer: pss_msg.view()){
        peer_data peer_data;
        peer_data.ip = peer.ip();
        peer_data.age = peer.age();
        peer_data.id = peer.id();
        peer_data.slice = peer.slice();
        peer_data.nr_slices = peer.nr_slices();
        peer_data.pos = peer.pos();
        recv_view.push_back(peer_data);
    }

    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    if(!recv_view.empty()) {
        this->view = std::move(recv_view);
    }
}

void dynamic_load_balancer::send_msg(peer_data& target_peer, proto::pss_message& msg){
    try {

        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(/*target_peer.port*/ client::lb_port);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        std::string buf;
        msg.SerializeToString(&buf);

        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){
            printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
        }
    }catch(...){
        std::cout <<"=============================== Não consegui enviar =================" << std::endl;
    }
}

void dynamic_load_balancer::operator()() {
    this->running = true;

    while(this->running){
        std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
        if(this->running){
            try{
                peer_data target_peer = this->get_peer(); //pair (port, age)

                proto::pss_message pss_message;
                pss_message.set_sender_ip(this->ip);
                pss_message.set_sender_pos(0); // not used
                pss_message.set_type(proto::pss_message::Type::pss_message_Type_LOADBALANCE);
                this->send_msg(target_peer, pss_message);
            }catch(const char* msg){
                // empty view -> nothing to do
            }
        }
    }

    close(this->sender_socket);
}

void dynamic_load_balancer::stop() {
    this->running = false;
}
