cmake_minimum_required(VERSION 3.6)
include("cmake/HunterGate.cmake")

set(CMAKE_CXX_STANDARD 14)

HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.18.43.tar.gz"
        SHA1 "d2c8c42cd07f7cefe18fd9a9b9c13114b1a15a27"
        LOCAL
)
project(AmyTest)


set (boostComponents system)
hunter_add_package(Boost COMPONENTS ${boostComponents})
find_package(Threads)
find_package(Boost COMPONENTS ${boostComponents} REQUIRED)

hunter_add_package(MySQL-client)
find_package(MySQL-client REQUIRED)

hunter_add_package(Protobuf)
find_package(Protobuf REQUIRED)

hunter_add_package(libsodium)
find_package(libsodium REQUIRED)

include_directories(SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/amy/include")

add_subdirectory(proto)


set(SOURCE_FILES src/main.cpp src/config.hpp
        src/base64.cpp src/base64.hpp
        src/hasher.cpp src/hasher.hpp
        src/notstd.hpp
        src/table_lookup.cpp src/table_lookup.hpp
        src/sql_escaper.cpp src/sql_escaper.hpp
        src/query_builder.cpp src/query_builder.hpp)

add_executable(amy-test ${SOURCE_FILES})
target_link_libraries(amy-test proto libsodium::libsodium ${MYSQL-CLIENT_LIBRARY} ${Boost_LIBRARIES} ${Protobuf_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(amy-test SYSTEM PRIVATE ${MYSQL-CLIENT_ROOT} ${Boost_INCLUDE_DIRS} ${Protobuf_INCLUDE_DIRS})
target_include_directories(amy-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(amy-test PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src)
target_include_directories(amy-test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(amy-test PUBLIC USE_BOOST_ASIO=1)
