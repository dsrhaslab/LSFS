//
// Created by danielsf97 on 3/4/20.
//

#ifndef P2PFS_UTIL_H
#define P2PFS_UTIL_H

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include "df_store/kv_store_key.h"
#include <iostream>

static boost::regex composite_key("(.+)#(.+)$");


int split_composite_total(std::string comp_key, std::string* key, std::map<long, long>* version);

int split_composite_key(std::string comp_key, std::string* key);

std::string compose_key_toString(std::string key, kv_store_key_version version);

std::map<long, long>* str2vv(std::string vv_str);

std::string vv2str(std::map<long, long> vv);

#endif //P2PFS_UTIL_H
