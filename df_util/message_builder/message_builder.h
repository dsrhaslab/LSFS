//
// Created by danielsf97 on 1/16/20.
//

#ifndef P2PFS_MESSAGE_BUILDER_H
#define P2PFS_MESSAGE_BUILDER_H


#include <iostream>
#include <chrono>
#include <utility>
#include <cstdlib>
#include <ctime>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <kv_message.pb.h>

#include "df_store/kv_store.h"

void build_get_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_key_version& version);
void build_get_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, std::unique_ptr<std::string> data, const std::string& key, const kv_store_key_version& version, bool is_deleted = false);

void build_get_latest_version_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, bool get_data = false);
void build_get_latest_version_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const std::vector<kv_store_key_version>& vversion, bool bring_data, const std::vector<std::unique_ptr<std::string>>& vdata, const std::vector<kv_store_key_version>& vdel_version);

void build_put_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_key_version& version, const char* data, size_t size);
void build_put_with_merge_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_key_version& version, const char* data, size_t size);
void build_put_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_key_version& version, const bool is_merge);

void build_delete_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_key_version& version);
void build_delete_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_key_version& version);

void build_put_child_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_key_version& version, const kv_store_key_version& past_version, std::string& child_path, bool is_create, bool is_dir);

void build_get_latest_metadata_size_or_stat_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, bool get_size, bool get_stat);

void build_get_metadata_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_key_version& version);

#endif //P2PFS_MESSAGE_BUILDER_H
