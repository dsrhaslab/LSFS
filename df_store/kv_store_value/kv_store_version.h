#ifndef P2PFS_KV_STORE_VERSION_H
#define P2PFS_KV_STORE_VERSION_H


#include <boost/serialization/map.hpp>


struct kv_store_version {

    std::map<long, long> vv; //version vector
    long client_id;

    explicit kv_store_version(std::map<long, long>  vv, long client_id): vv(vv), client_id(client_id){}

    kv_store_version(){
        this->vv = std::map<long, long> ();
        this->client_id = LONG_MAX;
    }

    kv_store_version(const kv_store_version& other){
        this->vv = other.vv;
        this->client_id = other.client_id;
    }

        //<x@1, y@2, z@1> < <x@2, y@2, z@1>
        //<x@1, y@2, z@1> > <x@0, y@2, z@1>
        //<y@2, z@1> < <x@0, y@2, z@1>
        //<y@3, z@1> =/= <x@0, y@2, z@1>

    inline bool operator==(const kv_store_version& other) const
    {
        for(auto x: this->vv){
            auto it = other.vv.find(x.first);
            //Se nao encontrou, nao sao iguais
            if(it == other.vv.end())
                return false;
            //Se ate encontrou, mas o clock não coincide 
            else if(x.second != it->second)
                return false;
        }
        //Se chegou aqui todos os que estao no this->vv encontram-se no other, o único caso pode ser o other ser maior
        return  this->vv.size() == other.vv.size() && this->client_id == other.client_id;
    }

    inline bool operator!=(const kv_store_version& other) const
    {
        return !(*this == other);
    }

private:
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar&this->vv;
        ar&this->client_id;
    }

};


#endif //P2PFS_KV_STORE_VERSION_H
