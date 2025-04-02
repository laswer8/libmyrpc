#include "userlogin.pb.h"
#include "friend.pb.h"
#include <libmyrpc/rpcclient.hpp>

using namespace std;

// 模拟rpc调用方使用过程
int main(int argc,char** argv){
    
    //初始化框架（保证只初始化一次）
    RpcService::GetInstance()->Init(argc,argv);
    //调用rpc方法
    loginrpc::RejisterReq request;
    request.set_name("2836992987");
    request.set_pwd("123456");
    loginrpc::RejisterRes response;
    Rpc_Controller m_controller;
    m_controller.StartDelayCancel(ConfPraseINT(TIMEOUT_INTERVAL_MS));
    rpc_client_pkg::RpcMethod::GetInstance()->UserLoginStub->rejister(&m_controller,&request,&response,nullptr);
    //继续业务
    if(m_controller.Failed()){
        cout<<"Rpc called rejister err: "<<m_controller.ErrorText()<<endl;
    }else if(!response.err().errcode())
        cout<<"Rpc called rejister success - id: "<<response.err().errmsg()<<endl;
    else{
        cout<<"Rpc called rejister error: "<<response.err().errmsg()<<endl;
    }

    friendrpc::FriendListReq friendrequest;
    friendrequest.set_name("2836992987");
    friendrpc::FriendListRes friendresponse;
    m_controller.StartDelayCancel(ConfPraseINT(TIMEOUT_INTERVAL_MS));
    rpc_client_pkg::RpcMethod::GetInstance()->FriendStub->friendlist(&m_controller,&friendrequest,&friendresponse,nullptr);
    if(m_controller.Failed()){
        cout<<"Rpc called friendlist err: "<<m_controller.ErrorText()<<endl;
    }else{
        cout<<"FriendNum = "<<friendresponse.friends_size()<<endl;
        for(int i=0;i<friendresponse.friends_size();i++){
            cout<<"friend"<<i<<" : "<<friendresponse.friends(i)<<endl;
        }
    }
    

    return 0;
}