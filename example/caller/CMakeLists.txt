set(SRC_LIST userclient.cpp ../userlogin.pb.cc ../friend.pb.cc)

add_executable(rpc_caller ${SRC_LIST})

target_link_libraries(rpc_caller prpc protobuf  muduo_base pthread muduo_net)