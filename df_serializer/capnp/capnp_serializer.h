//
// Created by danielsf97 on 10/7/19.
//

#ifndef DATAFLASKSCPP_CAPNP_SERIALIZER_H
#define DATAFLASKSCPP_CAPNP_SERIALIZER_H

#include "capnp_serializer.h"
#include "df_serializer/serializer.h"
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <vector>
#include "df_serializer/capnp/packet.capnp.h"
#include "df_pss/pss_message.h"

class Capnp_Serializer: public Serializer {

public:
//    void send_view(std::unordered_map<int,int>& map, int i);
//    void recv_view(std::unordered_map<int, int>& map, int *socket);
//    int recv_identity(int socket);
//    void send_identity(int port, int socket);

    void recv_pss_message(int *socket, pss_message &pss_msg);
    void send_pss_message(int *socket, pss_message &pss_msg);
};

#endif //DATAFLASKSCPP_CAPNP_SERIALIZER_H
