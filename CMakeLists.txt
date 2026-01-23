cmake_minimum_required(VERSION 3.16)

project(EasyKiconverter_Cpp_Version VERSION 0.1 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 启用多线程编译
if(MSVC)
    # MSVC 使用 /MP 选项，自动使用最大可用线程
    add_compile_options(/MP)
    # 启用正确的 __cplusplus 宏值
    add_compile_options(/Zc:__cplusplus)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC 使用 -j 选项，但需要通过 make 的 -j 参数来控制
    # 这里不添加编译选项，而是在构建时使用 make -j
    message(STATUS "GCC compiler detected. Use 'make -j' or 'cmake --build . --parallel' for parallel compilation")
endif()

# 调试选项：启用符号和封装解析数据导出
option(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT "Enable debug export for symbol and footprint data" ON)

if(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
    message(STATUS "Symbol and footprint debug export is ENABLED")
    add_compile_definitions(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
else()
    message(STATUS "Symbol and footprint debug export is DISABLED")
endif()

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找 Qt 6 组件
find_package(Qt6 REQUIRED COMPONENTS Quick Network Core Gui Widgets QuickControls2 Concurrent)

# 尝试查找 zlib 库
# 在 Windows + MinGW 环境下，Qt 的 zlib 库通常在 Qt 安装目录中
find_package(ZLIB QUIET)

# 如果找不到标准的 zlib，使用 Qt 的 zlib 库
if(NOT ZLIB_FOUND)
    # 使用 Qt 的 zlib 库（在 MinGW 环境下通常可用）
    message(STATUS "Using Qt's zlib library")
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIR "")
    set(ZLIB_LIBRARY z)
endif()

message(STATUS "ZLIB_FOUND: ${ZLIB_FOUND}")
message(STATUS "ZLIB_LIBRARY: ${ZLIB_LIBRARY}")

qt_standard_project_setup(REQUIRES 6.8)

# QML 文件列表
set(QML_FILES
    Main.qml
    src/ui/qml/MainWindow.qml
    src/ui/qml/components/Card.qml
    src/ui/qml/components/ComponentListItem.qml
    src/ui/qml/components/ResultListItem.qml
    src/ui/qml/styles/AppStyle.qml
)

# 添加源码模块
add_subdirectory(src)

# 创建可执行文件（包含 main.cpp）
qt_add_executable(appEasyKiconverter_Cpp_Version
    main.cpp
)

# 添加 src 目录到 include 路径
target_include_directories(appEasyKiconverter_Cpp_Version
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接主源码库
target_link_libraries(appEasyKiconverter_Cpp_Version
    PRIVATE
        EasyKiConverterLib
        Qt6::Quick
        Qt6::Network
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::QuickControls2
        ${ZLIB_LIBRARY}
)

# 添加 QML 模块
qt_add_qml_module(appEasyKiconverter_Cpp_Version
    URI EasyKiconverter_Cpp_Version
    VERSION 1.0
    QML_FILES
        ${QML_FILES}
        src/ui/qml/components/Icon.qml
        src/ui/qml/components/ModernButton.qml
    RESOURCES
        resources/icons/app_icon.icns
        resources/icons/app_icon.ico
        resources/icons/app_icon.png
        resources/icons/app_icon.svg
        resources/icons/play.svg
        resources/icons/folder.svg
        resources/icons/trash.svg
        resources/icons/upload.svg
        resources/icons/add.svg
        resources/icons/loading.svg
        resources/icons/github-mark.svg
        resources/icons/github-mark-white.svg
        resources/icons/github-mark.png
        resources/icons/github-mark-white.png
        resources/icons/Blue_light_bulb.svg
        resources/icons/Grey_light_bulb.svg
        resources/imgs/background.jpg
        src/ui/qml/styles/qmldir
)

# 设置目标属性
set_target_properties(appEasyKiconverter_Cpp_Version PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "EasyKiConverter"
)

# 安装规则
include(GNUInstallDirs)
install(TARGETS appEasyKiconverter_Cpp_Version
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt version: ${Qt6_VERSION}")

# 生成 compile_commands.json 用于 VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)