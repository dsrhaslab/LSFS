

#ifndef P2PFS_CLOCK_H
#define P2PFS_CLOCK_H



#include <atomic>
#include <iostream>
#include <unordered_map>


class clock_vv {

public: 
    inline const static int CONCURRENT = 1;
    inline const static int GREATER = 2;
    inline const static int EQUAL = 3;


private:

    std::atomic<long> clock; //Clock for version vector 
    

public:
    clock_vv();
    void increment();
    void print();
    int equals_vv(std::unordered_map<long, long>& vv1, std::unordered_map<long, long>& vv2 );
};

#endif //P2PFS_CLOCK_H
