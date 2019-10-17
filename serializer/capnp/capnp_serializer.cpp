//
// Created by danielsf97 on 10/8/19.
//

#include "capnp_serializer.h"
#include "../../pss/pss_message.h"
#include <iostream>
#include <unordered_map>

#define LOG(X) std::cout << X << std::endl;



void Capnp_Serializer::recv_pss_message(int* socket, pss_message& pss_msg){
    try {
        ::capnp::PackedFdMessageReader message(*socket);
        auto packet = message.getRoot<packet::Packet>();
        bool has_view = true;

        pss_msg.sender_ip = packet.getSenderIP();
        pss_msg.sender_port = packet.getSenderPort();

        switch (packet.getType()) {
            case packet::Packet::Type::NORMAL:
                pss_msg.type = pss_message::Type::Normal;
                break;
            case packet::Packet::Type::RESPONSE:
                pss_msg.type = pss_message::Type::Response;
                break;
            case packet::Packet::Type::ANNOUNCE:
                has_view = false;
                pss_msg.type = pss_message::Type::Announce;
                break;
        }

        if (has_view) {
            auto view = packet.getPayload().getView();
            for (auto peer: view) {
                pss_msg.view.push_back({std::string(peer.getHost().cStr()), peer.getPort(), peer.getAge()});
            }
        }
    }catch(...){throw;}
}

void Capnp_Serializer::send_pss_message(int* socket, pss_message& pss_msg){

    ::capnp::MallocMessageBuilder message;
    ::packet::Packet::Builder packet = message.initRoot<packet::Packet>();
    packet.setSenderIP(pss_msg.sender_ip);
    packet.setSenderPort(pss_msg.sender_port);

    bool has_view = true;

    switch(pss_msg.type){
        case pss_message::Type::Normal:
            packet.setType(::packet::Packet::Type::NORMAL);
            break;
        case pss_message::Type::Response:
            packet.setType(::packet::Packet::Type::RESPONSE);
            break;
        case pss_message::Type::Announce:
            packet.setType(::packet::Packet::Type::ANNOUNCE);
            packet.getPayload().setNothing();
            has_view = false;
            break;
    }

    if(has_view){
        ::capnp::List<::packet::Peer>::Builder payload = packet.getPayload().initView(pss_msg.view.size());

        int peer_idx = 0;
        for(peer_data peer: pss_msg.view){
            ::packet::Peer::Builder peer_builder = payload[peer_idx];
            peer_builder.setHost(peer.ip);
            peer_builder.setPort(peer.port);
            peer_builder.setAge(peer.age);
            peer_idx++;
        }
    }

    try{
        ::capnp::writePackedMessageToFd(*socket,message);

    }catch (kj::Exception e){
        std::cerr << "Unable to satisfy request";
    }

}


// OLD CODE

//void Capnp_Serializer::recv_view(std::unordered_map<int, int>& map, int *socket) {
//
//    bool recv_view = false;
//
//    while(!recv_view) {
//        ::capnp::PackedFdMessageReader message(*socket);
//        auto packet = message.getRoot<packet::Packet>();
//
//        if (packet.getType() == packet::Packet::Type::VIEW) {
//            recv_view = true;
//            auto view = packet.getPayload().getView();
//
//            for (auto peer: view) {
//                map.insert(std::make_pair(peer.getPort(), peer.getAge()));
//            }
//
//        }
//    }
//}
//
//int Capnp_Serializer::recv_identity(int socket) {
//    bool recv_identity = false;
//
//    int identity = -1;
//
//    while(!recv_identity) {
//        ::capnp::PackedFdMessageReader message(socket);
//        auto packet = message.getRoot<packet::Packet>();
//
//        if (packet.getType() == packet::Packet::Type::ANNOUNCE) {
//            recv_identity = true;
//            identity = packet.getSender();
//        }
//    }
//
//    return identity;
//}
//
//void Capnp_Serializer::send_view(std::unordered_map<int,int>& view, int socket) {
//    ::capnp::MallocMessageBuilder message;
//    ::packet::Packet::Builder packet = message.initRoot<packet::Packet>();
//    packet.setSender(12345);
//    packet.setType(packet::Packet::Type::VIEW);
//
//    ::capnp::List<::packet::Peer>::Builder payload = packet.getPayload().initView(view.size());
//
//    int peer_idx = 0;
//    for(auto const& [port, age] : view){
//        ::packet::Peer::Builder p1 = payload[peer_idx];
//        p1.setHost("127.0.0.1");
//        p1.setPort(port);
//        p1.setAge(age);
//        peer_idx++;
//    }
//
//    try{
//        ::capnp::writePackedMessageToFd(socket,message);
//
//    }catch (kj::Exception e){
//        std::cerr << "Unable to satisfy request";
//    }
//}
//
//void Capnp_Serializer::send_identity(int port, int socket){
//    ::capnp::MallocMessageBuilder message;
//    ::packet::Packet::Builder packet = message.initRoot<packet::Packet>();
//    packet.setType(packet::Packet::Type::ANNOUNCE);
//    packet.setSender(port);
//    packet.getPayload().setNothing();
//
//    try{
//    ::capnp::writePackedMessageToFd(socket,message);
//
//    }catch (kj::Exception e){
//        std::cerr << "Unable to satisfy request";
//    }
//}