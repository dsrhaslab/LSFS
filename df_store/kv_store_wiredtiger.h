//
// Created by danielsf97 on 2/24/20.
//

#ifndef P2PFS_KV_STORE_WIREDTIGER_H
#define P2PFS_KV_STORE_WIREDTIGER_H

#include <wiredtiger.h>
#include "exceptions/custom_exceptions.h"
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

/*
 * TODO: Melhorar isto: get latest e get latest version estão a percorrer a db toda
 * -> Coisas que já tentei:
 *      -> criar um index: (não funciona porque o set_key corresponde à key do index, pelo que
 *      não consigo ir buscar a chave primária associada, bem como penso que só funciona para valores
 *      de indice diferentes )
 *      -> tentei ainda fazer search_near: não funciona porque não ordena da forma esperada as chaves
 * */

class kv_store_wiredtiger: public kv_store<std::string>{
private:
    WT_CONNECTION *conn;
    WT_SESSION *session;
    std::recursive_mutex store_mutex;

private:
    void error_check(int call);
    std::unique_ptr<long> get_client_id_from_key_version(std::string key, long version);

public:
    explicit kv_store_wiredtiger(std::string (*f)(std::string&, std::string&));
    ~kv_store_wiredtiger();
    int init(void*, long id) override ;
    void close() override ;
    std::string db_name() const override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<std::string>> get_keys() override;
    bool put(std::string key, long version, long client_id, std::string bytes) override; // use string.c_str() to convert string to const char*
    bool put_with_merge(std::string key, long version, long client_id, std::string bytes);
    std::shared_ptr<std::string> get(kv_store_key<std::string>& key) override;
    std::shared_ptr<std::string> remove(kv_store_key<std::string> key) override;
    std::shared_ptr<std::string> get_latest(std::string key, kv_store_key_version* kv_version) override;
    std::unique_ptr<long> get_latest_version(std::string key) override;
    void print_store() override;
};

kv_store_wiredtiger::kv_store_wiredtiger(std::string (*f)(std::string&, std::string&)) {
    this->merge_function = f;
}

kv_store_wiredtiger::~kv_store_wiredtiger() {
    conn->close(conn, NULL);
}

void kv_store_wiredtiger::close() {
    std::cout << "closing connection" << std::endl;
    conn->close(conn, NULL);
}

std::string kv_store_wiredtiger::db_name() const {
    return "wiredDB";
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
//        std::cout << "\033[1;31mAll Good\033[0m" << std::endl;
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
    res = session->create(session, table_name.c_str(), "key_format=Sll,value_format=u,columns=(key,version,client_id,value)");
    if (res != 0){
        fprintf(stderr,
                "create_table: %s\n",
                "Error creating wiredtiger table!");
        return -1;
    }
//    /* Create an immutable index. */
//    /* Immutable - This setting tells WiredTiger that the index keys for a record do not change when records are updated*/
//    std::string index_name = "index:" + std::to_string(this->id) + ":key";
//    res = session->create(session, index_name.c_str(), "columns=(key),immutable");
//    if (res != 0){
//        fprintf(stderr,
//                "create_table: %s\n",
//                "Error creating wiredtiger index!");
//        return -1;
//    }

    return 0;
}

void kv_store_wiredtiger::update_partition(int p, int np) {
    WT_CURSOR *cursor;

    if(np != this->nr_slices){
//        std::cout << "UPDATE_PARTITION " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->seen_mutex, this->store_mutex);
        std::string table_name = "table:" + std::to_string(this->id);
        session->open_cursor(session, table_name.c_str(), NULL, NULL, &cursor);

        try {
            error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));
            for(const auto& seen_pair: this->seen){
                cursor->set_key(cursor, seen_pair.first.key.c_str(), seen_pair.first.key_version.version, seen_pair.first.key_version.client_id);
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
    uint32_t client_id;

    std::unordered_set<kv_store_key<std::string>> keys;
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor)); //throw exception

    while (cursor->next(cursor) == 0) {
        cursor->get_key(cursor, &key, &version, &client_id);
        keys.insert({std::string(key), kv_store_key_version((long) version, (long) client_id)});
    }

    cursor->close(cursor);
    return std::move(keys);
}

bool kv_store_wiredtiger::put(std::string key, long version, long client_id, std::string bytes) {
    WT_CURSOR *cursor;
    WT_ITEM item;

    this->seen_it(key, version, client_id);
    int k_slice = this->get_slice_for_key(key);

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    if(this->slice == k_slice){
        std::string table_name = "table:" + std::to_string(this->id);
        try{
            error_check(session->open_cursor(session, table_name.c_str(), nullptr, "overwrite=true", &cursor));
        }catch(WiredTigerException& e){
            this->unseen_it(key, version, client_id);
            throw WiredTigerException();
        }
        cursor->set_key(cursor, key.c_str(), version, client_id);
        auto data_size = bytes.size();
        char buf[data_size];
        bytes.copy(buf, data_size);
        item.data = buf;
        item.size = data_size;
        cursor->set_value(cursor, &item);
        cursor->insert(cursor);
        cursor->close(cursor);

        try {
            std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
            if (max_client_id == nullptr){
                return false;
            }else{
                return *max_client_id == client_id;
            }
        }catch(WiredTigerException e){
            this->unseen_it(key, version, client_id);
            throw WiredTigerException();
        }
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

std::unique_ptr<long> kv_store_wiredtiger::get_client_id_from_key_version(std::string key, long version){
    WT_CURSOR *cursor;
    char *key_it;
    uint32_t version_it;
    uint32_t client_id_it;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);

    error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor)); //throw WiredTigerError

    auto current_max_version = kv_store_key_version(version);
    bool exists = false;

    while (cursor->next(cursor) == 0) {
        cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
        kv_store_key_version temp_version = kv_store_key_version(version_it, client_id_it);
        if(strcmp(key.c_str(),key_it) == 0 && version_it == version && temp_version >= current_max_version) {
            current_max_version = temp_version;
            exists = true;
        }
    }

    if(exists){
        return std::make_unique<long>(current_max_version.client_id);
    }

    return nullptr;
}

void kv_store_wiredtiger::print_store(){
    WT_CURSOR *cursor;
    char *key;
    uint32_t version;
    uint32_t client_id;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    try {
        error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));

        std::cout << "================= MY STORE =============" << std::endl;
        int i;
        while ((i = cursor->next(cursor)) == 0) {
            cursor->get_key(cursor, &key, &version, &client_id);
            std::cout << std::string(key) << ": " << (long) version << ": " << (long) client_id << std::endl;
        }
        std::cout << "========================================" << std::endl;
        cursor->close(cursor);
    }catch(WiredTigerException e){}
}

std::shared_ptr<std::string> kv_store_wiredtiger::get(kv_store_key<std::string>& key) {
    WT_CURSOR *cursor;
    WT_ITEM value;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    try {
        error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));
    }catch(WiredTigerException e){
        return nullptr;
    }

    if(key.key_version.client_id == -1){
        std::unique_ptr<long> current_max_client_id(nullptr);
        try{
            current_max_client_id = get_client_id_from_key_version(key.key, key.key_version.version);
        }catch(WiredTigerException e){}

        if(current_max_client_id == nullptr){
            return nullptr;
        }
        key.key_version.client_id = *current_max_client_id;
    }


    cursor->set_key(cursor, key.key.c_str(), key.key_version.version, key.key_version.client_id);
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

std::shared_ptr<std::string> kv_store_wiredtiger::get_latest(std::string key, kv_store_key_version* kv_version) {
    WT_CURSOR *cursor;
    WT_ITEM value;
    char *key_it;
    uint32_t version_it;
    uint32_t client_id_it;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    try {
        error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));
    }catch(WiredTigerException e){
        return nullptr;
    }
//    cursor->set_key(cursor, key.c_str());

    std::string current_key;
    auto current_max_version = kv_store_key_version(LONG_MIN);

    while (cursor->next(cursor) == 0) {
        cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
        kv_store_key_version temp_version = kv_store_key_version(version_it, client_id_it);
        if(strcmp(key.c_str(),key_it) == 0 && temp_version >= current_max_version) {
            current_max_version = temp_version;
            current_key = std::string(key_it);
        }
    }

    if(!current_key.empty()){
        *kv_version = current_max_version;
        cursor->set_key(cursor, key.c_str(), current_max_version.version, current_max_version.client_id);

        int res = cursor->search(cursor);
        if(res == 0){
            cursor->get_value(cursor, &value);
            cursor->close(cursor);
            return std::make_shared<std::string>(std::string((char*) value.data, value.size));
        }
    }

    cursor->close(cursor);
    return nullptr;
}

std::unique_ptr<long> kv_store_wiredtiger::get_latest_version(std::string key) {
    WT_CURSOR *cursor;
    WT_ITEM value;
    char *key_it;
    uint32_t version_it;
    uint32_t client_id_it;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    try {
        error_check(session->open_cursor(session, table_name.c_str(), nullptr, nullptr, &cursor));
    }catch(WiredTigerException e){
        return nullptr;
    }
//    cursor->set_key(cursor, key.c_str());

    std::string current_key;
    long current_max_version = LONG_MIN;

    while (cursor->next(cursor) == 0) {
        cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
        if(strcmp(key.c_str(),key_it) == 0 && version_it >= current_max_version){
            current_max_version = version_it;
            current_key = std::string(key_it);
        }
    }

    if(!current_key.empty()){
        cursor->close(cursor);
        return std::make_unique<long>(current_max_version);
    }

    cursor->close(cursor);
    return nullptr;
}

std::shared_ptr<std::string> kv_store_wiredtiger::remove(kv_store_key<std::string> key) {
    WT_CURSOR *cursor;
    WT_ITEM value;
    std::shared_ptr<std::string> ret = nullptr;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    std::string table_name = "table:" + std::to_string(this->id);
    error_check(session->open_cursor(session,table_name.c_str(), nullptr, nullptr, &cursor));
    cursor->set_key(cursor, key.key.c_str(), key.key_version.version, key.key_version.client_id);


    int res = cursor->search(cursor);
    if(res == 0){
        cursor->get_value(cursor, &value);
        ret = std::make_shared<std::string>(std::string((char*) value.data, value.size));
        cursor->remove(cursor);
    }

    cursor->close(cursor);

    return ret;

}

bool kv_store_wiredtiger::put_with_merge(std::string key, long version, long client_id, std::string bytes) {
    try{
        std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
        if(max_client_id == nullptr){
            //no conflict
            return put(key, version, client_id, bytes);
        }else{
            if(*max_client_id != client_id){
                kv_store_key<std::string> kv_key = {key, kv_store_key_version(version, *max_client_id)};
                std::shared_ptr<std::string> data = get(kv_key);
                if(data == nullptr){
                    // caso ocorresse algum erro
                    return false;
                }
                return put(key, version, std::max(*max_client_id, client_id), this->merge_function(*data, bytes));
            }
        }

        return true;

    }catch(WiredTigerException e){
        return false;
    }
}

#endif //P2PFS_KV_STORE_WIREDTIGER_H
