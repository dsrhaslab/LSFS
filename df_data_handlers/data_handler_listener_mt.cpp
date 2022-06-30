//
// Created by danielsf97 on 5/24/20.
//

#include "data_handler_listener_mt.h"

class data_handler_listener_worker : public udp_handler{
    // This class is shared by all threads
private:
    data_handler_listener* data_handler;

public:
    data_handler_listener_worker(data_handler_listener* data_handler): data_handler(data_handler){}

    void handle_function(const char *data, size_t size) override {
        try {
            proto::kv_message msg;
            msg.ParseFromArray(data, size);

            if(msg.has_get_msg()){
                std::cout << "Received get message from client " << std::endl;
                data_handler->process_get_message(msg);
            }else if(msg.has_get_latest_version_msg()){
                std::cout << "Received get latest message from client " << std::endl;
                data_handler->process_get_latest_version_msg(msg);
            }else if(msg.has_put_msg()){
                std::cout << "Received put message from client " << std::endl;
                data_handler->process_put_message(msg);
            }else if(msg.has_put_with_merge_msg()){
                std::cout << "Received put with merge message from client " << std::endl;
                data_handler->process_put_with_merge_message(msg);
            }else if(msg.has_delete_msg()){
                std::cout << "Received delete message from client " << std::endl;
                data_handler->process_delete_message(msg);
            }else if(msg.has_put_child_msg()){
                std::cout << "Received Put child message from client " << std::endl;
                data_handler->process_put_child_message(msg);
            }else if(msg.has_put_met_stat_msg()){
                std::cout << "Received Put metadata stat from client " << std::endl;
                data_handler->process_put_metadata_stat_message(msg);
            }else if(msg.has_get_latest_met_size_or_stat_msg()){
                std::cout << "Received get latest size or stat message from client " << std::endl;
                data_handler->process_get_latest_metadata_size_or_stat_msg(msg);
            }else if(msg.has_get_met_msg()){
                std::cout << "Received get metadata message from client " << std::endl;
                data_handler->process_get_metadata_message(msg);
            }else if(msg.has_anti_entropy_msg()){
                data_handler->process_anti_entropy_message(msg);
            }else if(msg.has_anti_entropy_get_msg()){
                data_handler->process_anti_entropy_get_message(msg);
            }else if(msg.has_anti_entropy_get_met_msg()){
                data_handler->process_anti_entropy_get_metadata_message(msg);
            }else if(msg.has_anti_entropy_get_reply_msg()){
                data_handler->process_anti_entropy_get_reply_message(msg);
            }else if(msg.has_anti_entropy_get_met_reply_msg()){
                data_handler->process_anti_entropy_get_metadata_reply_message(msg);
            }else if(msg.has_recover_request_msg()){
                data_handler->process_recover_request_msg(msg);
            }

        }
        catch(const char* e){
            spdlog::error(e);
        }
        catch(...){}
    }
};

data_handler_listener_mt::data_handler_listener_mt(std::string ip, int kv_port, long id, float chance, pss *pss, group_construction* group_c, anti_entropy* anti_ent, std::shared_ptr<kv_store<std::string>> store, bool smart)
        : data_handler_listener(std::move(ip), kv_port, id, chance, pss, group_c, anti_ent, std::move(store), smart){}

void data_handler_listener_mt::operator()() {

    //wait for database to recover
    this->anti_ent_ptr->wait_while_recovering();

    try {
        data_handler_listener_worker worker(this);
        udp_async_server server(this->io_service, this->kv_port, (udp_handler*) &worker);

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
