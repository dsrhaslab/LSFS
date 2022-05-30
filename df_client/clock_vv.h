

#ifndef P2PFS_CLOCK_H
#define P2PFS_CLOCK_H



#include <atomic>
#include <iostream>
#include <unordered_map>


class clock_vv {

private:

    std::atomic<long> clock;     

public:
    clock_vv(){
        this->clock = 0;
    }

    void increment(){
        this->clock++;
    }

    long increment_and_get(){
        return ++this->clock;
    }

    long get(){
        return this->clock;
    }
};

#endif //P2PFS_CLOCK_H
