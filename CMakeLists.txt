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
find_package(Boost COMPONENTS ${boostComponents} REQUIRED)

hunter_add_package(MySQL-client)
find_package(MySQL-client REQUIRED)

include_directories(SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/amy/include")


set(SOURCE_FILES src/main.cpp src/config.hpp)

add_executable(amy-test ${SOURCE_FILES})
target_link_libraries(amy-test ${MYSQL-CLIENT_LIBRARY} ${Boost_LIBRARIES})
target_include_directories(amy-test SYSTEM PRIVATE ${MYSQL-CLIENT_ROOT} ${Boost_INCLUDE_DIRS})
target_compile_definitions(amy-test PUBLIC USE_BOOST_ASIO=1)
