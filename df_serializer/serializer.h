//
// Created by danielsf97 on 10/7/19.
//

#ifndef DATAFLASKSCPP_SERIALIZER_H
#define DATAFLASKSCPP_SERIALIZER_H

#include <unordered_map>
#include <iostream>
#include <vector>
#include "df_pss/pss_message.h"

class Serializer {

public:
//    virtual void send_view(std::unordered_map<int, int>& map, int i) = 0;
//    virtual void recv_view(std::unordered_map<int, int>& map, int *socket) = 0;
//    virtual int recv_identity(int socket) = 0;
//    virtual void send_identity(int port, int socket) = 0;
    virtual void recv_pss_message(int *socket, pss_message &pss_msg) = 0;
    virtual void send_pss_message(int *socket, pss_message &pss_msg) = 0;
};

#endif //DATAFLASKSCPP_SERIALIZER_H
