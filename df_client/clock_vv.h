#ifndef P2PFS_CLOCK_H
#define P2PFS_CLOCK_H



#include <atomic>
#include <mutex>


class clock_vv {

private:

    long clock;

public:

     std::recursive_mutex c_mutex;

    clock_vv(){
        std::scoped_lock<std::recursive_mutex> lk(this->c_mutex);
        this->clock = 0;
    }

    void increment(){
        std::scoped_lock<std::recursive_mutex> lk(this->c_mutex);
        this->clock++;
    }

    long increment_and_get(){
        std::scoped_lock<std::recursive_mutex> lk(this->c_mutex);
        return ++this->clock;
    }

    long get(){
        std::scoped_lock<std::recursive_mutex> lk(this->c_mutex);
        return this->clock;
    }

    void assign(long num){
        std::scoped_lock<std::recursive_mutex> lk(this->c_mutex);
        this->clock = num;
    }

};

#endif //P2PFS_CLOCK_H
