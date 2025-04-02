#ifndef RPC_CONTROLLER_HPP
#define RPC_CONTROLLER_HPP

#include "pb_include.hpp"
#include "rpc_servrice.hpp"

// RpcController 用于控制单个rpc方法的调用，主要是提供一种方式来操作特定的 RPC 方法的设置，并获取 RPC 层面的错误
//
// RpcController 是一个抽象类，提供了用于实现功能的基础接口，使所有实现都能支持这些特性
// 处理实现基础接口，还可以自行提供更多功能，如rpc调用取消时间
class Rpc_Controller : public ::google::protobuf::RpcController
{
public:
    // rpc客户端方法 ---------------------------------------------
    // 这些方法只能由rpc调用方使用，在rpc服务端的行为是未知的，可能导致崩溃

    Rpc_Controller()
    {
        is_failed = false;
        is_canceled = false;
        errmsg = "";

        wheel_size = ConfPraseINT(TIMEWHEEL_SIZE);
        interval_ms = ConfPraseINT(TIMEWHEEL_INTERVAL_MS);
        current_index = 0;
        wheels.resize(wheel_size);
        is_timed = false;
    }

    ~Rpc_Controller() {
        stop();
    }

    // 将 RpcController 重置为其初始状态，以便可以在新的调用中重复使用
    // 在 RPC 进行时不得调用此方法
    virtual void Reset()
    {
        is_failed = false;
        is_canceled = false;
        errmsg = "";

        wheel_size = ConfPraseINT(TIMEWHEEL_SIZE);
        interval_ms = ConfPraseINT(TIMEWHEEL_INTERVAL_MS);
        current_index = 0;
        wheels.clear();
        wheels.resize(wheel_size);
        is_timed = false;
    }

    // 在rpc方法调用完成后，如果调用失败则返回 true。失败的可能原因取决于 RPC 实现
    // 在调用完成之前不得调用 Failed()
    // 如果 Failed() 返回 true，则响应消息的内容是未知的
    virtual bool Failed() const
    {
        return is_failed;
    }

    // 如果 Failed() 返回 true，返回一个人类可读的错误描述
    virtual string ErrorText() const
    {

        return errmsg;
    }

    // 通知 RPC 调用方希望取消 RPC 调用
    // RPC 服务方可以立即取消它，可以稍等一会儿然后取消，或者根本不取消调用
    // 如果调用被取消，"done" 回调仍然会被调用，并且 RpcController 会在那时表明调用失败
    virtual void StartCancel()
    {
        is_canceled = true;
    }

    // 开启定时通知 RPC 调用方希望取消 RPC 调用
    void StartDelayCancel(size_t ms)
    {
        if(is_timed)
            return;
        is_timed = true;
        emplacetask(ms,[this](){is_canceled = true;});
        start();
    }

    // 停止定时
    void StoptTimeWheel()
    {
        if(!is_timed)
            return;
        is_timed = false;
        stop();
    }

    // rpc服务端方法 ---------------------------------------------
    // 这些调用只能从rpc服务提供方调用，在rpc调用方使用的结果是未知的，可能导致崩溃
    //
    // 使客户端的 Failed() 返回 true。 "reason" 将被包含在 ErrorText() 返回的消息中
    // 如果您需要返回关于失败的机器可读信息，应该将其包含在响应协议缓冲区中，而不是调用 SetFailed()
    virtual void SetFailed(const std::string &reason)
    {
        is_failed = true;
        errmsg = reason;
    }

    // 如果为 true，表示客户端取消了 RPC，因此服务器也可以放弃回复它
    // 服务器仍然应该调用最终的 "done" 回调
    virtual bool IsCanceled() const
    {
        return is_canceled;
    }

    // 调用在 RPC 被取消时给定的回调函数
    // 回调函数将始终被调用一次。如果 RPC 完成且未被取消，回调将在完成后被调用
    // 如果在调用 NotifyOnCancel() 时 RPC 已经被取消，回调将立即被调用
    //
    // NotifyOnCancel() 每个请求只能调用一次
    virtual void NotifyOnCancel(::google::protobuf::Closure *callback)
    {
        callback->Run();
    }

private:
    // 开始计时
    void start()
    {
        if (is_timed)
        {
            return;
        }
        is_timed = true;
        m_thread = std::thread([this]()
                               {
            while (is_timed) {
                this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                take();
            } });
        m_thread.detach();
    }

    void stop(){
        if (!is_timed)
        {
            return;
        }
        is_timed = false;
        if(m_thread.joinable())
            m_thread.join();
    }

    void emplacetask(size_t timeout_ms,function<void()> task){
        lock_guard<mutex> guard(m);
        size_t times = timeout_ms/interval_ms;
        size_t index = (current_index + times)%wheel_size;
        size_t index_t = index;
        for (size_t i = 1 ; index_t < wheel_size; i++)
        {
            index_t = index * i;
            if (index_t >= wheel_size)
                break;
            wheels[index_t].push_back(task);
        }
    }

    // 执行定时任务
    void take()
    {
        lock_guard<mutex> guard(m);
        auto &lists = wheels[current_index];
        for(auto& t:lists)
            t();
        lists.clear();
        current_index = (current_index + 1) % wheel_size;
    }

    bool is_failed;   // rpc调用状态，失败为true
    bool is_canceled; // rpc调用是否被取消，取消为true
    string errmsg;    // rpc调用失败的错误信息

    size_t wheel_size;  // 任务容量
    size_t interval_ms; // 定时器间隔（ms）
    size_t current_index;
    vector<list<function<void()>>> wheels; // 任务队列
    bool is_timed;                             // 是否正在计时
    thread m_thread;
    mutex m;
};

#endif // RPC_CONTROLLER_HPP