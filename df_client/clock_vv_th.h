#ifndef P2PFS_CLOCK_VV_TH_H
#define P2PFS_CLOCK_VV_TH_H


#include <atomic>
#include <mutex>
#include <condition_variable>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h> 

#include "clock_vv.h"

class clock_vv_th {

private:

    inline static const std::string FILE_NAME_PREFIX = "clock_u";

public:

    std::shared_ptr<clock_vv> clock_v;
    std::shared_ptr<std::condition_variable> cond_m;
    std::mutex mutex;
    int fd;
    long id;
    std::string base_path;

    clock_vv_th(long id, std::shared_ptr<clock_vv> clock, std::shared_ptr<std::condition_variable> cond_m, std::string base_path);
    ~clock_vv_th();
    void operator ()();
    void save();
    void load(const std::string& f_name);
    void stop();
};



#endif //P2PFS_CLOCK_VV_TH_H
