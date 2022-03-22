//
// Created by danielsf97 on 3/4/20.
//

#include "util.h"

int split_composite_total(std::string comp_key, std::string* key, std::map<long, long>* version){
    boost::cmatch match;
    auto res = boost::regex_search(comp_key.c_str(), match, composite_key);

    try{
        if(match.size() > 2){
            *key = std::string(match[1].str());
            
            std::string vers = match[2].str();

            std::vector<std::string> cli;
            boost::split(cli, vers, boost::is_any_of(","));

            if(cli.size() <= 0){
                 version = nullptr;
                 return 0;
            }

            for(auto x: cli){
                std::vector<std::string> cli_id_clock;
                boost::split(cli_id_clock, x, boost::is_any_of("@"));

                version->insert(std::make_pair(std::stol(cli_id_clock[0]), std::stol(cli_id_clock[1])));
            }

        }
    }catch(std::invalid_argument e){
        return -1;
    }
    return 0;
}


int split_composite_key(std::string comp_key, std::string* key){
    boost::cmatch match;
    auto res = boost::regex_search(comp_key.c_str(), match, composite_key);

    try{
        if(match.size() > 2){
            *key = std::string(match[1].str());
        }
    }catch(std::invalid_argument e){
        return -1;
    }
    return 0;
}



// Version Vector to string
std::string vv2str(std::map<long, long> vv){
    std::string mapStr;
    for(auto& x: vv){
        mapStr.append(std::to_string(x.first)).append("@").append(std::to_string(x.second)).append(",");
    }
    if(mapStr.size() > 0)
        mapStr.pop_back(); // Retirar a última ","

    return mapStr; 
}

std::string compose_key_toString(std::string key, kv_store_key_version version){
    std::string toStr;
    std::string mapStr = vv2str(version.vv);
    toStr.append(key).append("#").append(mapStr);

    return toStr; 
}


std::map<long, long>* str2vv(std::string vv_str){
    std::vector<std::string> peers;
    boost::split(peers, vv_str, boost::is_any_of(","));

    std::map<long, long>* vv;
    if(peers.size() <= 0) vv = nullptr;

    for(auto x: peers){
        std::vector<std::string> peer_id_clock;
        boost::split(peer_id_clock, x, boost::is_any_of("@"));
        
        vv->insert(std::make_pair(std::stol(peer_id_clock[0]), std::stol(peer_id_clock[1])));
    } 
    return vv;
}
