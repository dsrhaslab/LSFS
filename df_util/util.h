//
// Created by danielsf97 on 3/4/20.
//

#ifndef P2PFS_UTIL_H
#define P2PFS_UTIL_H

#include <boost/regex.hpp>
//#include <regex>

static boost::regex composite_key("(.+)#(\\d+)#(\\d+)$");

int split_composite_key(std::string comp_key, std::string* key, long* version, long* client_id);


#endif //P2PFS_UTIL_H
