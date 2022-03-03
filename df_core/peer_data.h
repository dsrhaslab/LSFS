//
// Created by danielsf97 on 10/12/19.
//

#ifndef DATAFLASKSCPP_PEER_DATA_H
#define DATAFLASKSCPP_PEER_DATA_H

#include <string>

struct peer_data {
    std::string ip;
    int kv_port; //Data Port
    int pss_port; //Pss Port
    int recover_port; //Recover Port - Anti Entropy
    int age;
    long id;
    //group construction
    int nr_slices;
    double pos;
    int slice;

    bool operator<(const struct peer_data& rhs) const
    {
        return id < rhs.id;  //assume that you compare the record based on a
    }

};

#endif //DATAFLASKSCPP_PEER_DATA_H
