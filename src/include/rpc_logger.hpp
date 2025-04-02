#pragma once

#include "rpc_messagelist.hpp"

#define LOG_OUT(level,msg) (LoggerManager<ProducerRpcLogger>::WriteLog(level,msg))
#define CLIENT_LOG_OUT(level,msg) (LoggerManager<ClientRpcLogger>::WriteLog(level,msg))

class ProducerRpcLogger{
public:
    static shared_ptr<ProducerRpcLogger> GetInstance(){
        if(!logger){
            lock_guard<mutex> guard(m);
            if(!logger){
                logger = shared_ptr<ProducerRpcLogger>(new ProducerRpcLogger());
            }
        }
        return logger;
    }

    ~ProducerRpcLogger(){
        producer.End();
        consumer.stop();
    }

    void WriteLog(const string& level,const string& msg){
        lock_guard<mutex> guard(m);
        if(!is_init)
            init();
        string s = msg + " " +ConfPrase(STR_SERVER_IP)+":"+ConfPrase(STR_SERVER_PORT);
        producer.CommitMessage(level,s);
    }

private:
    //消息处理函数
    void msgprocess(void* arg){
        string msg = (char*)arg;
        //获取消息地址，判断是否是自身写入
        size_t pos = msg.rfind(" ");
        if(pos == msg.npos){
            LOG_INFO<<" ProducerRpcLogger::msgprocess - address format error, writing failed!";
            return;  
        }
        string addr = msg.substr(pos+1);
        if(addr == (ConfPrase(STR_SERVER_IP)+":"+ConfPrase(STR_SERVER_PORT))){
            //成功获取消息,Level使用消息自带
            ProcessMessage::AppendMessage(string(),msg);
        }
    }
    void init(){
        if(is_init)
            return;
        is_init = true;
        producer.Init();
        consumer.Init();
        consumer.setMsgCallBack(bind(&ProducerRpcLogger::msgprocess,this,_1));
        vector<string> topics;
        topics.push_back(ConfPrase(KAFKA_LOG_TOPIC_NAME));
        consumer.start_recv(topics);
    }
    ProducerRpcLogger():is_init(false){}
    ProducerRpcLogger(const ProducerRpcLogger&) = delete;
    ProducerRpcLogger(ProducerRpcLogger&&) = delete;
    ProducerRpcLogger& operator=(const ProducerRpcLogger&) = delete;

    static mutex m;
    static shared_ptr<ProducerRpcLogger> logger;
    bool is_init;
    RpcMessageProducer producer;
    RpcMessageConsumer consumer;
};
shared_ptr<ProducerRpcLogger> ProducerRpcLogger::logger = nullptr;
mutex ProducerRpcLogger::m;


class ClientRpcLogger{
public:
    ~ClientRpcLogger(){}
    static shared_ptr<ClientRpcLogger> GetInstance(){
        if(!c_logger){
            lock_guard<mutex> guard(c_m);
            if(!c_logger){
                c_logger = shared_ptr<ClientRpcLogger>(new ClientRpcLogger());
            }
        }
        return c_logger;
    }
    void WriteLog(const string& level,const string& msg){
        lock_guard<mutex> guard(c_m);
        ClientProcessMessage::AppendMessage(level,msg);
    }
private:
    ClientRpcLogger(){}
    ClientRpcLogger(const ClientRpcLogger&) = delete;
    ClientRpcLogger(ClientRpcLogger&&) = delete;
    ClientRpcLogger& operator=(const ClientRpcLogger&) = delete;

    static mutex c_m;
    static shared_ptr<ClientRpcLogger> c_logger;
};
shared_ptr<ClientRpcLogger> ClientRpcLogger::c_logger = nullptr;
mutex ClientRpcLogger::c_m;

template<class T>
class LoggerManager{
public:
    static void WriteLog(const string& level,const string& msg){LOG_INFO<<"无效推导";}
};
template<>
class LoggerManager<ProducerRpcLogger>{
public:
    static void WriteLog(const string& level,const string& msg){
        ProducerRpcLogger::GetInstance()->WriteLog(level,msg);
    }
};
template<>
class LoggerManager<ClientRpcLogger>{
public:
    static void WriteLog(const string& level,const string& msg){
        ClientRpcLogger::GetInstance()->WriteLog(level,msg);
    }
};