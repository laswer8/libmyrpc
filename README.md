项目依赖：
    nlohmann/json, zookeeper/zookeeper.h, muduo, librdkafka, google/protobuf
    
项目描述：
    设计并实现了一个分布式服务框架，包含RPC服务发布与调用、日志消息队列、分布式锁等功能，满足分布式系统的需求
    
项目编译：
    1. 请确认默认的配置文件confile.conf符合需求
    2. 进入目录后运行autobuild.sh
    3. 请参考example目录下的proto文件生成对应的proto文件，并使用protoc正确生成库
    4. 在需使用的文件中通过include<libmyrpc/...>进行引入
    
Rpc功能添加:
    1. 需要添加新的Rpc服务需要进行代码的增添
    2. 创建对应rpc服务的proto文件并使用protoc编译
    3. rpc方法提供端按照example目录下callee下的文件进行修改
    4. rpc调用方修改src/include/rpc_clientchannel.hpp文件，在RpcMethod中添加对应*_stub
    
扩展:
    该项目通过zookeeper获取对应rpc方法节点下对应的服务器名，可以通过keepalived与nginx，完成将nginx集群通过keepalived生成一个vip保存到rpc节点中，使调用方获取这个vip去访问nginx集群，提供rpc方法的服务器交由nginx管理
    
主要功能：
    1. rpc服务发布/调用：通过zookeeper的服务注册中心功能发现rpc服务并调用，方法提供端通过proto文件创建rpc服务
    2. 日志处理：利用Kafka实现日志消息的发布与消费，I/O线程发送日志并异步写入日志文件
