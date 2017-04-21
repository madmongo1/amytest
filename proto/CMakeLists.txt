set(PROTOS limits.proto test.proto)

protobuf_generate_cpp(PROTO_SRC PROTO_HDR ${PROTOS})


add_library(proto ${PROTO_SRC} ${PROTO_HDR})
target_include_directories(proto SYSTEM PUBLIC ${Protobuf_INCLUDE_DIRS})
