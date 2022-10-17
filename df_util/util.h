#ifndef P2PFS_UTIL_H
#define P2PFS_UTIL_H

#include <iostream>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "df_store/kv_store_key.h"
#include "lsfs/fuse_common/macros.h"


static boost::regex composite_key("(.+)#(.+)#(.+)$");
static boost::regex base_path_pattern("(.*):[^:]+$");
static boost::regex blk_num_pattern(":([^:]+)$");


std::string get_local_ip_address();
std::string compose_key_toString(std::string key, kv_store_version version);
int get_base_path(const std::string& key, std::string* base_path);
int get_blk_num(const std::string& key, std::string* blk_num);
void print_kv(const kv_store_version& kv);
void print_kv(const kv_store_key<std::string>& kv);
kVersionComp comp_version (const kv_store_version& k1, const kv_store_version& k2);
kv_store_version merge_kv(const kv_store_version& k1, const kv_store_version& k2);
kv_store_version add_vv(std::pair<long, long> version, const kv_store_version& k2);
kv_store_version choose_latest_version(std::vector<kv_store_version>& kvv_v);
std::unique_ptr<std::string> split_data(std::unique_ptr<std::string> data, int blk_num);

#endif //P2PFS_UTIL_H
