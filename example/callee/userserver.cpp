#include <libmyrpc/rpcserver.hpp>
#include "userlogin.pb.h"
#include "friend.pb.h"

// 本地业务发布RPC服务
// 1.编写proto文件，创建响应的类型

// 2.继承相应生成的类
class PUserLogin : public loginrpc::UserLoginRpc
{
public:
    bool login(const string &name, const string &pwd)
    {
        if (name.empty() || pwd.empty())
            return false;
        cout << "called UserLogin::login() : " << name << "-" << pwd << endl;
        return true;
    }

    bool rejister(string &id ,const string &name, const string &pwd)
    {
        if (name.empty() || pwd.empty())
            return false;
        cout << "called UserLogin::rejister() : " << name << "-" << pwd << endl;
        id = to_string(rand());
        return true;
    }

    // 3.rpc远程方法重载，caller端请求的方法
    void login(::google::protobuf::RpcController *controller,
               const ::loginrpc::LoginReq *request,
               ::loginrpc::LoginRes *response,
               ::google::protobuf::Closure *done)
    {

        // 1.获取参数数据
        string name = request->name();
        string pwd = request->pwd();

        // 2.业务操作
        if (login(name, pwd))
        {
            // 3.写入响应
            response->mutable_err()->set_errcode(0);
            response->mutable_err()->set_errmsg("");
            response->set_issuccess(true);
        }
        else
        {
            response->mutable_err()->set_errcode(1);
            response->mutable_err()->set_errmsg("方法调用失败");
            response->set_issuccess(false);
        }

        // 4.执行回调操作（响应数据序列化以及发送）
        if (done)
            done->Run();
    }

    void rejister(::google::protobuf::RpcController * controller,
        const ::loginrpc::RejisterReq * request,
        ::loginrpc::RejisterRes* response,
        ::google::protobuf::Closure* done){
            string id;
            string name = request->name();
            string pwd = request->pwd();
            if (rejister(id,name,pwd))
            {
                response->mutable_err()->set_errcode(0);
                response->mutable_err()->set_errmsg(id);
                response->set_issuccess(true);
            }
            else
            {
                response->mutable_err()->set_errcode(1);
                response->mutable_err()->set_errmsg("方法调用失败");
                response->set_issuccess(false);
            }
            if (done)
                done->Run();
        }

};

class FriendMethod: public friendrpc::FriendMethodRpc
{
    vector<string> friendlist(const string& name){
        cout<<"called FriendMethod::friendlist by : "<<name<<endl;
        return {"战双","帕弥什","鸣潮"};
    }

    void friendlist(::google::protobuf::RpcController* controller,
        const ::friendrpc::FriendListReq* request,
        ::friendrpc::FriendListRes* response,
        ::google::protobuf::Closure* done){

            string name = request->name();
            vector<string> vec = friendlist(name);
            response->mutable_err()->set_errcode(0);
            response->mutable_err()->set_errmsg("获取成功");
            for(string& s:vec)
                response->add_friends(s);
            if(done)
                done->Run();

        }
};

int main(int argc, char **args)
{
    // 1.初始化框架，传入配置文件：rpc_callee -i coonfigure.conf
    RpcService::GetInstance()->Init(argc, args);
    // 2.发布对象到rpc节点上
    rpc_server_pkg::RpcChannel::GetInstanse()->RpcRegisted(new PUserLogin());
    rpc_server_pkg::RpcChannel::GetInstanse()->RpcRegisted(new FriendMethod());
    // 3.启动rpc服务发布节点,会阻塞当前线程，可以创建一个新线程去异步处理RPC，交给用户决定
    rpc_server_pkg::RpcChannel::GetInstanse()->Stared();
    return 0;
}
