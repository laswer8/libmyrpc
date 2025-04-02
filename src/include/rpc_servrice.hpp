#pragma once

#include "pb_include.hpp"
#include "rpc_confprase.hpp"

#define ConfPrase(KEY) (RpcService::GetInstance()->GetConf().Prase(KEY))
#define ConfPraseINT(KEY) (RpcService::GetInstance()->GetConf().PraseInt(KEY))
//框架基础类
class RpcService{
    public:
        RpcService(const RpcService&) = delete;
        RpcService& operator=(const RpcService&) = delete;
        ~RpcService(){}

        static shared_ptr<RpcService>& GetInstance(){
            if(!ptr){
                lock_guard<mutex> guard(m);
                if(!ptr){
                    ptr = shared_ptr<RpcService>(new RpcService());
                }
            }
            return ptr;
        }
        
        static RpcConfPrase& GetConf(){
            return m_conf;
        }

        //初始化框架(读取配置文件)
        void Init(int argc,char** args){
            if(isInit) return;
            //检查参数
            if(argc<2){
                LOG_INFO<<"format: command -i <configfile.conf>";
                exit(EXIT_FAILURE);
            }
            //getopt解析参数，“i:”表示匹配-i选项且-i选项有参数，返回匹配到的字符如i，匹配到未知字符返回？，匹配到-i但没有需要的参数返回:，解析结束返回-1
            int check_c = 0;
            string confile;
            while ((check_c = getopt(argc, args, "i:")) != -1)
            {
                switch (check_c)
                {
                case 'i':
                    confile = optarg; // 保存了当前选项的参数
                    break;
                default:
                    //其他选项暂时均为非法参数，无法支持运行
                    LOG_INFO << "format: command -i <configfile.conf>";
                    exit(EXIT_FAILURE);
                }
            }

            //覆盖默认配置文件
            if(confile != CONFILEPATH)
                m_conf = RpcConfPrase(confile);
            isInit = true;
        }

    private:
        RpcService(){}

        static RpcConfPrase m_conf;         //解析配置文件
        static shared_ptr<RpcService> ptr;
        static mutex m;
        static bool isInit;                 //保证只能初始化一次
};

shared_ptr<RpcService> RpcService::ptr = nullptr;
mutex RpcService::m;
bool RpcService::isInit = false;
RpcConfPrase RpcService::m_conf;