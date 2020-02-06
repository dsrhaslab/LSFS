//
// Created by danielsf97 on 10/8/19.
//

#include "capnp_serializer.h"
#include "../../df_pss/pss_message.h"
#include <iostream>

#include <netinet/in.h>
#include "../../df_tcp_client_server_connection/tcp_client_server_connection.h"


#define LOG(X) std::cout << X << std::endl;



void Capnp_Serializer::recv_pss_message(int* socket, pss_message& pss_msg){
    try {
        // Setting timeout for receiving data
        fd_set set;
        FD_ZERO(&set);
        FD_SET(*socket, &set);

        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int rv = select(*socket + 1, &set, NULL, NULL, &timeout);
        if (rv == -1) // In case of Error on Socket
        {
            throw "Socket Error";
        }
        else if (rv == 0)  // In case of Timeout
        {
            throw "Timeout Error";
        }

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
                pss_msg.type = pss_message::Type::Announce;
                break;
            case packet::Packet::Type::TERMINATION:
                has_view = false;
                pss_msg.type = pss_message::Type::Termination;
                break;
            case packet::Packet::Type::GETVIEW:
                has_view = false;
                pss_msg.type = pss_message::Type::GetView;
                break;
        }

        if (has_view) {
            auto view = packet.getPayload().getView();
            for (auto peer: view) {
                pss_msg.view.push_back({std::string(peer.getHost().cStr()), peer.getPort(), peer.getAge(), peer.getId(), 0, peer.getPos(), 0});
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
            break;
        case pss_message::Type::Termination:
            packet.setType(::packet::Packet::Type::TERMINATION);
            packet.getPayload().setNothing();
            has_view = false;
            break;
        case pss_message::Type::GetView:
            packet.setType(::packet::Packet::Type::GETVIEW);
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
            peer_builder.setId(peer.id);
            peer_builder.setPos(peer.pos);
            peer_idx++;
        }
    }

    try{
        ::capnp::writePackedMessageToFd(*socket,message);

    }catch (kj::Exception e){
        std::cout << "Unable to satisfy request";
        throw;
    }

}
