#include "util.h"


std::string get_local_ip_address(){
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    sockaddr_in loopback;

    if (sock == -1) {
        throw "ERROR CREATING SOCKET";
    }

    std::memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = INADDR_LOOPBACK;   // using loopback ip address
    loopback.sin_port = htons(9);                 // using debug port

    if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        throw "ERROR COULD NOT CONNECT";
    }

    socklen_t addrlen = sizeof(loopback);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
        close(sock);
        throw "ERROR COULD NOT GETSOCKNAME";
    }

    close(sock);

    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == 0x0) {
        throw "ERROR COULD NOT INET_NTOP";
    } else {
        return std::string(buf);
    }
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



std::string vv2str(std::map<long, long> vv){
    std::string mapStr;
    for(auto& x: vv){
        mapStr.append(std::to_string(x.first)).append("@").append(std::to_string(x.second)).append(",");
    }
    if(mapStr.size() > 0)
        mapStr.pop_back(); // Retirar a última ","

    return mapStr; 
}

std::string compose_key_toString(std::string key, kv_store_version version){
    std::string toStr;
    std::string mapStr = vv2str(version.vv);
    toStr.append(key).append("#").append(mapStr).append("#").append(std::to_string(version.client_id));

    return toStr; 
}



void print_kv(const kv_store_key<std::string>& kv){
    std::cout << "##### Key: " << kv.key << " Vector: <";

    for(auto pair: kv.version.vv)
        std::cout << "(" <<  pair.first << "@" << pair.second << "),";
    
    std::cout << ">" << " Client: " << kv.version.client_id << std::endl;
}


void print_kv(const kv_store_version& kv){
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
kVersionComp comp_version(const kv_store_version& k1, const kv_store_version& k2){
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

kv_store_version merge_kv(const kv_store_version& k1, const kv_store_version& k2){
    kv_store_version res = k1;

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

kv_store_version add_vv(std::pair<long, long> version, const kv_store_version& k2){
    kv_store_version k1;
    k1.vv.insert(version);
    return merge_kv(k1, k2);
}



//Considera-se que o id do cliente é único
kv_store_version choose_latest_version(std::vector<kv_store_version>& kvv_v){

    kv_store_version choosed_v;

    for(auto &kvv: kvv_v){
        if(kvv.client_id < choosed_v.client_id)
            choosed_v = kvv;
    }
    return choosed_v;
}



std::unique_ptr<std::string> split_data(std::unique_ptr<std::string> data, int blk_num){
    if(data != nullptr){
        size_t NR_BLKS = (data->size() / BLK_SIZE);
        if(data->size() % BLK_SIZE > 0) NR_BLKS = NR_BLKS + 1;

        size_t pos = (blk_num - 1)*BLK_SIZE;

        std::string value;

        //Last block
        if(blk_num == NR_BLKS){
            value = data->substr(pos);  
        }else{
            value = data->substr(pos, BLK_SIZE); 
        }

        return std::make_unique<std::string>(value);
    }

    return nullptr;
}
