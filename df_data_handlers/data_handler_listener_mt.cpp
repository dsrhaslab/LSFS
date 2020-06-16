//
// Created by danielsf97 on 5/24/20.
//

#include <df_communication/udp_async_server.h>
#include "data_handler_listener_mt.h"

class data_handler_listener_worker : public udp_handler{
    //Esta classe é partilhada por todas as threads
private:
    data_handler_listener* data_handler;

public:
    data_handler_listener_worker(data_handler_listener* data_handler): data_handler(data_handler){}

    void handle_function(const char *data, size_t size) override {
        try {
            proto::kv_message msg;
            msg.ParseFromArray(data, size);

            if(msg.has_get_msg()){
                data_handler->process_get_message(msg);
            }else if(msg.has_get_reply_msg()){
                data_handler->process_get_reply_message(msg);
            }else if(msg.has_put_msg()){
                data_handler->process_put_message(msg);
            }else if(msg.has_put_with_merge_msg()){
                data_handler->process_put_with_merge_message(msg);
            }else if(msg.has_put_reply_msg()){
                //Este caso não vai acontecer porque os peers não deveriam receber mensagens de reply a um put
            }else if(msg.has_anti_entropy_msg()){
                data_handler->process_anti_entropy_message(msg);
            }else if(msg.has_get_latest_version_msg()){
                data_handler->process_get_latest_version_msg(msg);
            }else if(msg.has_get_latest_version_reply_msg()){
                //Este caso não vai acontecer porque os peers não deveriam receber mensagens de reply a um get version
            }else if(msg.has_recover_request_msg()){
                data_handler->process_recover_request_msg(msg);
            }

        }
        catch(const char* e){
            spdlog::error(e);
//            std::cout << e << std::endl;
        }
        catch(...){}
    }
};

data_handler_listener_mt::data_handler_listener_mt(std::string ip, long id, float chance, pss *pss, group_construction* group_c, std::shared_ptr<kv_store<std::string>> store, bool smart)
        : data_handler_listener(std::move(ip), id, chance, pss, group_c, std::move(store), smart){}

void data_handler_listener_mt::operator()() {
    try {
        data_handler_listener_worker worker(this);
        udp_async_server server(this->io_service, peer::kv_port/*this->port*/, (udp_handler*) &worker);

        for (unsigned i = 0; i < this->nr_worker_threads; ++i)
            this->thread_pool.create_thread(bind(&asio::io_service::run, ref(this->io_service)));

        this->thread_pool.join_all();
    }
    catch(const std::system_error& e) {
        std::cerr << "System Error: Not avaliable resources to create peer (data handler listener)!" << std::endl;
        exit(1);
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
}

void data_handler_listener_mt::stop_thread() {
    //terminating ioService processing loop
    this->io_service.stop();
    //joining all threads of thread_loop
    this->thread_pool.join_all();
}
