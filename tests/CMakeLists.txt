cmake_minimum_required(VERSION 3.16)
project(test_uuid_extraction)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core)

# 添加可执行文件
add_executable(test_uuid_extraction test_uuid_extraction.cpp)

# 链接 Qt 库
target_link_libraries(test_uuid_extraction Qt6::Core)