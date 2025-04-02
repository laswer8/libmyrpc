#pragma once

#include "pb_include.hpp"
#include "rpc_logger.hpp"

//提供zookeeper的基础操作，如建立连接，心跳保活，创建节点，获取节点信息
template<class T>
class ZK_Opreator{
//用于接收server返回的连接建立响应，用于判断是否连接成功
static void connect_watcher(zhandle_t *zh, int type, int state, const char *path,void *watcherCtx){
    if(state == ZOO_CONNECTED_STATE){//处理会话相关事件
        if(type == ZOO_SESSION_EVENT){
            //如果连接建立成功
            sem_t* sem = (sem_t*)zoo_get_context(zh);
            is_connected = true;
            sem_post(sem);
        }
    }
}
//用于接收server返回的连接建立响应，用于判断是否连接成功
static void a_connect_watcher(zhandle_t *zh, int type, int state, const char *path,void *watcherCtx){
    if(type == ZOO_SESSION_EVENT){//处理会话相关事件
        if(state == ZOO_CONNECTED_STATE){
            //如果连接建立成功
            is_connected = true;
            LoggerManager<T>::WriteLog(LOG_LEVEL_SUCCESS," ZK_Opreator<T>::a_connect_watcher - zookeeper session connected success");
        }else{
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR," ZK_Opreator<T>::a_connect_watcher - zookeeper session connected unknowed error");
        }
    }
}

static void create_completion(int rc, const char *value, const void *data){
    pair<int*,sem_t*>* args = (pair<int*,sem_t*>*)data;
    *(args->first) = rc;
    sem_post(args->second);
}

static void a_create_completion(int rc, const char *value, const void *data){
    string* path = (string*)data;
    string s;
    switch (rc)
    {
    case ZNONODE:                   // 该节点的父节点不存在
        s = " ZK_Opreator<T>::a_create_completion - 该节点的父节点不存在: "+*path;
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    case ZOK:                       // 创建节点成功
        s = " ZK_Opreator<T>::a_create_completion - 创建节点成功: "+*path;
        LoggerManager<T>::WriteLog(LOG_LEVEL_SUCCESS,s);
        break;
    case ZNODEEXISTS:               // 该节点已存在
        s = " ZK_Opreator<T>::a_create_completion - 该节点已存在: "+*path;
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    case ZNOAUTH:                   // 权限被拒
        s = " ZK_Opreator<T>::a_create_completion - 权限被拒: "+*path;
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    case ZNOCHILDRENFOREPHEMERALS:  // 对临时节点创建子节点是非法操作
        s = " ZK_Opreator<T>::a_create_completion - 对临时节点创建子节点是非法操作: "+*path;
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    default:
        s = " ZK_Opreator<T>::a_create_completion - 未知操作: "+*path;
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    };
}

static void setdata_completion(int rc, const char *value, int value_len,const struct Stat *stat, const void *data){
    tuple<int*,sem_t*,string*>* args = (tuple<int*,sem_t*,string*>*)data;
    *(std::get<0>(*args)) = rc;
    if(std::get<2>(*args) != nullptr && value != nullptr)
        *(std::get<2>(*args)) = value;
    sem_post(std::get<1>(*args));
}

static void a_modifydata_completion(int rc, const struct Stat *stat, const void *data){
    pair<string,string>* args = (pair<string,string>*)data;
    string s;
    switch (rc)
    {
    case ZOK:
        s = " ZK_Opreator<T>::a_modifydata_completion - 更新节点成功 " + args->first + " " + args->second + " version-" + to_string(version_id);
        LoggerManager<T>::WriteLog(LOG_LEVEL_SUCCESS,s);
        version_id++;
        break;
    case ZNONODE:
        s = " ZK_Opreator<T>::a_modifydata_completion - 该节点不存在 " + args->first + " " + args->second + " version-" + to_string(version_id);
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    case ZNOAUTH:
        s = " ZK_Opreator<T>::a_modifydata_completion - 客户端权限被拒 " + args->first + " " + args->second + " version-" + to_string(version_id);
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    case ZBADVERSION:
        s = " ZK_Opreator<T>::a_modifydata_completion - 预期版本不匹配 " + args->first + " " + args->second + " version-" + to_string(version_id);
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    default:
        s = " ZK_Opreator<T>::a_modifydata_completion - 未知错误 " + args->first + " " + args->second + " version-" + to_string(version_id);
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
        break;
    }
}

static void getznodechildren_completion(int rc,const struct String_vector *strings, const void *data){
    tuple<int*,vector<string>*,sem_t*>* args = (tuple<int*,vector<string>*,sem_t*>*)data;
    *(std::get<0>(*args)) = rc;
    vector<string> vec;
    for(int i = 0;i < strings->count;i++){
        vec.emplace_back(strings->data[i]);
    }
    *(std::get<1>(*args)) = vec;
    sem_post(std::get<2>(*args));
    //strings须交由程序员释放
    deallocate_String_vector(strings);
}

public:

    static shared_ptr<ZK_Opreator<T>> GetInstance(){
        if(!zk_operator){
            lock_guard<mutex> guard(m);
            if(!zk_operator){
                zk_operator = shared_ptr<ZK_Opreator<T>>(new ZK_Opreator<T>());
            }
        }
        return zk_operator;
    }

    ~ZK_Opreator(){
        is_connected = false;
        if(zk_client)
            zookeeper_close(zk_client);
        
    }

    bool get_connected(){
        return is_connected;
    }
    //同步连接zookeeper服务端
    bool Connect2Server(){
        if(is_connected)
            return false;
        string zk_addr = ConfPrase(STR_ZOOKEEPER_IP) + ":" + ConfPrase(STR_ZOOKEEPER_PORT);
        //发起连接，其返回值不代表连接成功，只是说明发起连接与初始化成功，需要watcher接收服务端的响应才能判断是否成功
        zk_client = zookeeper_init(zk_addr.c_str(),connect_watcher,30000,nullptr,nullptr,0);
        if(!zk_client){
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR," ZK_Opreator<T>::Connect2Server - init zookeeper session connect fd error: "+zk_addr);
            return false;
        }
        //阻塞等待服务器响应
        sem_t sem;
        sem_init(&sem,0,0);
        zoo_set_context(zk_client,&sem);
        sem_wait(&sem);
        if(is_connected){
            is_connected = true;
            LoggerManager<T>::WriteLog(LOG_LEVEL_SUCCESS," ZK_Opreator<T>::Connect2Server - zookeeper session connected success!");
            return true;
        }
        LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR," ZK_Opreator<T>::Connect2Server - zookeeper session connected failed!");
        return false;
    }
    //异步连接zookeeper服务端
    bool A_Connect2Server(){
        if(is_connected)
            return false;
        string zk_addr = ConfPrase(STR_ZOOKEEPER_IP) + ":" + ConfPrase(STR_ZOOKEEPER_PORT);
        //发起连接，其返回值不代表连接成功，只是说明发起连接与初始化成功，需要watcher接收服务端的响应才能判断是否成功
        zk_client = zookeeper_init(zk_addr.c_str(),ZK_Opreator<T>::a_connect_watcher,30000,nullptr,nullptr,0);
        //发起连接失败
        if(!zk_client){
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR," ZK_Opreator<T>::A_Connect2Server - init zookeeper session connect fd error!");
            return false;
        }
        return true;
    }

    //同步创建存储data数据的znode节点
    bool create_znode(const string& path,const string& data,const int& length,const int& state){
        if(!is_connected)return false;
        //无需检查是否存在节点，server返回的响应包括了是否已存在节点
        sem_t sem;
        sem_init(&sem,0,0);
        string s;
        //保存结果信息
        int rc = -1;
        pair<int*,sem_t*> args = make_pair<int*,sem_t*>(&rc,&sem);
        int res = zoo_acreate(zk_client,path.c_str(),data.c_str(),data.length(),&ZOO_OPEN_ACL_UNSAFE,state,ZK_Opreator<T>::create_completion,&args);
        if(res != ZOK){
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR," ZK_Opreator<T>::create_znode - Create zookeeper ZNode Application error!");
            return false;
        }
        sem_wait(&sem);
        //判断结果
        switch (rc)
        {
        case ZNONODE:                   // 该节点的父节点不存在
            s = " ZK_Opreator<T>::create_znode - 该节点的父节点不存在: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZOK:                       // 创建节点成功
            s = " ZK_Opreator<T>::create_znode - 创建节点成功: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_SUCCESS,s);
            return true;
        case ZNODEEXISTS:               // 该节点已存在
            s = " ZK_Opreator<T>::create_znode - 该节点已存在: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZNOAUTH:                   // 权限被拒
            s = " ZK_Opreator<T>::create_znode - 权限被拒: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZNOCHILDRENFOREPHEMERALS:  // 对临时节点创建子节点是非法操作
            s = " ZK_Opreator<T>::create_znode - 对临时节点创建子节点是非法操作: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        default:
            s = " ZK_Opreator<T>::create_znode - 未知操作: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        };
        return false;
    }
    //异步创建存储data数据的znode节点
    bool a_create_znode(const string& path,const string& data,const int& length,const int& state){
        if(!is_connected)return false;
        int res = zoo_acreate(zk_client,path.c_str(),data.c_str(),data.length(),&ZOO_OPEN_ACL_UNSAFE,state,ZK_Opreator<T>::a_create_completion,&path);
        if(res != ZOK){
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR," ZK_Opreator<T>::a_create_znode - Create zookeeper ZNode Application error!");
            return false;
        }
        return true;
    }

    //同步获取znode节点存储的信息
    string GetZNodeData(const string& path){
        LOG_INFO<<"GetZNodeData1";
        string str = "";
        if(!is_connected)return str;
        sem_t sem;
        sem_init(&sem,0,0);
        string s;
        int rc = -1;
        tuple<int*,sem_t*,string*> args = make_tuple<int*,sem_t*,string*>(&rc,&sem,&str);
        int res = zoo_aget(zk_client,path.c_str(),0,setdata_completion,&args);
        if(res != ZOK){
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR," ZK_Opreator<T>::GetZNodeData - Get zookeeper ZNode Data error!");
            return str;
        }
        sem_wait(&sem);
        switch (rc)
        {
        case ZNONODE:                   // 该节点的父节点不存在
            s = " ZK_Opreator<T>::GetZNodeData - 该节点不存在: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZOK:                       // 创建节点成功
            s = " ZK_Opreator<T>::GetZNodeData - 获取成功: "+path+" value: "+str;
            LoggerManager<T>::WriteLog(LOG_LEVEL_SUCCESS,s);
            break;
        case ZNOAUTH:                   // 权限被拒
            s = " ZK_Opreator<T>::GetZNodeData - 权限被拒: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        default:
            s = " ZK_Opreator<T>::GetZNodeData - 未知操作: "+path;
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        };
        return str;
    }

    //同步更新znode节点数据
    bool ModifyZNodeData(const string& path,const string& data){
        if(!is_connected)return false;
        string s;
        //修改节点数据，version为-1表示不启用版本控制，completion为空表示同步执行
        int res = zoo_aset(zk_client,path.c_str(),data.c_str(),data.size(),version_id,nullptr,nullptr);
        switch (res)
        {
        case ZOK:
            version_id++;
            s = " ZK_Opreator<T>::ModifyZNodeData - 更新节点成功 " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_SUCCESS,s);
            return true;
        case ZBADARGUMENTS:
            s = " ZK_Opreator<T>::ModifyZNodeData - 参数错误 " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZINVALIDSTATE:
            s = " ZK_Opreator<T>::ModifyZNodeData - zhandle State 为 ZOO_SESSION_EXPIRED_STATE 或 ZOO_AUTH_FAILED_STATE " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZMARSHALLINGERROR:
            s = " ZK_Opreator<T>::ModifyZNodeData - 可能因内存不足而导致请求失败 " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZNONODE:
            s = " ZK_Opreator<T>::ModifyZNodeData - 该节点不存在 " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZNOAUTH:
            s = " ZK_Opreator<T>::ModifyZNodeData - 客户端权限被拒 " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZBADVERSION:
            s = " ZK_Opreator<T>::ModifyZNodeData - 预期版本不匹配 " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        default:
            s = " ZK_Opreator<T>::ModifyZNodeData - 未知错误 " + path + " " + data + " version-" + to_string(version_id);
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        }
        return false;
    }

    //异步更新znode节点的数据
    bool A_ModifyZNodeData(const string& path,const string& data){
        if(!is_connected)return false;
        pair<string,string> args(path,data);
        string s;
        int res = zoo_aset(zk_client,path.c_str(),data.c_str(),data.size(),version_id,ZK_Opreator<T>::a_modifydata_completion,&args);
        if(res != ZOK){
            switch (res)
            {
            case ZBADARGUMENTS:
                s = " ZK_Opreator<T>::A_ModifyZNodeData - 参数错误 " + path + " " + data + " version-" + to_string(version_id);
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            case ZINVALIDSTATE:
                s = " ZK_Opreator<T>::A_ModifyZNodeData - zhandle State 为 ZOO_SESSION_EXPIRED_STATE 或 ZOO_AUTH_FAILED_STATE " + path + " " + data + " version-" + to_string(version_id);
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            case ZMARSHALLINGERROR:
                s = " ZK_Opreator<T>::A_ModifyZNodeData - 可能因内存不足而导致请求失败 " + path + " " + data + " version-" + to_string(version_id);
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            default:
                s = " ZK_Opreator<T>::A_ModifyZNodeData - 未知错误 " + path + " " + data + " version-" + to_string(version_id);
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            }
            return false;
        }

        return true;
    }

    //获取znode下的子节点列表
    vector<string> GetZNodeChildrenList(const string& path){
        vector<string> vec;
        if(!is_connected)return vec;
        sem_t sem;
        sem_init(&sem,0,0);
        string s;
        int rc = -1;    //ZOK == 0
        tuple<int*,vector<string>*,sem_t*> args = make_tuple<int*,vector<string>*,sem_t*>(&rc,&vec,&sem);
        int res = zoo_aget_children(zk_client,path.c_str(),0,ZK_Opreator<T>::getznodechildren_completion,&args);
        if(res != ZOK){
            switch (res)
            {
            case ZBADARGUMENTS:
                s = " ZK_Opreator<T>::GetZNodeChildrenList - 参数错误 " + path;
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            case ZINVALIDSTATE:
                s = " ZK_Opreator<T>::GetZNodeChildrenList - zhandle State 为 ZOO_SESSION_EXPIRED_STATE 或 ZOO_AUTH_FAILED_STATE " + path;
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            case ZMARSHALLINGERROR:
                s = " ZK_Opreator<T>::GetZNodeChildrenList - 可能因内存不足而导致请求失败 " + path;
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            default:
                s = " ZK_Opreator<T>::GetZNodeChildrenList - 未知错误 " + path;
                LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
                break;
            }
            return vec;
        }
        sem_wait(&sem);
        switch (rc)
        {
        case ZCONNECTIONLOSS:
            s = " ZK_Opreator<T>::GetZNodeChildrenList - 连接丢失 " + path+" "+to_string(vec.size());
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZOPERATIONTIMEOUT:
            s = " ZK_Opreator<T>::GetZNodeChildrenList - 连接超时 " + path+" "+to_string(vec.size());
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZOK:
            s = " ZK_Opreator<T>::GetZNodeChildrenList - 获取子节点列表成功 " + path+" "+to_string(vec.size());
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZNONODE:
            s = " ZK_Opreator<T>::GetZNodeChildrenList - 该节点不存在 " + path+" "+to_string(vec.size());
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        case ZNOAUTH:
            s = " ZK_Opreator<T>::GetZNodeChildrenList - 客户端权限被拒 " + path+" "+to_string(vec.size());
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        default:
            s = " ZK_Opreator<T>::GetZNodeChildrenList - 未知错误 " + path+" "+to_string(vec.size());
            LoggerManager<T>::WriteLog(LOG_LEVEL_ERROR,s);
            break;
        }
        return vec;
    }

private:
    ZK_Opreator():zk_client(nullptr){}
    ZK_Opreator(const ZK_Opreator&) = delete;
    ZK_Opreator(ZK_Opreator&&) = delete;
    ZK_Opreator& operator=(const ZK_Opreator&) = delete;

    //zk客户端连接
    zhandle_t* zk_client;       //C API RAII
    static bool is_connected;
    static size_t version_id;          //用于版本控制
    static shared_ptr<ZK_Opreator<T>> zk_operator;
    static mutex m;
};
template<class T> shared_ptr<ZK_Opreator<T>> ZK_Opreator<T>::zk_operator = nullptr;
template<class T> mutex ZK_Opreator<T>::m;
template<class T> bool ZK_Opreator<T>::is_connected = false;
template<class T> size_t ZK_Opreator<T>::version_id = 1; 