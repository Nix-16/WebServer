cmake_minimum_required(VERSION 3.5)
project(WebServer)

# C++标准，若用C++11或以上
set(CMAKE_CXX_STANDARD 17)

# 如果你想开启调试信息
set(CMAKE_BUILD_TYPE Debug)

# 把资源目录路径保存下来(可选)
set(RESOURCE_DIR ${PROJECT_SOURCE_DIR}/resources)

# 寻找 MySQL client 库
# 方式 1：如果系统自带 FindMySQLclient.cmake，可尝试 find_package(mysqlclient REQUIRED)
# 方式 2：手动 link_libraries(-lmysqlclient) 或 target_link_libraries(... mysqlclient)
# 这里演示手动链接:
find_library(MYSQLCLIENT_LIB mysqlclient)
if(NOT MYSQLCLIENT_LIB)
    message(FATAL_ERROR "Cannot find libmysqlclient, please install libmysqlclient-dev")
endif()

# 包含头文件目录(如 code 下有子目录)
# 你可以根据自己的结构自行添加
include_directories(
    ${PROJECT_SOURCE_DIR}/code
    ${PROJECT_SOURCE_DIR}/code/buffer
    ${PROJECT_SOURCE_DIR}/code/epoll
    ${PROJECT_SOURCE_DIR}/code/http
    ${PROJECT_SOURCE_DIR}/code/pool
    ${PROJECT_SOURCE_DIR}/code/reactor
    ${PROJECT_SOURCE_DIR}/code/server
    ${PROJECT_SOURCE_DIR}/code/config
    ${PROJECT_SOURCE_DIR}/code/log
)

# 收集所有 .cpp 文件
file(GLOB_RECURSE ALL_SRCS
    "${PROJECT_SOURCE_DIR}/code/*.cpp"
)

# 生成可执行文件 webserver
add_executable(webserver ${ALL_SRCS})

# 链接 MySQLclient 以及 pthread、dl 等常见库
target_link_libraries(webserver
    PRIVATE
    ${MYSQLCLIENT_LIB}           # -lmysqlclient
    pthread
)

# 如果需要链接其他库(如 ssl, crypto), 也可以加:
# target_link_libraries(webserver PRIVATE ssl crypto)

# 可选：把资源目录传给编译器做预处理宏
target_compile_definitions(webserver PRIVATE RESOURCE_DIR="${RESOURCE_DIR}")

# 打印一些提示
message(STATUS "MYSQLCLIENT_LIB = ${MYSQLCLIENT_LIB}")

