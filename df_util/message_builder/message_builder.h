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

void build_get_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_version& version, bool no_data);
void build_get_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, std::unique_ptr<std::string> data, const std::string& key, const kv_store_version& version, bool is_deleted = false);

void build_get_latest_version_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, bool get_data = false);
void build_get_latest_version_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const std::vector<kv_store_version>& vversion, bool bring_data, const std::vector<std::unique_ptr<std::string>>& vdata, const std::vector<kv_store_version>& vdel_version);

void build_put_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_version& version, FileType::FileType f_type, const char* data, size_t size, bool extra_reply);
void build_put_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_version& version, FileType::FileType f_type);

void build_delete_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_version& version, FileType::FileType f_type, bool extra_reply);
void build_delete_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_version& version, FileType::FileType f_type);

void build_get_latest_metadata_stat_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key);

void build_put_child_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& key, const kv_store_version& version, const std::string& child_path, bool is_create, bool is_dir, bool extra_reply);

void build_get_latest_metadata_size_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key);

void build_get_metadata_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_version& version);
void build_get_metadata_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, std::unique_ptr<std::string> data, const std::string& key, const kv_store_version& version, bool is_deleted, bool higher_version);

void build_anti_entropy_get_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_version& version, FileType::FileType f_type, bool is_deleted);
void build_anti_entropy_get_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_version& version, FileType::FileType f_type, bool is_deleted, std::unique_ptr<std::string> data);

void build_anti_entropy_get_metadata_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_version& version, FileType::FileType f_type, bool is_deleted);
void build_anti_entropy_get_metadata_reply_message(proto::kv_message* msg, std::string& ip, int kv_port, long id, const std::string& req_id, const std::string& key, const kv_store_version& version, FileType::FileType f_type, bool is_deleted, std::unique_ptr<std::string> data);

#endif //P2PFS_MESSAGE_BUILDER_H
