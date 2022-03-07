

#ifndef P2PFS_CLOCK_H
#define P2PFS_CLOCK_H



#include <atomic>
#include <iostream>


class clock_vv {
private:

    std::atomic<long> clock; //Clock for version vector 
    

public:
    clock_vv();
    void increment();
    void print();
};

#endif //P2PFS_CLOCK_H
