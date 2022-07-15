//
// Created by danielsf97 on 3/4/20.
//

#include "util.h"

int split_composite_total(std::string comp_key, std::string* key, std::map<long, long>* version, long* client_id){
    boost::cmatch match;
    auto res = boost::regex_search(comp_key.c_str(), match, composite_key);

    try{
        if(match.size() > 3){
            *key = std::string(match[1].str());
            
            std::string vers = match[2].str();

            *client_id = stol(match[3].str());

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



int get_base_path(const std::string& key, std::string* base_path){
    boost::cmatch match;
    auto res = boost::regex_search(key.c_str(), match, base_path_pattern);

    try{
        if(!match.empty()){
            *base_path = std::string(match[1].str());
        }
    }catch(std::invalid_argument e){
        return -1;
    }
    return 0;

}

int get_blk_num(const std::string& key, std::string* blk_num){
    boost::cmatch match;
    auto res = boost::regex_search(key.c_str(), match, blk_num_pattern);

    try{
        if(!match.empty()){
            *blk_num = std::string(match[1].str());
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
    toStr.append(key).append("#").append(mapStr).append("#").append(std::to_string(version.client_id));

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

void print_kv(const kv_store_key<std::string>& kv){
    std::cout << "##### Key: " << kv.key << " Vector: <";

    for(auto pair: kv.key_version.vv)
        std::cout << "(" <<  pair.first << "@" << pair.second << "),";
    
    std::cout << ">" << " Client: " << kv.key_version.client_id << std::endl;
}


void print_kv(const kv_store_key_version& kv){
    std::cout << " Vector: <";

    for(auto pair: kv.vv)
        std::cout << "(" <<  pair.first << "@" << pair.second << "),";
    
    std::cout << ">" << std::endl;
}

        //<x@1, y@2, z@1> < <x@2, y@2, z@1>
        //<x@1, y@2, z@1> > <x@0, y@2, z@1>
        //<y@2, z@1> < <x@0, y@2, z@1>
        //<y@3, z@1> =/= <x@0, y@2, z@1>
         //<y@3, z@1, u@3> =/= <x@0, y@2, z@1>
         //<y@3, z@1, u@3, x@0> =/= <x@1, y@2, z@1>
         //<y@3, z@1, u@3, x@2> > <x@1, y@2, z@1>

//k1 compared to k2
kVersionComp comp_version(const kv_store_key_version& k1, const kv_store_key_version k2){
    bool is_equal = true;
    bool is_lower = true;
    bool is_bigger = true;
    int is_bigger_count = 0;
    bool is_concurrent = true;
    
    for(auto & x: k1.vv){
        auto it = k2.vv.find(x.first);
        //Se nao encontrou, nunca podera ser igual ou menor
        if(it == k2.vv.end()){
            is_equal = false;
            is_lower = false;
        }
        //Se ate encontrou, mas o clock não coincide 
        else{
            if(x.second != it->second)
                is_equal = false;
            
            if(x.second > it->second)
                is_lower = false;
            
            if(x.second < it->second)
                is_bigger = false;

            if(x.second >= it->second)
                is_bigger_count++; 
        }
            
    }

    if(is_equal && k1.vv.size() == k2.vv.size())
        return kVersionComp::Equal;
    if(is_lower && k1.vv.size() <= k2.vv.size())
        return kVersionComp::Lower;
    if(is_bigger && is_bigger_count == k2.vv.size() && k1.vv.size() >= k2.vv.size())
        return kVersionComp::Bigger;

    return kVersionComp::Concurrent;
}


kv_store_key_version merge_vkv(const std::vector<kv_store_key_version>& vkv){
    kv_store_key_version res;

    for(auto &kv: vkv){
        for(auto p: kv.vv){
            auto it = res.vv.find(p.first);
            if(it == res.vv.end())
                res.vv.insert(std::make_pair(p.first, p.second));
            else {
                if(it->second < p.second)
                    it->second = p.second;
            }   
        }
        if(kv.client_id < res.client_id)
            res.client_id = kv.client_id;
    }
    return res;
}

kv_store_key_version merge_kv(const kv_store_key_version& k1, const kv_store_key_version& k2){
    kv_store_key_version res = k1;

    for(auto p: k2.vv){
        auto it = res.vv.find(p.first);
        if(it == res.vv.end())
            res.vv.insert(std::make_pair(p.first, p.second));
        else {
            if(it->second < p.second)
                it->second = p.second;
        } 
    }
    if(k2.client_id < res.client_id)
        res.client_id = k2.client_id;

    return res;
}

kv_store_key_version add_vv(std::pair<long, long> version, const kv_store_key_version& k2){
    kv_store_key_version k1;
    k1.vv.insert(version);
    return merge_kv(k1, k2);
}


int sum_vv_clock(kv_store_key_version& v){
    int sum = 0;
    for(auto &cc: v.vv){
        sum += cc.second;
    }
    return sum;
}

//Considera-se que o id do cliente é único
kv_store_key_version choose_latest_version(std::vector<kv_store_key_version>& kvv_v){

    kv_store_key_version max_v;

    for(auto &kvv: kvv_v){
        if(kvv.client_id < max_v.client_id)
            max_v = kvv;
    }
    return max_v;
}


