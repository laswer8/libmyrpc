#include <iostream>
#include <string>
#include "test.pb.h"

using namespace std;
/*
如果proto生成的.h文件报红解决方案：

include后添加以下代码：

#define PROTOBUF_NAMESPACE_OPEN \
  namespace google              \
  {                             \
    namespace protobuf          \
    {
#define PROTOBUF_NAMESPACE_CLOSE \
  }                              \
  }
#define PROTOBUF_NAMESPACE_ID google::protobuf
#define PROTOBUF_CONSTEXPR
#define PROTOBUF_ATTRIBUTE_REINITIALIZES
#define PROTOBUF_NODISCARD [[nodiscard]]
#define PROTOBUF_ALWAYS_INLINE
using namespace google;

*/
int main(){
    logintest::LoginReq request;
    request.set_name("2836992987");
    request.set_pwd("1472583369as");
    string str;
    if(request.SerializeToString(&str)){
        cout<<"序列化："<<endl<<str<<endl;
    }else{
        cout<<"序列化失败"<<endl;
        return -1;
    }
    logintest::GetFriendListsRes res;
    auto err_ptr = res.mutable_err(); //返回自定义类型的成员变量的指针引用，直接修改指针指向的对象即可
    err_ptr->set_errcode(0);
    err_ptr->set_errmsg("无错误有，errmsg可以不用设置，根据业务进行处理");

    auto user1 = res.add_friendlists(); //向列表添加一个元素，直接返回改元素的指针引用
    user1->set_name("李老师");
    user1->set_age(53);
    user1->set_sex(logintest::User::Sex::User_Sex_MAN);
    auto user2 = res.add_friendlists(); //向列表添加一个元素，直接返回改元素的指针引用
    user2->set_name("撒旦王");
    user2->set_age(26);
    user2->set_sex(logintest::User::Sex::User_Sex_WOMAN);

    auto lists = res.friendlists();
    

    cout<<"列表大小："<<res.friendlists_size()<<endl;
    cout<<"元素："<<endl;
    for(auto& i:lists){
      cout<<"姓名"<<i.name()<<" 年龄"<<i.age()<<" 性别"<<i.sex()<<endl;
    }
    if(res.SerializeToString(&str)){
        cout<<"序列化："<<endl<<str<<endl;
    }else{
        cout<<"序列化失败"<<endl;
        return -1;
    }
    return 0;
}
