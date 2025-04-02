#pragma once
#include "pb_include.hpp"
#include "rpc_servrice.hpp"

string GenerateLogName(string dir){
    auto current_time = chrono::system_clock::now();
    time_t t_time = chrono::system_clock::to_time_t(current_time);
    struct tm *current_tm = localtime(&t_time);
    char buffer[128];
    strftime(buffer, sizeof(buffer), "logfile-%Y-%m-%d.txt", current_tm);
    if(dir.at(dir.size()-2) != '/')
        dir += "/";
    return dir+string(buffer);
}

//日志消息处理类
class ProcessMessage{
public:
    static void AppendMessage(const string& level,const string& msg){
        //level为空代表消息自带level
        if(logfile.is_open() && !level.empty()){
            logfile<<getcurrenttime()<<level<<msg<<endl;
        }else if(logfile.is_open() && level.empty()){
            logfile<<getcurrenttime()<<msg<<endl;
        }else{
            LOG_INFO<<"- Opend File Error, StrInfo: "<<msg;
        }
    }

    static string getcurrenttime(){
        auto current_time = chrono::system_clock::now();
        time_t t_time = chrono::system_clock::to_time_t(current_time);
        struct tm* current_tm = localtime(&t_time); 
        char buffer[64];
        strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S ",current_tm);
        return string(buffer);
    }
private:
    static ofstream logfile;
};
ofstream ProcessMessage::logfile(GenerateLogName(ConfPrase(SERVERLOGFILEPATH)),ios::app);


class ClientProcessMessage
{
public:
    static void AppendMessage(const string &level, const string &msg)
    {
        // level为空代表消息自带level
        if (logfile.is_open() && !level.empty())
        {
            logfile << getcurrenttime() << level << msg<<" " << ConfPrase(STR_SERVER_IP)<<":"<<ConfPrase(STR_SERVER_PORT)<< endl;
        }
        else if (logfile.is_open() && level.empty())
        {
            logfile << getcurrenttime() << msg<<" " << ConfPrase(STR_SERVER_IP)<<":"<<ConfPrase(STR_SERVER_PORT) << endl;
        }
        else
        {
            LOG_INFO << "- Opend File Error, StrInfo: " << msg<<" " << ConfPrase(STR_SERVER_IP)<<":"<<ConfPrase(STR_SERVER_PORT);
        }
    }

    static string getcurrenttime()
    {
        auto current_time = chrono::system_clock::now();
        time_t t_time = chrono::system_clock::to_time_t(current_time);
        struct tm *current_tm = localtime(&t_time);
        char buffer[64];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S ", current_tm);
        return string(buffer);
    }

private:
    static ofstream logfile;
};
ofstream ClientProcessMessage::logfile(GenerateLogName(ConfPrase(CLIENTLOGFILEPATH)),ios::app);

//向kafka提交kv后执行的回调
//主要用于判断操作是否成功
//不需要在该回调中手动重试，由kafka进行重试
class KafkaMessageProduceDeliveryReportCB: public RdKafka::DeliveryReportCb{
public:
    void dr_cb(RdKafka::Message& m){
        string brokerid = to_string(m.broker_id());
        if(m.err()){
            ProcessMessage::AppendMessage(LOG_LEVEL_FAILED,brokerid+" KafkaMessageProduceDeliveryReportCB::dr_cb - Message Producer Delivery To Topic Failed: "+m.errstr());
        }else{
            ProcessMessage::AppendMessage(LOG_LEVEL_SUCCESS ,brokerid+" KafkaMessageProduceDeliveryReportCB::dr_cb - Message Producer Delivery To Topic Success: topicname: "+m.topic_name()+" partition: "+to_string(m.partition()));
        }
    }
};

//处理kafka各种事件的回调
//主要用于捕获 Kafka 集群连接状态、错误日志、生产者和消费者的状态等信息
class KafkaEventCB: public RdKafka::EventCb{
public:
    void event_cb(RdKafka::Event& event){
        string brokerid = to_string(event.broker_id());
        switch (event.type())
        {
        case RdKafka::Event::EVENT_ERROR :
            ProcessMessage::AppendMessage(LOG_LEVEL_ERROR,brokerid+" KafkaEventCB::event_cb - Kafka Client Error Event! Info: "+RdKafka::err2str(event.err()));
            break;
        case RdKafka::Event::EVENT_STATS :
            ProcessMessage::AppendMessage(LOG_LEVEL_WARNING,brokerid+" KafkaEventCB::event_cb - Kafka States Change Event! Info: "+event.str());
            break;
        case RdKafka::Event::EVENT_LOG :
            ProcessMessage::AppendMessage(LOG_LEVEL_INFO,brokerid+" KafkaEventCB::event_cb - Kafka Log Event! Info: "+event.fac());
            break;
        case RdKafka::Event::EVENT_THROTTLE :
            ProcessMessage::AppendMessage(LOG_LEVEL_WARNING,brokerid+" KafkaEventCB::event_cb - Kafka Throttle Event! Info: throttled by broker: "+event.broker_name()+", throttle time: "+to_string(event.throttle_time()));
            break;
        default:
            ProcessMessage::AppendMessage(LOG_LEVEL_ERROR,brokerid+" KafkaEventCB::event_cb - Kafka Unknow Event!");
            break;
        }
    }
};



//自定义kafka分区策略
//使用sha256哈希算法作为分区策略
//提高分布的均衡性，一致性哈希，避免热点问题，适应多种数据类型，提高容错性
//kafka默认使用CRC32哈希算法
//足够快速，并且在大多数应用场景中提供了良好的性能
//
//在需要高度数据完整性和安全性时可使用sha256哈希算法
//虽然sha256带来的开销较大，可在性能与安全性之间权衡
class TopicPartitionCB_Sha256: public RdKafka::PartitionerCb{
public:
    int32_t partitioner_cb(const RdKafka::Topic *topic,
                           const std::string *key,
                           int32_t partition_cnt,
                           void *msg_opaque)
    {
        string str = topic->name() + (*key);
        //获取哈希值
        unsigned char hashset[SHA256_DIGEST_LENGTH];
        SHA256_CTX _sha256;
        SHA256_Init(&_sha256);
        SHA256_Update(&_sha256,str.c_str(),str.size());
        SHA256_Final(hashset,&_sha256);
        //将其转换为十六进制字符串
        stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        {
            ss << setw(2) << setfill('0') << hex << (int)hashset[i];
        }
        string hash_str = ss.str();
        //转换为整数
        return stoll(hash_str,nullptr,16)%partition_cnt;
    }
};

class RpcMessageProducer{
public:
    RpcMessageProducer():kafka_conf(nullptr),
                        topic_conf(nullptr),
                        _producer(nullptr),
                        _topic(nullptr),
                        _dr_cb(nullptr),
                        _event_cb(nullptr),
                        _partition_cb(nullptr),
                        is_init(false)
                        {}

    ~RpcMessageProducer(){
        End();
    }

    //初始化
    bool Init(){
        if(is_init == true)
            return false;
        RdKafka::Conf::ConfResult t_conf; //保存create时的全局或主题配置
        string errmsg;  //存储错误信息
        do{
            //创建kafka conf对象
            kafka_conf = shared_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
            if(!kafka_conf){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Producer Create Kafka Conf Error!");
                break;
            }
            //设置broker信息，bootstrap.servers - 生产者连接集群所需的broker地址清单
            t_conf = kafka_conf->set("bootstrap.servers",ConfPrase(KAFKA_BROKER_LIST),errmsg);
            if(t_conf !=RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Set Kafka Broker Address List Error: "+errmsg);
                break;
            }
            //linger.ms - 生产者调用produce时，不会立即发送，而是等待linger.ms时间，累积多条消息一并发送
            //当等于0时代表立即发送，即一次发送一条，设置时间越大，吞吐量越大，推荐5-100ms
            //如果发送时缓冲队列已满或空间不足时会一直等待，或者等待max.block.ms，如果依然不足则producer抛出TimeoutException异常
            t_conf = kafka_conf->set("linger.ms",ConfPrase(KAFKA_PRODUCE_LINGER_MS),errmsg);
            if(t_conf !=RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Set Kafka Produce Wait Linger Time Error: "+errmsg);
                break;
            }

            //statistics.interval.ms - 设置生成统计信息报告的时间间隔(ms)，包含各种性能和状态数据
            t_conf = kafka_conf->set("statistics.interval.ms",ConfPrase(KAFKA_STATISTICS_INTERVAL_MS),errmsg);
            if(t_conf !=RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Set Kafka Create Statistics Report Interval Error: "+errmsg);
                break;
            }
            //message.max.bytes - 设置最大发送消息大小(Byte)
            t_conf = kafka_conf->set("message.max.bytes",ConfPrase(KAFKA_MESSAGE_MAX_BYTES),errmsg);
            if(t_conf !=RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Set Kafka Client Max Send Message Length Error: "+errmsg);
                break;
            }
            //创建Topic Conf对象
            topic_conf = shared_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));
            if(!kafka_conf){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Create Topic Conf Error!");
                break;
            }
            //创建Producer对象，用于发送消息于不同主题
            _producer = shared_ptr<RdKafka::Producer>(RdKafka::Producer::create(kafka_conf.get(),errmsg));
            if(!_producer){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Create Kafka Producer Object Error: "+errmsg);
                break;
            }
            //创建Topic对象，用于存放主题信息
            _topic = shared_ptr<RdKafka::Topic>(RdKafka::Topic::create(_producer.get(),ConfPrase(KAFKA_LOG_TOPIC_NAME),topic_conf.get(),errmsg));
            if(!_topic){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Create Kafka Topic Object Error: "+errmsg);
                break;
            }
            //设置Kafka提交事件回调
            _dr_cb = make_shared<KafkaMessageProduceDeliveryReportCB>();
            t_conf = kafka_conf->set("dr_cb",_dr_cb.get(),errmsg);
            if(t_conf !=RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Set Kafka Delivery Report CallBack Error: "+errmsg);
                break;
            }
            //设置Kafka处理事件回调
            _event_cb = make_shared<KafkaEventCB>();
            t_conf = kafka_conf->set("event_cb",_event_cb.get(),errmsg);
            if(t_conf !=RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Set Kafka Event CallBack Error: "+errmsg);
                break;
            }
            //设置Kafka topic自定义分区策略
            _partition_cb = make_shared<TopicPartitionCB_Sha256>();
            t_conf = topic_conf->set("partitioner_cb",_partition_cb.get(),errmsg);
            if(t_conf !=RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::Init - Set Kafka Topic SelfDefine Partition CallBack Error: "+errmsg);
                break;
            }
            //初始化成功
            is_init = true;
            ProcessMessage::AppendMessage(LOG_LEVEL_SUCCESS," RpcMessageProducer::Init - Init RpcMessageProducer success!");
            return true;  
        }while(0);
        End();
        return false;
    }

     //释放资源
    void End(){
        //确保生产者消息队列中没有要发送的消息，outq_len返回队列中要发送的消息数
        while(_producer != nullptr && _producer->outq_len() > 0){
            //队列不为空
            //flush尝试等待(ms), 等待过程中，flush会尝试发送队列中要发送的消息
            //如果在等待时间内发送完全部消息则立即返回
            _producer->flush(ConfPraseINT(KAFKA_PRODUCER_FLUSH_TIME));
        }
    }

    //生产者推送消息至kafka主题
    void CommitMessage(const string& key,const string& value){
        //生产者推送value至指定topic,内部会自动添加发送时间
        //RdKafka::Topic::PARTITION_UA表示使用自定义分区策略回调函数partition_cb进行分区
        //RdKafka::Producer::RK_MSG_COPY表示拷贝要发送的消息
        //produce只负责发送消息至缓存队列，因此返回值不代表消息发送成功
        //kafka默认重传retries次，默认无限次
    redo:
        RdKafka::ErrorCode errcode = _producer->produce(_topic.get(),
                                                        RdKafka::Topic::PARTITION_UA,
                                                        RdKafka::Producer::RK_MSG_COPY,
                                                        const_cast<void *>(static_cast<const void *>(value.c_str())),
                                                        value.size(),
                                                        const_cast<string *>(&key),
                                                        nullptr);
        //检查是否发送到kafka消息缓存队列
        if(errcode != RdKafka::ERR_NO_ERROR){
            ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageProducer::CommitMessage - Kafka Producer Send Message Error! Info: "+RdKafka::err2str(errcode));
            //如果是队列已满,调用poll等待一段时间，同时处理内部事件以清理队列, 随后尝试重传
            if(errcode == RdKafka::ERR__QUEUE_FULL){
                _producer->poll(ConfPraseINT(KAFKA_MSGLISTFULL_WAIT_TIME));
                goto redo;
            }
            return;
        }
        //发送消息后启动事件处理，poll会在指定时间内查询释放有事件发生，如消息发送完成后触发dr_cb回调
        _producer->poll(ConfPraseINT(KAFKA_PRODUCER_POLL_TIMEOUT));
        
    }

private:
   


    atomic<bool> is_init;

    shared_ptr<RdKafka::Conf>              kafka_conf;
    shared_ptr<RdKafka::Conf>              topic_conf;
    shared_ptr<RdKafka::Topic>             _topic;
    shared_ptr<RdKafka::Producer>           _producer;
    shared_ptr<RdKafka::DeliveryReportCb>  _dr_cb;
    shared_ptr<RdKafka::EventCb>           _event_cb;
    shared_ptr<RdKafka::PartitionerCb>     _partition_cb;
};

//自定义kafka消费者再平衡策略回调
//设置自定义再平衡回调会取消kafka默认的自动分区和再分配
//再平衡：
//当消费者组、topic、partition等发生变化时，会触发再平衡机制，将重新选举一个消费者组leader以及重新分配分区
//再平衡机制通常应该避免: 
//再平衡时消费者无法进行消费，导致kafka无法被使用、再平衡机制效率低，需要消费者组中所有成员参与
//再平衡应该尽量避免，但无法完全避免，以此并没有很好的解决方案，实际生产中再平衡都没有在计划中，以此只能尽量避免
class ConsumerReBalanceCB: public RdKafka::RebalanceCb{
public:
    //err: 再平衡是因为分区获取成功还是失败引起的
    //partitions：获取的需要再分配的分区列表
    void rebalance_cb(RdKafka::KafkaConsumer *consumer,
                      RdKafka::ErrorCode err,
                      std::vector<RdKafka::TopicPartition *> &partitions)
    {
        ProcessMessage::AppendMessage(LOG_LEVEL_WARNING," ConsumerReBalanceCB::rebalance_cb - Kafka consumer "+consumer->name()+" ReBalance CallBack! Info: "+RdKafka::err2str(err));
        if(err == RdKafka::ERR__ASSIGN_PARTITIONS){
            //分区分配成功，消费者更新订阅分区
            consumer->assign(partitions);
        }else{
            //分区分配失败，消费者取消所有订阅
            consumer->unassign();
        }
    }
};

class RpcMessageConsumer{
public:
    RpcMessageConsumer() : is_init(false),
                           _stared(false),
                           consumer_conf(nullptr),
                           topic_conf(nullptr),
                           _consumer(nullptr),
                           _event_cb(nullptr),
                           _rebalance_cb(nullptr),
                           opt(nullptr){}
    ~RpcMessageConsumer()
    {
        stop();
    }
    bool Init(){
        if(_stared == true || is_init == true)
            return false;
        RdKafka::Conf::ConfResult confcode;
        string errmsg;
        do{
            consumer_conf = shared_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
            if(!consumer_conf){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Create Kafka Conf Error!");
                break;
            }
            //指定kafka broker集群地址
            confcode = consumer_conf->set("bootstrap.servers",ConfPrase(KAFKA_BROKER_LIST),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Kafka Broker Group Address Error! Info: "+errmsg);
                break;
            }
            //设置消费者所属组
            confcode = consumer_conf->set("group.id",ConfPrase(KAFKA_LOG_CONSUMER_GROUP_ID),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Consumer Group ID Error! Info: "+errmsg);
                break;
            }
            //设置回调
            _event_cb = make_shared<KafkaEventCB>();
            _rebalance_cb = make_shared<ConsumerReBalanceCB>();
            confcode = consumer_conf->set("event_cb",_event_cb.get(),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Event CallBack Error! Info: "+errmsg);
                break;
            }
            confcode = consumer_conf->set("rebalance_cb",_rebalance_cb.get(),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set ReBalance CallBack Error! Info: "+errmsg);
                break;
            }
            //设置从一个分区拉取消息最大字节数
            confcode = consumer_conf->set("max.partition.fetch.bytes",ConfPrase(KAFKA_PARTITION_FETCH_MAX_BYTES),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Max Fetch Bytes From Single Partition Error! Info: "+errmsg);
                break;
            }
            //设置从所有分区拉取消息总字节数
            confcode = consumer_conf->set("fetch.max.bytes",ConfPrase(KAFKA_FETCH_MAX_BYTES),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Max Fetch Bytes From Every Partition Error! Info: "+errmsg);
                break;
            }
            //设置消费者分区分配策略：
            //range连续分区，确保每个消费者获得的分区是连续的，适合消费者和分区数较少且需要分区连续(处理大规模数据时更加高效、稳定)，kafka默认
            //roundrobin轮询，保证每个消费者获取的分区均匀，适用于消费者数目不确定且分区数大
            //sticky稳定优先，在消费者数量发生变化时尽量维持已分配分区，适用于要求分区不变稳定且消费者数量少
            //coop.sticky平滑协调，比sticky多了协调机制，在消费者数量变化时以更平滑的方式重新分配，适合需要高稳定且消费者变化时进行调整
            confcode = consumer_conf->set("partition.assignment.strategy","range",errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Range Partition Assigment Strategy Error! Info: "+errmsg);
                break;
            }
            //设置消费者心跳保活机制
            confcode = consumer_conf->set("session.timeout.ms",ConfPrase(KAFKA_HEARTING_TIME_OUT_MS),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Hearting Time Error! Info: "+errmsg);
                break;
            }
            confcode = consumer_conf->set("heartbeat.interval.ms",ConfPrase(KAFKA_HEARTING_INTERVAL_MS),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Hearting Interval Error! Info: "+errmsg);
                break;
            }
            //创建topic conf
            topic_conf = shared_ptr<RdKafka::Conf>(RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC));
            if(!topic_conf){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Create Topic Conf Error!");
                break;
            }
            //设置新添加消费者的消费偏移量offset：
            //latest 从最新消息消费(默认)，earliest 从头开始消费，即获取所有消息
            confcode = topic_conf->set("auto.offset.reset",ConfPrase(KAFKA_CONSUMER_OFFSET_STRATEGY),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set New Join Consumer Offset Strategy Error! Info: "+errmsg);
                break;
            }
            //设置kafka client默认topic配置，使后续创建的topic应用相同配置
            confcode = consumer_conf->set("default_topic_conf",topic_conf.get(),errmsg);
            if(confcode != RdKafka::Conf::CONF_OK){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Consumer Set Default Topic Conf Error! Info: "+errmsg);
                break;
            }
            //创建consumer实例
            _consumer = shared_ptr<RdKafka::KafkaConsumer>(RdKafka::KafkaConsumer::create(consumer_conf.get(),errmsg));
            if(!_consumer){
                ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::Init - Create Kafka Consumer Error! Info: "+errmsg);
                break;
            }
            is_init = true;
            ProcessMessage::AppendMessage(LOG_LEVEL_SUCCESS," RpcMessageConsumer::Init - Init RpcMessageConsumer success!");
            return true;
        }while(0);
        stop();
        return false;
    }
    void stop(){
        _stared = false;
    }
    void setMsgCallBack(const function<void(void*)>& _opt){
        opt = _opt;
    }
    void start_recv(const vector<string> &topics){
        if(!_consumer || topics.empty() || !opt || _stared){
            return;
        }
        //消费者订阅主题
        RdKafka::ErrorCode errcode = _consumer->subscribe(topics);
        if(errcode != RdKafka::ERR_NO_ERROR){
            ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::start_recv - Consumer Subscribe Topics Error! Info: "+RdKafka::err2str(errcode));
            return;
        }
        _stared = true;
        //异步消费
        _thread = thread([&](){
            RdKafka::Message* msg = nullptr;
            while(_stared){
                //消费者拉取消息，如果超时返回nullptr
                msg = _consumer->consume(ConfPraseINT(KAFKA_FETCH_TIME_OUT_MS));
                if(msg != nullptr){
                    //处理消息
                    switch (msg->err())
                    {
                    case RdKafka::ERR__TIMED_OUT:   //超时
                        //ProcessMessage::AppendMessage(LOG_LEVEL_INFO," RpcMessageConsumer::start_recv - Consumer Consume Topics Message Time Out Error! Info: "+msg->errstr());
                        break;
                    case RdKafka::ERR_NO_ERROR:     //成功获取消息
                        //调用自定义消息处理函数
                        if(opt)
                            opt(msg->payload());
                        break;
                    default:
                        ProcessMessage::AppendMessage(LOG_LEVEL_ERROR," RpcMessageConsumer::start_recv - Consumer Consume Topics Message Unknowed Error!");
                        break;
                    }
                    delete msg;
                    msg = nullptr;
                }
            }
            //同步提交消费者本次消费的偏移量,在得到kafka返回的确认消息前处于阻塞状态
            if(_consumer){
                _consumer->commitSync();
            }
            ProcessMessage::AppendMessage(LOG_LEVEL_INFO," RpcMessageConsumer::start_recv - Consumer Consume END!");
        });
        _thread.detach();
    }

private:
    shared_ptr<RdKafka::Conf> consumer_conf;
    shared_ptr<RdKafka::Conf> topic_conf;
    shared_ptr<RdKafka::KafkaConsumer> _consumer;
    shared_ptr<RdKafka::EventCb> _event_cb;
    shared_ptr<RdKafka::RebalanceCb> _rebalance_cb;

    thread _thread;
    atomic<bool> _stared;
    atomic<bool> is_init;
    function<void(void*)> opt;
};