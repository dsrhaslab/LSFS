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

static int
my_compare(
        WT_COLLATOR *collator, WT_SESSION *session, const WT_ITEM *k1, const WT_ITEM *k2, int *cmp)
{
    char *key1;
    int64_t version1;
    int64_t client_id1;

    char *key2;
    int64_t version2;
    int64_t client_id2;

    /* Unused parameters */
    (void)collator;

    /* Unpack key 1. */
    wiredtiger_struct_unpack(
            session, k1->data, k1->size, "Sqq", &key1, &version1, &client_id1);

    /* Unpack key 2. */
    wiredtiger_struct_unpack(
            session, k2->data, k2->size, "Sqq", &key2, &version2, &client_id2);

    *cmp = strcmp(key1, key2);
    if(*cmp == 0) *cmp = version1 < version2? -1 : (version1 == version2? 0 : 1);
    if(*cmp == 0) *cmp = client_id1 < client_id2? -1 : (client_id1 == client_id2? 0 : 1);

    return (0);
}

static WT_COLLATOR my_collator = {my_compare, NULL, NULL};

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
    std::string table_name;

private:
    void error_check(int call);
    std::unique_ptr<long> get_client_id_from_key_version(const std::string& key, long version);

public:
    kv_store_wiredtiger(std::string (*f)(const std::string&, const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at);
    ~kv_store_wiredtiger();
    int init(void*, long id) override ;
    void close() override ;
    std::string db_name() const override;
    void update_partition(int p, int np) override;
    std::unordered_set<kv_store_key<std::string>> get_keys() override;
    bool put(const std::string& key, long version, long client_id, const std::string& bytes, bool is_merge) override; // use string.c_str() to convert string to const char*
    bool put_with_merge(const std::string& key, long version, long client_id, const std::string& bytes);
    std::unique_ptr<std::string> get(kv_store_key<std::string>& key) override;
    std::unique_ptr<std::string> remove(const kv_store_key<std::string>& key) override;
    std::unique_ptr<std::string> get_latest(const std::string& key, kv_store_key_version* kv_version) override;
    std::unique_ptr<long> get_latest_version(const std::string& key) override;
    std::unique_ptr<std::string> get_anti_entropy(const kv_store_key<std::string>& key, bool* is_merge) override;
    void remove_from_set_existent_keys(std::unordered_set<kv_store_key<std::string>>& keys) override;
    void print_store() override;
};

kv_store_wiredtiger::kv_store_wiredtiger(std::string (*f)(const std::string&, const std::string&), long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at) {
    this->merge_function = f;
    this->seen_log_garbage_at = seen_log_garbage_at;
    this->request_log_garbage_at = request_log_garbage_at;
    this->anti_entropy_log_garbage_at = anti_entropy_log_garbage_at;
}

kv_store_wiredtiger::~kv_store_wiredtiger() {
    conn->close(conn, nullptr);
}

void kv_store_wiredtiger::close() {
    std::cout << "closing connection" << std::endl;
    conn->close(conn, nullptr);
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
    this->table_name = "table:" + std::to_string(this->id);
    this->path = std::string((char*) path);
    std::string database_path = (char*) path +  std::to_string(id);
    try{
        std::filesystem::create_directories(database_path.c_str());
    }catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }

    int res = wiredtiger_open(database_path.c_str(), nullptr, "create, cache_size=50M" /*default 100M*/, &conn);
    if (res != 0){
        fprintf(stderr,
                "wiredtiger_open: %s\n",
                "Error opening connection to database!");
        return -1;
    }
    res = conn->add_collator(conn, "my_col", &my_collator, nullptr);
    if (res != 0){
        fprintf(stderr,
                "create collator: %s\n",
                "Error adding collator to wiredtiger connection!");
        return -1;
    }
    res = conn->open_session(conn, NULL, NULL, &session);
    if (res != 0){
        fprintf(stderr,
                "open_session: %s\n",
                "Error opening session to wiredtiger!");
        return -1;
    }
    res = session->create(session, this->table_name.c_str(), "key_format=Sqq,value_format=ub,columns=(key,version,client_id,value,merge),collator=my_col ");
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

//TODO talvez mudar este método por causa que implica muitas procuras
void kv_store_wiredtiger::update_partition(int p, int np) {
    WT_CURSOR *cursor;

    if(np != this->nr_slices){
//        std::cout << "UPDATE_PARTITION " << std::to_string(np) << std::endl;
        this->nr_slices = np;
        this->slice = p;
        //clear memory to allow new keys to be stored
        std::scoped_lock<std::recursive_mutex, std::recursive_mutex> lk(this->seen_mutex, this->store_mutex);
        session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor);

        try {
            error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor));
            for(const auto& seen_pair: this->seen){
                if(seen_pair.second) {
                    cursor->set_key(cursor, seen_pair.first.key.c_str(), (int64_t) seen_pair.first.key_version.version,
                                    (int64_t) seen_pair.first.key_version.client_id);
                    int res = cursor->search(cursor);

                    if (res != 0) { // a chave não existe
                        this->seen.insert_or_assign(seen_pair.first, false);
                    }
                    cursor->reset(cursor);
                }
            }
            cursor->close(cursor);
        }catch(WiredTigerException& e){}
    }
}

//TODO melhorar isto
std::unordered_set<kv_store_key<std::string>> kv_store_wiredtiger::get_keys() {
    WT_CURSOR *cursor;
    char *key;
    int64_t version;
    int64_t client_id;

    std::unordered_set<kv_store_key<std::string>> keys;
    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor)); //throw exception

    while (cursor->next(cursor) == 0) {
        cursor->get_key(cursor, &key, &version, &client_id);
        keys.insert({std::string(key), kv_store_key_version((long) version, (long) client_id)});
    }

    cursor->close(cursor);
    return std::move(keys);
}

void kv_store_wiredtiger::remove_from_set_existent_keys(std::unordered_set<kv_store_key<std::string>>& keys){
    WT_CURSOR *cursor;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);

    error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor)); //throw WiredTigerError

    for (auto it_keys = keys.begin(); it_keys != keys.end();) {
        cursor->set_key(cursor, it_keys->key.c_str(), (int64_t) it_keys->key_version.version, (int64_t) it_keys->key_version.client_id);
        int res = cursor->search(cursor);
        if(res == 0){
            // if we have the key
            it_keys = keys.erase(it_keys);
        }else if(this->get_slice_for_key(it_keys->key) != this->slice){
            // se a chave não pertence a esta store
            it_keys = keys.erase(it_keys);
        }else{
            ++it_keys;
        }
        cursor->reset(cursor);
    }

//    while (cursor->next(cursor) == 0) {
//        cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
//        kv_store_key_version temp_version = kv_store_key_version(version_it, client_id_it);
//        kv_store_key<std::string> temp_key = {key_it, std::move(temp_version)};
//        auto it = keys.find(temp_key);
//        if(it != keys.end()) {
//            keys.erase(it);
//        }
//    }
//
//    for (auto it_keys = keys.begin(); it_keys != keys.end();) {
//        if(this->get_slice_for_key(it_keys->key) != this->slice){
//            // se a chave não pertence a esta store
//            it_keys = keys.erase(it_keys);
//        }else{
//            ++it_keys;
//        }
//    }

}

bool kv_store_wiredtiger::put(const std::string& key, long version, long client_id, const std::string& bytes, bool is_merge) {
    WT_CURSOR *cursor;
    WT_ITEM item;

    this->seen_it(key, version, client_id);
    int k_slice = this->get_slice_for_key(key);

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);

    if(this->slice == k_slice){
        try{
            error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, "overwrite=true", &cursor));
        }catch(WiredTigerException& e){
            this->unseen_it(key, version, client_id);
            throw WiredTigerException();
        }
        cursor->set_key(cursor, key.c_str(), (int64_t) version, (int64_t) client_id);
        item.data = bytes.data();
        item.size = bytes.size();
        cursor->set_value(cursor, &item, is_merge);
        cursor->insert(cursor);
        cursor->close(cursor);


        //if(!is_merge){
        //    //if is not a merge operation should only respond that put was sucessfully if the
        //    //version commited is actualy the current version

        //    try {
        //        std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
        //        if (max_client_id == nullptr){
        //            return false;
        //        }else{
        //            return *max_client_id == client_id;
        //        }
        //    }catch(WiredTigerException e){
        //        this->unseen_it(key, version, client_id);
        //        throw WiredTigerException();
        //    }
        //}

        return true;
    }else{
        //Object received but does not belong to this df_store.
        return false;
    }
}

std::unique_ptr<long> kv_store_wiredtiger::get_client_id_from_key_version(const std::string& key, long version){
    WT_CURSOR *cursor;
    char *key_it;
    int64_t version_it;
    int64_t client_id_it;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);

    error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor)); //throw WiredTigerError

    const char* search_key = key.c_str();
    cursor->set_key(cursor, search_key, (int64_t) version , (int64_t) LONG_MAX); // set to possible max client
    int exact;
    int ret = cursor->search_near(cursor, &exact);
    if (ret == 0 && exact >= 0)
        ret = cursor->prev(cursor);
    if(ret == 0){
        cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
        if(strcmp(search_key, key_it) == 0 &&  version == version_it) {
            cursor->close(cursor);
            return std::make_unique<long>(client_id_it);
        }
    }

    return nullptr;
}

void kv_store_wiredtiger::print_store(){
    WT_CURSOR *cursor;
    char *key;
    int64_t version;
    int64_t client_id;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    try {
        error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor));

        std::cout << "================= MY STORE =============" << std::endl;
        int i;
        while ((i = cursor->next(cursor)) == 0) {
            cursor->get_key(cursor, &key, &version, &client_id);
            std::cout << std::string(key) << ": " << (long) version << ": " << (long) client_id << std::endl;
        }
        std::cout << "========================================" << std::endl;
        cursor->close(cursor);
    }catch(WiredTigerException& e){}
}

//USAR LONG_MAX
std::unique_ptr<std::string> kv_store_wiredtiger::get(kv_store_key<std::string>& key) {
    WT_CURSOR *cursor;
    WT_ITEM value;
    bool is_merge;

    char *key_it;
    int64_t version_it;
    int64_t client_id_it;


    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    try {
        error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor));
    }catch(WiredTigerException& e){
        return nullptr;
    }

    if(key.key_version.client_id == -1){
        //if we want some version but we don't know the client for that version
        const char* search_key = key.key.c_str();
        cursor->set_key(cursor, search_key, (int64_t) key.key_version.version, (int64_t) LONG_MAX); // set to possible max client
        int exact;
        int ret = cursor->search_near(cursor, &exact);
        if (ret == 0 && exact >= 0)
            ret = cursor->prev(cursor);
        if(ret == 0){
            cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
            if(strcmp(search_key, key_it) == 0 &&  key.key_version.version == version_it) {
                key.key_version.client_id = client_id_it;
                cursor->get_value(cursor, &value, &is_merge);
                cursor->close(cursor);
                return std::make_unique<std::string>(std::string((char*) value.data, value.size));
            }
        }
        //if the key dont exists or dont exists the wanted version
        return nullptr;
    }else{
        //if we want a precise (version, client_id) of a key
        cursor->set_key(cursor, key.key.c_str(), (int64_t) key.key_version.version, (int64_t) key.key_version.client_id);
        int res = cursor->search(cursor);
        if(res == 0){
            cursor->get_value(cursor, &value, &is_merge);
            cursor->close(cursor);
            return std::make_unique<std::string>(std::string((char*) value.data, value.size));
        }else{
            cursor->close(cursor);
            return nullptr;
        }
    }
}

std::unique_ptr<std::string> kv_store_wiredtiger::get_anti_entropy(const kv_store_key<std::string>& key, bool* is_merge) {
    WT_CURSOR *cursor;
    WT_ITEM value;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    try {
        error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor));

        cursor->set_key(cursor, key.key.c_str(), (int64_t) key.key_version.version, (int64_t) key.key_version.client_id);
        int res = cursor->search(cursor);
        if(res == 0){
            cursor->get_value(cursor, &value, is_merge);
            cursor->close(cursor);
            return std::make_unique<std::string>(std::string((char*) value.data, value.size));
        }else{
            cursor->close(cursor);
            return nullptr;
        }
    }catch(WiredTigerException& e){
        return nullptr;
    }
}

std::unique_ptr<std::string> kv_store_wiredtiger::get_latest(const std::string& key, kv_store_key_version* kv_version) {
    WT_CURSOR *cursor;
    WT_ITEM value;
    bool merge;
    char *key_it;
    int64_t version_it;
    int64_t client_id_it;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    try {
        error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor));
    }catch(WiredTigerException& e){
        return nullptr;
    }

//    std::string next_key = key;
    //increase last character of string
//    next_key.back()++;
    cursor->set_key(cursor, key.c_str(), (int64_t) LONG_MAX, (int64_t) LONG_MAX);
    int exact;
    int ret = cursor->search_near(cursor, &exact);
    if (ret == 0 && exact >= 0)
        ret = cursor->prev(cursor);
    if(ret == 0){
        cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
        if(strcmp(key.c_str(),key_it) == 0) {
            kv_version->version = version_it;
            kv_version->client_id = client_id_it;
            cursor->get_value(cursor, &value, &merge);
            cursor->close(cursor);
            return std::make_unique<std::string>(std::string((char*) value.data, value.size));
        }
    }

    cursor->close(cursor);
    return nullptr;
}

std::unique_ptr<long> kv_store_wiredtiger::get_latest_version(const std::string& key) {
    WT_CURSOR *cursor;
    char *key_it;
    int64_t version_it;
    int64_t client_id_it;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    try {
        error_check(session->open_cursor(session, this->table_name.c_str(), nullptr, nullptr, &cursor));

//        std::string next_key = key;
        //increase last character of string
//        next_key.back()++;
        cursor->set_key(cursor, key.c_str(), (int64_t) LONG_MAX, (int64_t) LONG_MAX);
        int exact;
        int ret = cursor->search_near(cursor, &exact);
        if (ret == 0 && exact >= 0)
            ret = cursor->prev(cursor);
        if(ret == 0){
            cursor->get_key(cursor, &key_it, &version_it, &client_id_it);
            if(strcmp(key.c_str(),key_it) == 0) {
                cursor->close(cursor);
                return std::make_unique<long>(version_it);
            }
        }

        cursor->close(cursor);
    }catch(WiredTigerException& e){
        return nullptr;
    }

    return nullptr;
}

std::unique_ptr<std::string> kv_store_wiredtiger::remove(const kv_store_key<std::string>& key) {
    WT_CURSOR *cursor;
    WT_ITEM value;
    bool is_merge;
    std::unique_ptr<std::string> ret = nullptr;

    std::scoped_lock<std::recursive_mutex> lk(this->store_mutex);
    error_check(session->open_cursor(session,this->table_name.c_str(), nullptr, nullptr, &cursor));
    cursor->set_key(cursor, key.key.c_str(), (int64_t) key.key_version.version, (int64_t) key.key_version.client_id);

    int res = cursor->search(cursor);
    if(res == 0){
        cursor->get_value(cursor, &value, &is_merge);
        ret = std::make_unique<std::string>(std::string((char*) value.data, value.size));
        cursor->remove(cursor);
    }

    cursor->close(cursor);

    return ret;

}

bool kv_store_wiredtiger::put_with_merge(const std::string& key, long version, long client_id, const std::string& bytes) {
    try{
//        std::unique_ptr<long> max_client_id = get_client_id_from_key_version(key, version);
//        if(max_client_id == nullptr){
//            //no conflict
//            return put(key, version, client_id, bytes);
//        }else{
//            if(*max_client_id != client_id){
//                kv_store_key<std::string> kv_key = {key, kv_store_key_version(version, *max_client_id)};
//                std::shared_ptr<std::string> data = get(kv_key);
//                if(data == nullptr){
//                    // caso ocorresse algum erro
//                    return false;
//                }
//                return put(key, version, std::max(*max_client_id, client_id), this->merge_function(*data, bytes));
//            }
//        }
//
//        return true;

        // quer exista ou não uma entrada para a mesma versão-client_id de outro cliente
        // faz-se put na mesma da versão deste cliente para ter registo que a versão
        // de tal cliente foi processada e por motivos de anti-entropia (não requerer
        // versões-client_id já processados)
        int res = put(key, version, client_id, bytes, true);
        if(res == false){
            return false;
        }

        kv_store_key_version kv_version;
        //We only need to merge with latest version
        std::unique_ptr<std::string> data = get_latest(key, &kv_version);
        if(data != nullptr){
            if(kv_version.version > version){
                //if there is a version bigger than mine, use its client_id
                return put(key, version, client_id, this->merge_function(*data, bytes), true);
            }else if(kv_version.client_id != client_id){
                //if there is no version bigger than mine, use max client_id for merge
                return put(key, version, std::max(kv_version.client_id, client_id), this->merge_function(*data, bytes), true);
            }

            //No need for merge, my version-client_id is bigger than the others
            return true;
        }else{
            // error on get
            throw WiredTigerException();
        }

    }catch(WiredTigerException& e){
        return false;
    }
}

#endif //P2PFS_KV_STORE_WIREDTIGER_H
