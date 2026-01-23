cmake_minimum_required(VERSION 3.16)

# ──────────────────────────────────────────────
# MinGW 环境特殊处理
# ──────────────────────────────────────────────
# MinGW 环境下禁用 vcpkg toolchain，因为 vcpkg 默认使用 x64-windows triplet（需要 MSVC）
# 会导致 "Unable to find a valid Visual Studio instance" 错误
if(MINGW)
    message(STATUS "MinGW environment detected. Disabling vcpkg toolchain.")
    # 强制清除 vcpkg toolchain 文件（包括环境变量和缓存）
    unset(CMAKE_TOOLCHAIN_FILE CACHE)
    unset(CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "" CACHE FILEPATH "Toolchain file" FORCE)
    # 清除 VCPKG_ROOT 环境变量
    unset(ENV{VCPKG_ROOT})
    unset(VCPKG_ROOT CACHE)
    message(STATUS "CMAKE_TOOLCHAIN_FILE cleared: ${CMAKE_TOOLCHAIN_FILE}")
endif()

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
    # 启用严格一致性模式
    add_compile_options(/permissive-)
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

# 查找 zlib 库（NetworkUtils.cpp 使用 zlib API）
# MSVC: 通过 vcpkg 安装 zlib
# MinGW: 通过 MSYS2 安装 mingw-w64-x86_64-zlib 或 Strawberry Perl
# Linux/macOS: 系统自带 zlib
if(MINGW)
    # MinGW 环境下，添加常见的 zlib 安装路径
    # Strawberry Perl (GitHub Actions runner 自带)
    list(APPEND CMAKE_PREFIX_PATH "C:/Strawberry/c")
    # MSYS2
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
    # Qt 自带的 zlib
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}")
endif()
find_package(ZLIB REQUIRED)

message(STATUS "ZLIB_FOUND: ${ZLIB_FOUND}")
message(STATUS "ZLIB_INCLUDE_DIRS: ${ZLIB_INCLUDE_DIRS}")
message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")

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

# 创建可执行文件（包含 main.cpp）—— 注意：不再使用 WIN32
qt_add_executable(appEasyKiconverter_Cpp_Version
    main.cpp
)

# ──────────────────────────────────────────────
# MinGW 专用修复
# ──────────────────────────────────────────────
if(MINGW)
    set_target_properties(appEasyKiconverter_Cpp_Version PROPERTIES
        QT_NO_ENTRYPOINT            ON
    )
    set_target_properties(appEasyKiconverter_Cpp_Version PROPERTIES
        WIN32_EXECUTABLE            TRUE
    )
    target_link_options(appEasyKiconverter_Cpp_Version PRIVATE
        "-mwindows"
        "-Wl,--exclude-libs=libQt6EntryPoint.a"
        "-Wl,--allow-multiple-definition"
    )
    target_link_libraries(appEasyKiconverter_Cpp_Version
        PRIVATE
            mingw32
            EasyKiConverterLib
            Qt6::Quick
            Qt6::Network
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::QuickControls2
            ZLIB::ZLIB
    )
endif()

# 添加 src 目录到 include 路径
target_include_directories(appEasyKiconverter_Cpp_Version
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接库（非 MinGW 平台）
if(NOT MINGW)
    target_link_libraries(appEasyKiconverter_Cpp_Version
        PRIVATE
            EasyKiConverterLib
            Qt6::Quick
            Qt6::Network
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::QuickControls2
            ZLIB::ZLIB
    )
endif()

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
if(MSVC)
    set_target_properties(appEasyKiconverter_Cpp_Version PROPERTIES
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "EasyKiConverter"
    )
else()
    set_target_properties(appEasyKiconverter_Cpp_Version PROPERTIES
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "EasyKiConverter"
    )
endif()

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
