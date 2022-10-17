#ifndef DATAFLASKSCPP_PEER_DATA_H
#define DATAFLASKSCPP_PEER_DATA_H

#include <string>

struct peer_data {
    std::string ip;
    int kv_port; 
    int pss_port; 
    int recover_port; 
    int age;
    long id;
    //group construction
    int nr_slices;
    double pos;
    int slice;

    bool operator<(const struct peer_data& rhs) const
    {
        return id < rhs.id;  //assume that you compare the record based on a id
    }

};

#endif //DATAFLASKSCPP_PEER_DATA_H
