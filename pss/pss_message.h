//
// Created by danielsf97 on 10/12/19.
//

#ifndef DATAFLASKSCPP_PSS_MESSAGE_H
#define DATAFLASKSCPP_PSS_MESSAGE_H

#include <string>
#include <vector>
#include "../core/peer_data.h"

struct pss_message {
    enum Type {
        Normal,
        Response,
        Announce
    };

    Type type;
    std::string sender_ip;
    int sender_port;
    std::vector<peer_data> view;
};

#endif //DATAFLASKSCPP_PSS_MESSAGE_H
