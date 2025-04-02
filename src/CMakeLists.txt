aux_source_directory(. SRC_LIST)

add_library(prpc STATIC ${SRC_LIST})

target_link_libraries(prpc rdkafka++ muduo_base pthread muduo_net crypto zookeeper_mt)