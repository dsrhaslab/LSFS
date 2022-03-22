//
// Created by danielsf97 on 3/22/20.
//

#ifndef P2PFS_KV_STORE_KEY_VERSION_H
#define P2PFS_KV_STORE_KEY_VERSION_H


struct kv_store_key_version {

    std::map<long, long> vv; //version vector

    explicit kv_store_key_version(std::map<long, long>  vv): vv(vv){}

    kv_store_key_version(){
        this->vv = std::map<long, long> ();
    }

    kv_store_key_version(const kv_store_key_version& other){
        this->vv = other.vv;
    }

        //<x@1, y@2, z@1> < <x@2, y@2, z@1>
        //<x@1, y@2, z@1> > <x@0, y@2, z@1>
        //<y@2, z@1> < <x@0, y@2, z@1>
        //<y@3, z@1> =/= <x@0, y@2, z@1>

    inline bool operator==(const kv_store_key_version& other) const
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
        //Se chegou aqui todos se encontram no other, o único caso pode ser o other ser maior
        return  this->vv.size() == other.vv.size();
    }

    inline bool operator<(const kv_store_key_version& other) const
    {
        for(auto x: this->vv){
            auto it = other.vv.find(x.first);
            if(it == other.vv.end())
                return false;
            else if(x.second > it->second)
                return false;
        }
        return true;
    }

    inline bool operator>(const kv_store_key_version& other) const
    {
        for(auto x: other.vv){
            auto it = this->vv.find(x.first);
            if(it == this->vv.end())
                return false;
            else if(x.second > it->second)
                return false;
        }
        return true;
    }

    inline bool operator>=(const kv_store_key_version& other) const
    {
        return *this == other || *this > other;
    }

    inline bool operator<=(const kv_store_key_version& other) const
    {
        return *this == other || *this < other;
    }
};


#endif //P2PFS_KV_STORE_KEY_VERSION_H
