#pragma once

#include "pb_include.hpp"
#include "rpc_servrice.hpp"
#include "userlogin.pb.h"
#include "friend.pb.h"
#include "rpc_logger.hpp"
#include "rpc_zookeeper.hpp"

namespace rpc_client_pkg{
    // 重写protobuf::RpcChannel的callmethod纯虚函数，调用rpc方法本质为调用callmethod
    class RpcChannel : public google::protobuf::RpcChannel
    {
        using ClientZK = ZK_Opreator<ClientRpcLogger>;
    public:
        void CallMethod(const google::protobuf::MethodDescriptor *method, // RPC方法描述
                        google::protobuf::RpcController *controller,
                        const google::protobuf::Message *request, // RPC方法所需要的参数
                        google::protobuf::Message *response,      // 返回的响应结果
                        google::protobuf::Closure *done)          // 回调
        {
            string serialize;       //存储序列化请求消息：headersize + (service_name + method_name + args_size) + (args)
            uint32_t args_size = 0; //序列化参数长度
            string args;            //序列化参数
            string service_name;
            string method_name;
            //序列化为二进制字符串
            if (controller->IsCanceled() || !request->SerializeToString(&args))
            {
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod - serialize args error!");
                controller->SetFailed(" rpc_client_pkg::CallMethod - serialize args error!");
                return;
            }
            args_size = args.size();
            service_name = method->service()->name();
            method_name = method->name();
            //组装请求头（服务名+方法名+参数长度）
            loginrpc::MessageHeader header;
            header.set_service_name(service_name);
            header.set_method_name(method_name);
            header.set_args_size(args_size);
            string header_str;
            uint32_t header_size;
            //序列化请求头
            if (controller->IsCanceled() || !header.SerializeToString(&header_str))
            {
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod - serialize header error!");
                controller->SetFailed(" rpc_client_pkg::CallMethod - serialize header error!");
                return;
            }
            header_size = header_str.size();
            // 约定发送给callee的请求消息均为二进制字符串，且请求头大小固定为4字节
            serialize.resize(ConfPraseINT(HEADERPKG_BYTE_SIZE));
            memcpy(&serialize[0], (char*)&header_size, ConfPraseINT(HEADERPKG_BYTE_SIZE));
            //serialize.insert(0,string((char*)&header_size),0,4);
            serialize = serialize + header_str + args;
            //LOG_INFO<<serialize<<" : "<<serialize.size();
            // 发送请求置Rpc提供端
            // 建立连接
            caller_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (controller->IsCanceled() || caller_fd == -1)
            {
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod - create caller socket error!");
                controller->SetFailed(" rpc_client_pkg::CallMethod - create caller socket error!");
                return;
            }

            string znode_path;
            string server_ip,server_port;
            //从zk上获取服务所在服务器的地址
            ClientZK::GetInstance()->Connect2Server();
            znode_path = "/"+service_name+"/"+method_name;
            //LOG_INFO<<znode_path;
            string res = ClientZK::GetInstance()->GetZNodeData(znode_path);
            if(controller->IsCanceled() || res.empty()){
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod - "+ znode_path + "is not exists!");
                controller->SetFailed(znode_path + " rpc_client_pkg::CallMethod - is not exists!");
                close(caller_fd);
                return;
            }
            size_t pos = res.find(':',0);
            if(pos == res.npos){
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR,znode_path + " rpc_client_pkg::CallMethod - address format error!");
                controller->SetFailed(znode_path + " rpc_client_pkg::CallMethod - address format error!");
                close(caller_fd);
                return;
            }
            server_ip = res.substr(0,pos);
            server_port = res.substr(pos+1,res.size());
            
            sockaddr_in addr = {0};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
            addr.sin_port = htons(atoi(server_port.c_str()));
            if (controller->IsCanceled() || connect(caller_fd, (sockaddr *)&addr, sizeof(addr)) == -1)
            {
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod -  callee failed!");
                controller->SetFailed(" rpc_client_pkg::CallMethod - connect callee failed!");
                close(caller_fd);
                return;
            }
            // 发送RPC请求报文
            if (controller->IsCanceled() || send(caller_fd, serialize.c_str(), serialize.size(), 0) < serialize.size())
            {
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod - send RpcPackge failed : "+serialize);
                controller->SetFailed(" rpc_client_pkg::CallMethod - send RpcPackge failed!");
                close(caller_fd);
                return;
            }
            // 等待响应
            string result(ConfPraseINT(BUFFERLEN), '\0');
            int result_size = 0;
            result_size = recv(caller_fd, &result[0], result.size(), 0);
            if (controller->IsCanceled() || result_size == -1)
            {
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod - recv response error!");
                controller->SetFailed(" rpc_client_pkg::CallMethod - recv response error!");
                close(caller_fd);
                return;
            }
            close(caller_fd);
            result = result.substr(0, result_size);
            //LOG_INFO << result.c_str();
            // 反序列化响应结果
            if (controller->IsCanceled() || !response->ParseFromString(result))
            {
                CLIENT_LOG_OUT(LOG_LEVEL_ERROR," rpc_client_pkg::CallMethod - prase reponse result error: "+result);
                controller->SetFailed(" rpc_client_pkg::CallMethod - prase reponse result error!");
                return;
            }
        }

    private:
        int caller_fd; // 调用端套接字
    };

    // 提供给caller的接口
    class RpcMethod
    {
    public:
        static shared_ptr<loginrpc::UserLoginRpc_Stub> UserLoginStub;
        static shared_ptr<friendrpc::FriendMethodRpc_Stub> FriendStub;
        static shared_ptr<RpcMethod> GetInstance()
        {
            if (rpcmethod == nullptr)
            {
                lock_guard<mutex> guard(m);
                if (rpcmethod == nullptr)
                    rpcmethod = shared_ptr<RpcMethod>(new RpcMethod());
            }
            return rpcmethod;
        }
    private:
        RpcMethod()
        {
            UserLoginStub = make_shared<loginrpc::UserLoginRpc_Stub>(new RpcChannel());
            FriendStub = make_shared<friendrpc::FriendMethodRpc_Stub>(new RpcChannel());
        }
        
        static mutex m;
        static shared_ptr<RpcMethod> rpcmethod;
    };
    shared_ptr<loginrpc::UserLoginRpc_Stub> RpcMethod::UserLoginStub = nullptr;
    shared_ptr<friendrpc::FriendMethodRpc_Stub> RpcMethod::FriendStub = nullptr;
    mutex RpcMethod::m;
    shared_ptr<RpcMethod> RpcMethod::rpcmethod = nullptr;
};
