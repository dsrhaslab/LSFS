

#include "clock_vv_th.h"


clock_vv_th::clock_vv_th(long id, std::shared_ptr<clock_vv> clock, std::shared_ptr<std::condition_variable> cond_m): 
id(id), clock_v(clock), cond_m(cond_m) {}


clock_vv_th::~clock_vv_th()
{
    close(fd);
}

void clock_vv_th::operator()() {
    std::string file_name = FILE_NAME_PREFIX + std::to_string(id); 
    load(file_name);
    while(true){
        save();
        std::unique_lock<std::mutex> lk(mutex);
        cond_m->wait(lk);
        lk.unlock();
    }
}

void clock_vv_th::save(){
    std::scoped_lock<std::recursive_mutex> lk(clock_v->c_mutex);
    long to_write = clock_v->get();
    
    int ww = write(fd, &to_write, sizeof(long));
    if(ww == -1) std::cout << "Error" << std::endl;
    fsync(fd);
    lseek(fd, 0, SEEK_SET);
}


void clock_vv_th::load(const std::string& f_name){
    long num_r = 0;
    fd = open(f_name.c_str(), O_CREAT | O_RDWR, 0777);
    int r = read(fd, &num_r, sizeof(long));
    if(r == -1) std::cout << "Error" << std::endl;
    if(r >= 0){ 
        clock_v->assign(num_r);
    }
}


void clock_vv_th::stop() 
{
    close(fd);
}