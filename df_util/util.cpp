//
// Created by danielsf97 on 3/4/20.
//

#include "util.h"

int split_composite_key(std::string comp_key, std::string* key, long* version, long* client_id){
    boost::cmatch match;
    auto res = boost::regex_search(comp_key.c_str(), match, composite_key);

    try{
        if(match.size() > 3){
            *key = std::string(match[1].str());
            *version = std::stol(match[2].str(), nullptr);
            *client_id = std::stol(match[3].str(), nullptr);
        }
    }catch(std::invalid_argument e){
        return -1;
    }
    return 0;
}