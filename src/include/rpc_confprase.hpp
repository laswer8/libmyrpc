#ifndef RPC_CONFIGURE
#define RPC_CONFIGURE

#include "pb_include.hpp"

//解析json格式的配置文件
class RpcConfPrase{
public:
    //解析/加载配置文件
    RpcConfPrase(const string& confile = CONFILEPATH){
        ifstream file(confile);
        if(!file){
            LOG_INFO<<confile<<" is not exists";
            exit(EXIT_FAILURE);
        }
        json tmp_j = json::parse(file);
        conf = tmp_j;
        file.close();
    }
    //获取配置
    string Prase(const string& key){
        if(conf.count(key) != 0)
            return conf[key];
        else{
            LOG_INFO<<"Prase "<<key<<" error";
            return string();
        }
            
    }
    //获取配置
     int PraseInt(const string& key){
        if(conf.count(key) != 0)
            return atoi(conf[key].c_str());
        else{
            LOG_INFO<<"Prase "<<key<<" error";
            return -1;
        }
            
    }
private:
    unordered_map<string,string> conf;
};


#endif // !RPC_CONFIGURE
