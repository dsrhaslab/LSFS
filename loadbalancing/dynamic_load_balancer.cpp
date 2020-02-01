//
// Created by danielsf97 on 1/27/20.
//

#include <serializer/capnp/capnp_serializer.h>
#include <tcp_client_server_connection/tcp_client_server_connection.h>
#include <thread>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "dynamic_load_balancer.h"

dynamic_load_balancer::dynamic_load_balancer(std::string boot_ip, int boot_port, std::string ip, int port, long sleep_interval):
    ip(ip), port(port), sleep_interval(sleep_interval), sender_socket(socket(PF_INET, SOCK_DGRAM, 0))
{
    srand (static_cast <unsigned> (time(nullptr))); //random seed

    bool recovered = false;
    std::shared_ptr<Capnp_Serializer> capnp_serializer(new Capnp_Serializer);

    while(!recovered){
        try {
            tcp_client_server_connection::tcp_client_connection connection(boot_ip.c_str(), boot_port, capnp_serializer);

            //sending announce msg
            pss_message pss_get_view_msg;
            pss_get_view_msg.sender_ip = ip;
            pss_get_view_msg.sender_port = port;
            pss_get_view_msg.type = pss_message::Type::GetView;
            connection.send_pss_msg(pss_get_view_msg);

            //receiving view from bootstrapper
            bool view_recv = false;
            pss_message pss_view_msg_rcv;
            while (!view_recv) {
                connection.recv_pss_msg(pss_view_msg_rcv);

                if (pss_view_msg_rcv.type == pss_message::Type::Normal)
                    view_recv = true;
            }

            recovered = true;

            std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
            this->view = std::move(pss_view_msg_rcv.view);
            //process received view
//            for (peer_data &peer : pss_view_msg_rcv.view) {
//                this->view.push_back(std::move(peer));
//            }

        }catch(const char* e){
            std::cout << e << std::endl;
        }catch(...){}
    }
}

peer_data dynamic_load_balancer::get_random_peer() {

    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    int random_int = rand()%(this->view.size());
    return this->view.at(random_int);
}

void dynamic_load_balancer::process_msg(proto::pss_message &pss_msg) {
    std::vector<peer_data> recv_view;
    for(auto& peer: pss_msg.view()){
        peer_data peer_data;
        peer_data.ip = peer.ip();
        peer_data.port = peer.port();
        peer_data.age = peer.age();
        peer_data.id = peer.id();
        peer_data.slice = peer.slice();
        peer_data.nr_slices = peer.nr_slices();
        peer_data.pos = peer.pos();
        recv_view.push_back(peer_data);
    }

    std::scoped_lock<std::recursive_mutex> lk(this->view_mutex);
    this->view = std::move(recv_view);
}

void dynamic_load_balancer::send_msg(peer_data& target_peer, proto::pss_message& msg){
    try {

        struct sockaddr_in serverAddr;
        memset(&serverAddr, '\0', sizeof(serverAddr));

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(target_peer.port);
        serverAddr.sin_addr.s_addr = inet_addr(target_peer.ip.c_str());

        std::string buf;
        msg.SerializeToString(&buf);

        int res = sendto(this->sender_socket, buf.data(), buf.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        if(res == -1){printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));}
    }catch(...){std::cout <<"=============================== NÂO consegui enviar =================" << std::endl;}
}

void dynamic_load_balancer::operator()() {
    this->running = true;

    while(this->running){
        std::this_thread::sleep_for (std::chrono::seconds(this->sleep_interval));
        if(this->running){

            peer_data target_peer = this->get_random_peer(); //pair (port, age)

            proto::pss_message pss_message;
            pss_message.set_sender_ip(this->ip);
            pss_message.set_sender_port(this->port);
            pss_message.set_type(proto::pss_message::Type::pss_message_Type_LOADBALANCE);
            this->send_msg(target_peer, pss_message);
        }
    }

    close(this->sender_socket);
}

void dynamic_load_balancer::stop() {
    this->running = false;
}
