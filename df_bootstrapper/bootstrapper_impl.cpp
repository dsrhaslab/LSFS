//
// Created by danielsf97 on 10/7/19.
//

#include "bootstrapper_impl.h"
#include <iostream>
#include <memory>
#include "df_serializer/capnp/capnp_serializer.h"
#include <thread>

#include <algorithm>    // std::random_shuffle
#include <random>      // std::rand, std::srand
#include <map>
#include <unistd.h>
#include <algorithm>
#include "yaml-cpp/yaml.h"


#define LOG(X) std::cout << X << std::endl;

BootstrapperImpl::BootstrapperImpl(int viewsize, const char* ip/*, int port*/):
    connection(tcp_client_server_connection::tcp_server_connection(ip, boot_port/*port*/, std::unique_ptr<Serializer>(new Capnp_Serializer))),
    viewsize(viewsize),
    ip(ip)
    //port(port)

{
//    std::cerr << "[df_bootstrapper] function: constructor [Creating Server Connection]" << std::endl;
    this->initialnodes = 10;
    this->running = true;
    std::cout << ip << ":" << boot_port << std::endl;
}

void BootstrapperImpl::stopThread(){
    this->running = false;
    //terminating ioService processing loop
    this->io_service.stop();
    //joining all threads of thread_loop
    this->thread_pool.join_all();
}

std::string BootstrapperImpl::get_ip(){
    return this->ip;
}

//int BootstrapperImpl::get_port(){
//    return this->port;
//}

std::vector<peer_data> BootstrapperImpl::get_view() {

    std::unique_lock<std::recursive_mutex> lk(this->fila_mutex);

    if(this->fila.empty()){
        this->boot_fila();
    }

    std::vector</*int*/std::string> to_send = this->fila.front();
    this->fila.pop();

    lk.unlock();

    std::vector<peer_data> res;
    for(/*int& peer_port*/ std::string peer_ip: to_send){
        peer_data peer;
        peer.ip = peer_ip;
        //peer.port = peer_port;
        peer.age = 20;
        peer.id = aliveIds[/*peer_port*/ peer_ip];
        peer.pos = alivePos[/*peer_port*/ peer_ip];

        res.push_back(peer);
    }

    return res;
}

void BootstrapperImpl::add_peer(std::string ip/*, int port*/, long id, double pos){
    std::scoped_lock<std::shared_mutex> lk(this->alive_ips_mutex /*this->alive_ports_mutex*/);
    //this->alivePorts.insert(port);
    this->aliveIps.insert(ip);
    this->aliveIds.insert(std::make_pair(/*port*/ ip,id));
    this->alivePos.insert(std::make_pair(/*port*/ ip,pos));
};

void BootstrapperImpl::remove_peer(/*int port*/ std::string ip){
    std::scoped_lock<std::shared_mutex> lk(this->alive_ips_mutex /*this->alive_ports_mutex*/);
    //this->alivePorts.erase(port);
    this->aliveIps.erase(/*port*/ ip);
    this->aliveIds.erase(/*port*/ ip);
    this->alivePos.erase(/*port*/ ip);
};

void BootstrapperImpl::boot_fila() {

    std::unique_lock<std::recursive_mutex> lk_fila(this->fila_mutex);
    std::unique_lock<std::shared_mutex> lk(this->alive_ips_mutex /*this->alive_ports_mutex*/);

    std::vector</*int*/ std::string> res;
    if(/*this->alivePorts.size()*/ this->aliveIps.size() <= this->viewsize){
        for (std::string peer_ip: this->aliveIps /*int peer_port: this->alivePorts*/){
            res.push_back(peer_ip /*peer_port*/);
        }
        lk.unlock();
        this->fila.push(res);
    }
    else if(/*this->alivePorts.size()*/ this->aliveIps.size() < this->viewsize * 10){
        std::vector<std::string /*int*/> tmp;
        for (std::string peer_ip: this->aliveIps /*int peer_port: this->alivePorts*/){
            tmp.push_back(peer_ip /*peer_port*/);
        }
        lk.unlock();
        std::shuffle(std::begin(tmp), std::end(tmp), std::default_random_engine(0));
        int max_rand = tmp.size() - this->viewsize - 1;
        for(int i = 0; i < this->initialnodes; i++){
            int st_index = std::rand() % (max_rand + 1);
            res = std::vector</*int*/ std::string>(std::begin(tmp) + st_index, std::begin(tmp) + st_index + this->viewsize);
            this->fila.push(res);
        }
    }else{
        for(int i = 0; i < this->initialnodes; i++){
            std::vector</*int*/ std::string> tmp;
            while(tmp.size() < this->viewsize){
                int st_index = std::rand() % (this->aliveIps.size() /*this->alivePorts.size()*/);
                auto it = std::begin(this->aliveIps /*this->alivePorts*/);
                std::advance(it, st_index);
                if(std::find(tmp.begin(), tmp.end(), *it) == tmp.end()) {
                    tmp.push_back(*it);
                }
            }
            this->fila.push(tmp);
        }
        lk.unlock();
    }
    lk_fila.unlock();
}

void BootstrapperImpl::boot_worker(int* socket){
    pss_message recv_pss_msg;

    try {

        this->connection.recv_pss_msg(socket, recv_pss_msg);

        switch (recv_pss_msg.type){
            case pss_message::Type::Announce: {
                pss_message msg_to_send;
                msg_to_send.view = this->get_view();
                msg_to_send.sender_ip = this->get_ip();
                //msg_to_send.sender_port = this->get_port();
                msg_to_send.type = pss_message::Type::Normal;

                this->connection.send_pss_msg(socket, msg_to_send);

                std::cout << recv_pss_msg.view[0].pos << std::endl;
                this->add_peer(recv_pss_msg.sender_ip/*, recv_pss_msg.sender_port*/, recv_pss_msg.view[0].id, recv_pss_msg.view[0].pos);
                break;
            }
            case pss_message::Type::Termination: {
                this->remove_peer(recv_pss_msg.sender_ip /*recv_pss_msg.sender_port*/);
                break;
            }
            case pss_message::Type::GetView: {
                pss_message msg_to_send;
                msg_to_send.view = this->get_view();
                msg_to_send.sender_ip = this->get_ip();
                //msg_to_send.sender_port = this->get_port();
                msg_to_send.type = pss_message::Type::Normal;

                this->connection.send_pss_msg(socket, msg_to_send);
                break;
            }
        }

    }catch(...){}

//    std::cerr << "[Bootstrap] function: boot_worker [Closing] socket -> " + std::to_string(*socket) << std::endl;
    close(*socket);
}

void BootstrapperImpl::run(){

    int nr_threads = Bootstrapper::bootstrapper_thread_loop_size;
    boost::asio::io_service::work work(this->io_service);
    for(int i = 0; i < nr_threads; ++i){
        this->thread_pool.create_thread(
                boost::bind(&boost::asio::io_service::run, &(this->io_service))
        );
    }

    while(this->running){
        int socket = this->connection.accept_connection();
//        std::cerr << "[Bootstrap] function: run [Opening] socket -> " + std::to_string(socket) << std::endl;
        this->io_service.post(boost::bind(&BootstrapperImpl::boot_worker, this, &socket));
    }
}

tcp_client_server_connection::tcp_server_connection* BootstrapperImpl::get_connection() {
    return &(this->connection);
}

std::string get_local_ip_address(){
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    sockaddr_in loopback;

    if (sock == -1) {
        throw "ERROR CREATING SOCKET";
    }

    std::memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = INADDR_LOOPBACK;   // using loopback ip address
    loopback.sin_port = htons(9);                 // using debug port

    if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        throw "ERROR COULD NOT CONNECT";
    }

    socklen_t addrlen = sizeof(loopback);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
        close(sock);
        throw "ERROR COULD NOT GETSOCKNAME";
    }

    close(sock);

    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == 0x0) {
        throw "ERROR COULD NOT INET_NTOP";
    } else {
        return std::string(buf);
    }
}

int main(int argc, char *argv[]) {

//    std::cout << "Starting Bootstrapper" << std::endl;

    if(argc < 2){
        exit(1);
    }

    const char* conf_filename = argv[1];

    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    int view_size = main_confs["view_size"].as<int>();

    std::string ip;
    try{
        ip = get_local_ip_address();
    }catch(const char* e){
        std::cerr << "Error Obtaining IP Address: " << e << std::endl;
        exit(1);
    }

    std::unique_ptr<Bootstrapper> bootstrapper(new BootstrapperImpl(view_size, ip.c_str()/*, 12345*/));
    bootstrapper->run();
};
