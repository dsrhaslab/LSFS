
#include "clock_vv.h"

clock_vv::clock_vv(){
    this->clock = 0;

}

void clock_vv::increment(){
    this->clock++;
}


void clock_vv::print(){
    std::cout << this->clock << std::endl;
}