#pragma once

#include "pb_include.hpp"
#include "rpc_servrice.hpp"
#include "userlogin.pb.h"
#include "friend.pb.h"
#include "rpc_logger.hpp"
#include "rpc_zookeeper.hpp"


namespace rpc_server_pkg
{
    // 框架网络服务类
    class RpcChannel
    {
    public:
        ~RpcChannel() {}
        RpcChannel(const RpcChannel &) = delete;
        RpcChannel &operator=(const RpcChannel &) = delete;

        static shared_ptr<RpcChannel> GetInstanse()
        {
            if (channel == nullptr)
            {
                lock_guard<mutex> guard(m);
                if (channel == nullptr)
                    channel = shared_ptr<RpcChannel>(new RpcChannel());
            }
            return channel;
        }

        // 注册RPC服务 - 所有发布RPC服务的对象，都继承了Service类以重载服务，都属于Service的派生类
        void RpcRegisted(google::protobuf::Service *service)
        {
            RpcObject obj;
            const google::protobuf::ServiceDescriptor *des = service->GetDescriptor();
            int count = des->method_count();
            shared_ptr<const google::protobuf::MethodDescriptor> method_ptr;
            for (int i = 0; i < count; i++)
            {
                method_ptr = shared_ptr<const google::protobuf::MethodDescriptor>(des->method(i));
                if (method_ptr)
                    obj.method_map[method_ptr->name()] = method_ptr;
            }
            obj.service_ptr = shared_ptr<google::protobuf::Service>(service);
            // 更新与对象名映射的rpc结构体
            RpcMap[des->name()] = obj;
        }

        // 发布RPC服务，使其开始工作 - 阻塞等待调用
        void Stared()
        {
            if (iStared)
                return;
            iStared = true;
            //在zk上注册服务，供客户端调用
            //如果使用nginx管理集群可以省略这一步，直接注册nginx地址即可

            //连接zk server
            ProducerZK::GetInstance()->Connect2Server();
            string path,subpath;
            string server_addr = ConfPrase(STR_SERVER_IP)+":"+ConfPrase(STR_SERVER_PORT);
            LOG_OUT(LOG_LEVEL_INFO," RpcChannel::Stared - "+server_addr);
            //注册znode节点
            for(auto& node:RpcMap){
                //service path 作为父节点
                path = "/"+node.first;
                ProducerZK::GetInstance()->create_znode(path.c_str(),"",0,ZOO_CONTAINER);
                //将该服务器上的rpc服务全部发布到zk上，并保存地址
                for(auto& servicenode:node.second.method_map){
                    subpath = path + "/" + servicenode.first;
                    ProducerZK::GetInstance()->create_znode(subpath.c_str(),server_addr.c_str(),server_addr.size(),ZOO_EPHEMERAL);
                }
            }
            LOG_OUT(LOG_LEVEL_SUCCESS," RpcChannel::Stared - Rpc Service Stared");
            // 进入阻塞状态
            server->start();
            m_loop.loop();
        }

    private:
        using ProducerZK = ZK_Opreator<ProducerRpcLogger>;
        // 记录Rpc对象以及方法
        struct RpcObject
        {
            shared_ptr<google::protobuf::Service> service_ptr;                                      // rpc服务提供对象
            unordered_map<string, shared_ptr<const google::protobuf::MethodDescriptor>> method_map; // 记录方法名与方法描述的映射
        };
        unordered_map<string, RpcObject> RpcMap; // 记录对象名与rpc结构体的映射
        static shared_ptr<RpcChannel> channel;
        static mutex m;
        bool iStared;
        shared_ptr<muduo::net::TcpServer> server;
        muduo::net::EventLoop m_loop;

        RpcChannel() : iStared(false)
        {
            // 注：框架使用的端口号不应该和使用者要使用的端口相同，因为这是两个不同的服务，客户端调用RPC方法时可能需要注意
            // 创建rpc服务套接字
            string server_ip = ConfPrase(STR_SERVER_IP);
            uint16_t server_port = atoi(ConfPrase(STR_SERVER_PORT).c_str());
            muduo::net::InetAddress addr(server_ip, server_port);
            server = make_shared<muduo::net::TcpServer>(&m_loop, addr, CALLEE_SERVER_CHANNEL_NAME);
            // 绑定回调：连接回调、消息回调
            server->setConnectionCallback(bind(&RpcChannel::ConnectionCallBack, this, _1));
            server->setMessageCallback(bind(&RpcChannel::MessageCallBack, this, _1, _2, _3));
            // 设置线程数
            server->setThreadNum(ConfPraseINT(SERVER_THREAD_NUM));
        }

        // RPC执行完的回调函数，用于将响应消息res序列化以及发送
        void ClosureCallBack(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *res)
        {
            // 序列化
            string res_str;
            if (!res->SerializeToString(&res_str))
            {
                LOG_OUT(LOG_LEVEL_ERROR," RpcChannel::ClosureCallBack - response serialize Error! : call back response pkg Failed");
                return;
            }
            // 发送回客户端
            conn->send(res_str);
            // 模拟短连接，由服务端主动断开
            conn->shutdown();
        }

        void ConnectionCallBack(const muduo::net::TcpConnectionPtr &conn)
        {
            if (!conn->connected())
                conn->shutdown();
        }

        /*
            客户端发送的Rpc请求，需要包含一个4字节的请求头大小(二进制整数字符串),来获取头部的protobuf，
            而且参数不能固定在protobuf message中，因此只能跟在头部的后面
            可以直接包含进json中进行发送，简化操作
            但损失了使用protobuf的优势
            因此采用字节流的方式
        */
        void MessageCallBack(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp time)
        {
            string recv_buf = buf->retrieveAllAsString();
            string tmp_str;
            // 读取前4字节的请求头大小(二进制)
            uint32_t header_size = 0;
            recv_buf.copy((char *)&header_size, ConfPraseINT(HEADERPKG_BYTE_SIZE), 0);
            tmp_str = "header_size = "+to_string(header_size);
            LOG_OUT(LOG_LEVEL_INFO,tmp_str);
            // 获取请求头
            loginrpc::MessageHeader header;
            if (!header.ParseFromString(recv_buf.substr(ConfPraseINT(HEADERPKG_BYTE_SIZE), header_size)))
            {
                tmp_str = " RpcChannel::MessageCallBack - Message Header Prase Error! header_size = "+to_string(header_size);
                LOG_OUT(LOG_LEVEL_ERROR,tmp_str);
                return;
            }
            const uint32_t &args_size = header.args_size();
            const string &service_name = header.service_name();
            const string &method_name = header.method_name();
            // 获取参数,序列化的请求message类型
            string args = recv_buf.substr(ConfPraseINT(HEADERPKG_BYTE_SIZE) + header_size, args_size);
            tmp_str = " RpcChannel::MessageCallBack - service_name = " + service_name +"\n\t"+"method_name = "+method_name+"\n\t"+"args_size = "+to_string(args_size)+"\n\t"+"args = "+args;
            LOG_OUT(LOG_LEVEL_INFO,tmp_str);
            // 查找对应的rpc方法
            shared_ptr<google::protobuf::Service> service;
            shared_ptr<const google::protobuf::MethodDescriptor> method;
            if (RpcMap.count(service_name) != 0 && RpcMap[service_name].method_map.count(method_name) != 0)
            {
                service = RpcMap[service_name].service_ptr;
                method = RpcMap[service_name].method_map[method_name];
            }
            else
            {
                tmp_str = service_name+" or "+method_name + " is not exist!";
                LOG_OUT(LOG_LEVEL_ERROR,tmp_str);
                return;
            }

            // 生成请求/响应参数（原型）
            shared_ptr<google::protobuf::Message> request = shared_ptr<google::protobuf::Message>(service->GetRequestPrototype(method.get()).New());
            if (!request->ParseFromString(args))
            {
                tmp_str = " RpcChannel::MessageCallBack - request args parse Error! - args: " + args;
                LOG_OUT(LOG_LEVEL_ERROR,tmp_str);
                return;
            }
            shared_ptr<google::protobuf::Message> response = shared_ptr<google::protobuf::Message>(service->GetResponsePrototype(method.get()).New());

            // 执行相应的RPC方法
            service->CallMethod(method.get(), nullptr, request.get(), response.get(),
                                // 生成相应回调函数的Closure对象
                                google::protobuf::NewCallback<RpcChannel, const muduo::net::TcpConnectionPtr &, google::protobuf::Message *>(this, &RpcChannel::ClosureCallBack, conn, response.get()));
        }
    };
    shared_ptr<RpcChannel> RpcChannel::channel = nullptr;
    mutex RpcChannel::m;
};
