# EasyKiConverter CMakeLists.txt

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找所需的库
find_package(PkgConfig REQUIRED)
find_package(CURL REQUIRED)
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# 查找zlib
find_package(ZLIB REQUIRED)

# 添加子目录
add_subdirectory(easyeda)
add_subdirectory(kicad)
add_subdirectory(gui)

# 创建可执行文件 - GUI版本
add_executable(easykiconverter
    gui_main.cpp
)

# 链接库 - GUI版本
target_link_libraries(easykiconverter
    easyeda
    kicad
    gui
    ${CURL_LIBRARIES}
    ZLIB::ZLIB
    Qt5::Core
    Qt5::Widgets
)

# 设置输出目录
set_target_properties(easykiconverter
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)