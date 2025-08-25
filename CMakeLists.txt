cmake_minimum_required(VERSION 3.10)
project(EasyKiConverter)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置策略以允许链接不在当前目录构建的目标
if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

# 设置自动处理moc、uic和rcc文件
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖库
find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)

# 查找Qt5组件
find_package(Qt5 COMPONENTS Core Widgets Gui Network REQUIRED)

# 查找zlib
find_package(ZLIB REQUIRED)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/src)

# 添加子目录
add_subdirectory(src)

# 链接Qt5和CURL库到GUI应用程序
target_link_libraries(EasyKiConverter
    gui
    easyeda
    kicad
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Network
    ${CURL_LIBRARIES}
    ${ZLIB_LIBRARIES}
)