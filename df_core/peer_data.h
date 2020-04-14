//
// Created by danielsf97 on 10/12/19.
//

#ifndef DATAFLASKSCPP_PEER_DATA_H
#define DATAFLASKSCPP_PEER_DATA_H

#include <string>

struct peer_data {
    std::string ip;
    //int port;
    int age;
    long id;
    //group construction
    int nr_slices;
    double pos;
    int slice;
};

#endif //DATAFLASKSCPP_PEER_DATA_H
