//
// Created by danielsf97 on 3/8/20.
//

#include "client_reply_handler_mt.h"
#include "kv_message.pb.h"
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include "df_communication/udp_async_server.h"
#include "exceptions/custom_exceptions.h"
#include "client.h"
#include <regex>

/* =====================================================================================================================
* ======================================== Worker Class ================================================================
* =====================================================================================================================*/

class client_reply_handler_listener_worker : public udp_handler {
    //Esta classe é partilhada por todas as threads
private:
    client_reply_handler *reply_handler;

public:
    client_reply_handler_listener_worker(client_reply_handler* reply_handler): reply_handler(reply_handler){}

    void handle_function(const char *data, size_t size) override {
        try {
            proto::kv_message message;
            message.ParseFromArray(data, size);
            if (message.has_get_reply_msg()) {

                const proto::get_reply_message &msg = message.get_reply_msg();
                reply_handler->process_get_reply_msg(msg);
            } else if (message.has_put_reply_msg()) {

                const proto::put_reply_message &msg = message.put_reply_msg();
                reply_handler->process_put_reply_msg(msg);
            }else if(message.has_get_latest_version_reply_msg()){

                const proto::get_latest_version_reply_message& msg = message.get_latest_version_reply_msg();
                reply_handler->process_get_latest_version_reply_msg(msg);
            }
        }
        catch (const char *e) {
            std::cerr << e << std::endl;
        }
        catch (...) {}
    }
};

/*=====================================================================================================================*/

client_reply_handler_mt::client_reply_handler_mt(std::string ip/*, int port*/, long wait_timeout, int nr_workers):
        client_reply_handler(ip/*, port*/, wait_timeout), nr_worker_threads(nr_workers)
{}

void client_reply_handler_mt::operator()() {
    try {
        client_reply_handler_listener_worker worker(this);
        udp_async_server server(this->io_service, client::kv_port /*this->port*/, (udp_handler*) &worker);

        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));

        this->thread_pool.join_all();
    }
    catch (std::exception& e) {
        spdlog::error(e.what());
    }
}

void client_reply_handler_mt::stop() {
    //terminating ioService processing loop
    this->io_service.stop();
    //joining all threads of thread_loop
    this->thread_pool.join_all();
}