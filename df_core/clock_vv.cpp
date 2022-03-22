
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

/*
int clock_vv::compare_vv(std::unordered_map<long, long>& vv1, std::unordered_map<long, long>& vv2 ){
      
        bool equals = true;
        bool concurrent = true;
        bool greater = true;
        int count = 0;

        for(auto& x: vv2){
            auto it = vv1.find(x.first);
            if(it == vv1.end()){
                equals = false;
                count++;
            }else{
                if(it->second > x.second)
                    equals = false;
                else if(it->second < x.second){
                    greater = false;
                }

            }
        }
        if(vv1.size() == vv2.size() + count && !equals){
            return GREATER;
        }

}
*/