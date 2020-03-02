//
// Created by danielsf97 on 2/24/20.
//

#ifndef P2PFS_KV_STORE_WIREDTIGER_H
#define P2PFS_KV_STORE_WIREDTIGER_H

#include <wiredtiger.h>
#include "../exceptions/custom_exceptions.h"
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <memory>
#include "kv_store_key.h"
#include "kv_store.h"
#include <climits>
#include <iostream>
#include <cstring>
#include <functional>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
namespace fs = std::filesystem;

class kv_store_wiredtiger: public kv_store<std::string>{
private:
    WT_CONNECTION *conn;
    WT_SESSION *session;
    long id;
    std::string path;
    std::atomic<int> slice = 1; //[1, nr_slices]
    std::atomic<int> nr_slices = 1;
    std::unordered_map<kv_store_key<std::string>, bool> seen;
    std::unordered_map<std::string, bool> request_log;
    std::unordered_map<std::string, bool> anti_entropy_log;
    std::recursive_mutex seen_mutex;
    std::recursive_mutex store_mutex;
    std::recursive_mutex req_log_mutex;
    std::recursive_mutex anti_entropy_log_mutex;

private:
    void error_check(int call);

public:
    ~kv_store_wiredtiger();
    int init(void*, long id) override ;
    void close() override ;
    int get_slice_for_key(std::string key) override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<std::string>> get_keys() override;
    bool have_seen(std::string key, long version) override;
    void seen_it(std::string key, long version) override;
    bool put(std::string key, long version, std::string bytes) override; // use string.c_str() to convert string to const char*
    std::shared_ptr<std::string> get(kv_store_key<std::string> key) override;
    std::shared_ptr<std::string> remove(kv_store_key<std::string> key) override;
    int get_slice() override;
    void set_slice(int slice) override;
    int get_nr_slices() override;
    void set_nr_slices(int nr_slices) override;
    bool in_log(std::string req_id);
    void log_req(std::string req_id);
    bool in_anti_entropy_log(std::string req_id);
    void log_anti_entropy_req(std::string req_id);
    void print_store();
};

kv_store_wiredtiger::~kv_store_wiredtiger() {

    conn->close(conn, NULL);
}

void kv_store_wiredtiger::close() {
    std::cout << "closing connection" << std::endl;
    conn->close(conn, NULL);
}

void kv_store_wiredtiger::error_check(int call){
    int res;
    if ((res = (call)) != 0 && res != ENOTSUP){
        std::string err;
        switch (res) {
            case WT_ROLLBACK:
                err = "WT_ROLLBACK";
                break;
            case WT_DUPLICATE_KEY:
                err = "WT_DUPLICATE_KEY";
                break;
            case WT_ERROR:
                err = "WT_ERROR";
                break;
            case WT_NOTFOUND:
                err = "WT_NOTFOUND";
                break;
            case WT_PANIC:
                err = "WT_PANIC";
                this->close();
                this->init((void *) this->path.c_str(), this->id);
                break;
            case WT_RUN_RECOVERY:
                err = "WT_RUN_RECOVERY";
                break;
            default:
                err = "DEFAULT";
                break;
        }
        std::cout << "\033[1;31mERROR WIREDTIGER: \033[0m" << err << std::endl;
        throw WiredTigerException();
    }else{
        std::cout << "\033[1;31mAll Good\033[0m" << std::endl;
    }
}

int kv_store_wiredtiger::init(void* path, long id){
    this->id = id;
    this->path = std::string((char*) path);
    std::string database_path = (char*) path +  std::to_string(id);
    try{
        std::filesystem::create_directories(database_path.c_str());
    }catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    int res = wiredtiger_open(database_path.c_str(), NULL, "create, cache_size=50M" /*default 100M*/, &conn);
    if (res != 0){
        fprintf(stderr,
                "wiredtiger_open: %s\n",
                "Error opening connection to database!");
        return -1;
    }
    res = conn->open_session(conn, NULL, NULL, &session);
    if (res != 0){
        fprintf(stderr,
                "open_session: %s\n",
                "Error opening session to wiredtiger!");
        return -1;
    }
    std::string table_name = "table:" + std::to_string(this->id);
    res = session->create(session, table_name.c_str(), "key_format=Sl,value_format=u");
    if (res != 0){
        fprintf(stderr,
                "create_table: %s\n",
                "Error creating wiredtiger table!");
        return -1;
    }
    return 0;
}

int kv_store_wiredtiger::get_slice_for_key(std::string key) {
    if(nr_slices == 1) return 1;

    size_t max = SIZE_MAX;
    size_t min = 0;
    size_t target = std::hash<std::string>()(key);
    size_t step = (max / this->nr_slices);

    size_t current = min;
    int slice = 1;
    size_t next_current = current + step;

    while (target > next_current){
        current = next_current;
        next_current = current + step;
        if(current > 0 && next_current < 0) break; //in the case of overflow
        slice = slice + 1;
    }

    if(slice > this->nr_slices){
        slice = this->nr_slices - 1;
    }

    return slice;
}

void kv_store_wiredtiger::update_partition(int p, int np) {
    WT_CURSOR *cursor;

    if(np != this->nr_slices){
        std::cout << "UPDATE_PARTITION " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->seen_mutex, this->store_mutex);
        std::string table_name = "table:" + std::to_string(this->id);
        session->open_cursor(session, table_name.c_str(), NULL, NULL, &cursor);

        try {
            error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));
            for(const auto& seen_pair: this->seen){
                cursor->set_key(cursor, seen_pair.first.key.c_str(), seen_pair.first.version);
                int res = cursor->search(cursor);

                if(res != 0) { // a chave não existe
                    this->seen.insert_or_assign(seen_pair.first, false);
                }
                cursor->reset(cursor);
            }
            cursor->close(cursor);
        }catch(WiredTigerException e){}
    }
}

std::unordered_set<kv_store_key<std::string>> kv_store_wiredtiger::get_keys() {
    WT_CURSOR *cursor;
    char *key;
    uint32_t version;

    std::unordered_set<kv_store_key<std::string>> keys;
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor)); //throw exception

    int i;
    while ((i = cursor->next(cursor)) == 0) {
        cursor->get_key(cursor, &key, &version);
        keys.insert({std::string(key), (long) version});
    }

    cursor->close(cursor);
    return std::move(keys);
}

bool kv_store_wiredtiger::have_seen(std::string key, long version) {
    kv_store_key<std::string> key_to_check({key, version});

    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    auto it = this->seen.find(key_to_check);
    if(it == this->seen.end()){ //a chave não existe no mapa seen
        return false;
    }else{
        return it->second;
    }
}

void kv_store_wiredtiger::seen_it(std::string key, long version) {
    kv_store_key<std::string> key_to_insert({key, version});
    std::scoped_lock<std::recursive_mutex> lk(this->seen_mutex);
    this->seen.insert_or_assign(std::move(key_to_insert), true);
}

bool kv_store_wiredtiger::put(std::string key, long version, std::string bytes) {
    WT_CURSOR *cursor;
    WT_ITEM item;

    this->seen_it(key, version);
    int k_slice = this->get_slice_for_key(key);

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    if(this->slice == k_slice){
        std::string table_name = "table:" + std::to_string(this->id);
        error_check(session->open_cursor(session, table_name.c_str(), nullptr, "overwrite=true", &cursor)); //throw exception
        cursor->set_key(cursor, key.c_str(), version);
        auto data_size = bytes.size();
        char buf[data_size];
        bytes.copy(buf, data_size);
        item.data = buf;
        item.size = data_size;
        cursor->set_value(cursor, &item);
        cursor->insert(cursor);
        cursor->close(cursor);
        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

void kv_store_wiredtiger::print_store(){
    WT_CURSOR *cursor;
    char *key;
    uint32_t version;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    try {
        error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));

        std::cout << "================= MY STORE =============" << std::endl;
        int i;
        while ((i = cursor->next(cursor)) == 0) {
            cursor->get_key(cursor, &key, &version);
            std::cout << std::string(key) << ": " << (long) version << std::endl;
        }
        std::cout << "========================================" << std::endl;
        cursor->close(cursor);
    }catch(WiredTigerException e){}
}

std::shared_ptr<std::string> kv_store_wiredtiger::get(kv_store_key<std::string> key) {
    WT_CURSOR *cursor;
    WT_ITEM value;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    try {
        error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));
    }catch(WiredTigerException e){
        return nullptr;
    }
    cursor->set_key(cursor, key.key.c_str(), key.version);
    int res = cursor->search(cursor);
    if(res == 0){
        cursor->get_value(cursor, &value);
        cursor->close(cursor);
        return std::make_shared<std::string>(std::string((char*) value.data, value.size));
    }else{
        cursor->close(cursor);
        return nullptr;
    }
}

std::shared_ptr<std::string> kv_store_wiredtiger::remove(kv_store_key<std::string> key) {
    WT_CURSOR *cursor;
    WT_ITEM value;
    std::shared_ptr<std::string> ret = nullptr;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    error_check(session->open_cursor(session,table_name.c_str(), nullptr, nullptr, &cursor));
    cursor->set_key(cursor, key.key.c_str(), key.version);


    int res = cursor->search(cursor);
    if(res == 0){
        cursor->get_value(cursor, &value);
        ret = std::make_shared<std::string>(std::string((char*) value.data, value.size));
        cursor->remove(cursor);
    }

    cursor->close(cursor);

    return ret;

}

int kv_store_wiredtiger::get_slice() {
    return this->slice;
}

void kv_store_wiredtiger::set_slice(int slice) {
    this->slice = slice;
}

int kv_store_wiredtiger::get_nr_slices() {
    return this->nr_slices;
}

void kv_store_wiredtiger::set_nr_slices(int nr_slices) {
    this->nr_slices = nr_slices;
}

bool kv_store_wiredtiger::in_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    return !(this->request_log.find(req_id) == this->request_log.end());
}

void kv_store_wiredtiger::log_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->req_log_mutex);
    this->request_log.insert_or_assign(req_id, true);
}

bool kv_store_wiredtiger::in_anti_entropy_log(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    return !(this->anti_entropy_log.find(req_id) == this->anti_entropy_log.end());
}

void kv_store_wiredtiger::log_anti_entropy_req(std::string req_id) {
    std::scoped_lock<std::recursive_mutex> lk(this->anti_entropy_log_mutex);
    this->anti_entropy_log.insert_or_assign(req_id, true);
}

#endif //P2PFS_KV_STORE_WIREDTIGER_H
