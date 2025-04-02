#ifndef PB_INCLUDE
#define PB_INCLUDE

#include <iostream>
#include <string>
#include <algorithm>
#include <atomic>
#include <bitset>
#include <memory>
#include <mutex>
#include <list>
#include <zookeeper/zookeeper.h>
#include <thread>
#include <condition_variable>
#include <librdkafka/rdkafkacpp.h>
#include <openssl/sha.h>
#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>
#include <google/protobuf/descriptor.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Logging.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/net/EventLoopThread.h>
#include <unistd.h>
#include <fstream>
#include <time.h>
#include <tuple>
#include <chrono>
#include <functional>
#include <vector>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>


using namespace std;
using namespace placeholders;
using json = nlohmann::json;

//固定读取配置文件路径
const string CONFILEPATH                    ="confile.conf";

// 配置文件宏
const string STR_SERVER_IP                  = "str_server_ip";
const string STR_SERVER_PORT                = "str_server_port";
const string STR_ZOOKEEPER_IP               = "str_zookeeper_ip";
const string STR_ZOOKEEPER_PORT             = "str_zookeeper_port";

const string SERVER_THREAD_NUM              = "server_thread_num";
const string HEADERPKG_BYTE_SIZE            = "headerpkg_byte_size";                //请求头固定字节大小

const string CALLEE_SERVER_CHANNEL_NAME     = "challee_server_channel_name";     //rpc服务提供方套接字服务名

const string BUFFERLEN                      = "bufferlen";             //缓冲区大小

const string TIMEWHEEL_SIZE                 = "timewheel_size";               //计时器任务容量
const string TIMEWHEEL_INTERVAL_MS          = "timewheel_interval_ms";             //计时器触发间隔

const string TIMEOUT_INTERVAL_MS            = "timeout_interval_ms";             //rpc请求超时时间

const string SERVERLOGFILEPATH              = "serverlogfilepath"; //日志文件
const string CLIENTLOGFILEPATH              = "clientlogfilepath"; //日志文件

const string KAFKA_BROKER_LIST              = "kafka_broker_list"; //broker列表(ip:port,ip:port,....)
const string KAFKA_LOG_TOPIC_NAME           = "kafka_log_topic_name";       //存储日志消息的主题名称
const string KAFKA_PARTITION_CNT            = "kafka_partition_cnt";                //kafka分区数
const string KAFKA_STATISTICS_INTERVAL_MS   = "kafka_statistice_interval_ms";          //生成统计信息报告的时间间隔(ms)
const string KAFKA_MESSAGE_MAX_BYTES        = "kafka_message_max_bytes";        //kafka最大发送消息大小(Byte)
const string KAFKA_PRODUCER_FLUSH_TIME      = "kafka_producer_flush_time";             //kafka producer调用flush等待时间(ms)
const string KAFKA_PRODUCER_POLL_TIMEOUT    = "kafka_producer_poll_timeout";              //kafka producer调用poll超时时间(ms)
const string KAFKA_MSGLISTFULL_WAIT_TIME    = "kafka_msglistfull_wait_time";              //kafka produce因队列已满而导致失败的重传等待时间(ms)
const string KAFKA_PRODUCE_LINGER_MS        = "kafka_produce_linger_ms";             //kafka 累计produce发送的消息的时间(ms)，与吞吐量成正比

const string KAFKA_LOG_CONSUMER_GROUP_ID    = "kafka_log_consumer_group_id";         //kafka 日志处理消费者所属消费者组ID
const string KAFKA_HEARTING_INTERVAL_MS     = "kafka_hearting_interval_ms";           //kafka 消费者与kafka的心跳保活间隔时间(ms)，kafka默认3s
const string KAFKA_HEARTING_TIME_OUT_MS     = "kafka_hearting_time_out_ms";          //kafka 消费者与Kafka集群的心跳保活时间，kafka默认45s
const string KAFKA_FETCH_MAX_RECODES        = "kafka_fetch_max_recodes";             //kafka 消费者一次fetch最大能拉取的消息数，kafka默认500
const string KAFKA_FETCH_MAX_BYTES          = "kafka_fetch_max_bytes";        //kafka 消费者一次fetch从所有分区中获取数据的最大字节数，kafka默认为50m，通常用于控制网络带宽
const string KAFKA_PARTITION_FETCH_MAX_BYTES= "kafka_partition_fetch_max_bytes";        //kafka 消费者一次fetch从单个分区中获取数据的最大字节数，kafka默认为1m，通常用于控制读取文件大小
const string KAFKA_FETCH_MAX_INTERVAL_MS    = "kafka_fetch_max_interval_ms";         //kafka 消费者发起下一次拉取的最长间隔，一旦超出kafka会认为该消费者已掉线触发再平衡，kafka默认5min
const string KAFKA_FETCH_TIME_OUT_MS        = "kafka_fetch_time_out_ms";             //kafka 消费者拉取消息的超时时间
const string KAFKA_CONSUMER_OFFSET_STRATEGY = "kafka_consumer_offset_strategy";         //kafka 新添加消费者初始偏移量策略，默认latest

//LOG_LEVEL
const string LOG_LEVEL_ERROR    = " - ERROR - ";
const string LOG_LEVEL_FAILED   = " - FAILED - ";
const string LOG_LEVEL_SUCCESS  = " - SUCCESS - ";
const string LOG_LEVEL_WARNING  = " - WARNING - ";
const string LOG_LEVEL_INFO     = " - INFO - ";

#endif // !PB_INCLUDE