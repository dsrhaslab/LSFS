//
// Created by danielsf97 on 10/12/19.
//

#include "pss_listener.h"
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include "df_core/peer.h"
#include <iostream>
#include <pss_message.pb.h>
#include <ctime>
#include <string>
#include "df_communication/udp_async_server.h"


#define LOG(X) std::cout << X << std::endl;

class pss_listener_worker : public udp_handler{
private:
    pss* pss_ptr;

public:
    pss_listener_worker(pss* pss): pss_ptr(pss){}

    void handle_function(const char *data, size_t size) override {
        try {
            proto::pss_message pss_message;
            pss_message.ParseFromArray(data, size);

            this->pss_ptr->process_msg(pss_message);
        }
        catch(const char* e){
            std::cout << e << std::endl;
        }
        catch(...){}
    }
};

pss_listener::pss_listener(pss* pss)
{
    this->pss_ptr = pss;
}

void pss_listener::operator()() {
    try {
        pss_listener_worker worker(this->pss_ptr);
        udp_async_server server(this->io_service, this->pss_ptr->pss_port,(udp_handler*) &worker);

        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));

        this->thread_pool.join_all();
    }
    catch(const std::system_error& e) {
        std::cerr << "System Error: Not avaliable resources to create peer (pss_listener)!" << std::endl;
        exit(1);
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

void pss_listener::stop_thread() {
    LOG("Stopping Listener thread");
    //terminating ioService processing loop
    this->io_service.stop();
    //joining all threads of thread_loop
    this->thread_pool.join_all();
    LOG("JOINT ALL THREADS FROM POOL!!!")
}