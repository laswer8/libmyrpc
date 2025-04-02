set(SRC_LIST userserver.cpp ../userlogin.pb.cc ../friend.pb.cc)

add_executable(rpc_callee ${SRC_LIST})
 
target_link_libraries(rpc_callee prpc protobuf  muduo_base pthread muduo_net)