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

# 源文件列表
set(SOURCES
    main.cpp
    src/models/ComponentData.cpp
    src/models/SymbolData.cpp
    src/models/FootprintData.cpp
    src/models/Model3DData.cpp
    src/core/utils/GeometryUtils.cpp
    src/core/utils/NetworkUtils.cpp
    src/core/utils/LayerMapper.cpp
    src/core/utils/SvgPathParser.cpp
    src/ui/utils/ConfigManager.cpp
    src/core/easyeda/EasyedaApi.cpp
    src/core/easyeda/EasyedaImporter.cpp
    src/core/easyeda/JLCDatasheet.cpp
    src/core/kicad/ExporterSymbol.cpp
    src/core/kicad/ExporterFootprint.cpp
    src/core/kicad/Exporter3DModel.cpp
    src/workers/ExportWorker.cpp
    src/workers/NetworkWorker.cpp
    # 流水线架构 Workers
    src/workers/FetchWorker.cpp
    src/workers/ProcessWorker.cpp
    src/workers/WriteWorker.cpp
    # Service 层
    src/services/ComponentService.cpp
    src/services/ExportService.cpp
    src/services/ExportService_Pipeline.cpp
    src/services/ConfigService.cpp
    src/services/ComponentDataCollector.cpp
    src/services/ComponentExportTask.cpp
    # ViewModel 层
    src/ui/viewmodels/ComponentListViewModel.cpp
    src/ui/viewmodels/ExportSettingsViewModel.cpp
    src/ui/viewmodels/ExportProgressViewModel.cpp
    src/ui/viewmodels/ThemeSettingsViewModel.cpp
)

# 头文件列表
set(HEADERS
    src/models/ComponentData.h
    src/models/SymbolData.h
    src/models/FootprintData.h
    src/models/Model3DData.h
    src/models/ComponentExportStatus.h
    src/core/utils/GeometryUtils.h
    src/core/utils/NetworkUtils.h
    src/core/utils/LayerMapper.h
    src/core/utils/SvgPathParser.h
    src/utils/BoundedThreadSafeQueue.h
    src/ui/utils/ConfigManager.h
    src/core/easyeda/EasyedaApi.h
    src/core/easyeda/EasyedaImporter.h
    src/core/easyeda/JLCDatasheet.h
    src/core/kicad/ExporterSymbol.h
    src/core/kicad/ExporterFootprint.h
    src/core/kicad/Exporter3DModel.h
    src/workers/ExportWorker.h
    src/workers/NetworkWorker.h
    # 流水线架构 Workers
    src/workers/FetchWorker.h
    src/workers/ProcessWorker.h
    src/workers/WriteWorker.h
    # Service 层
    src/services/ComponentService.h
    src/services/ExportService.h
    src/services/ExportService_Pipeline.h
    src/services/ConfigService.h
    src/services/ComponentDataCollector.h
    src/services/ComponentExportTask.h
    # ViewModel 层
    src/ui/viewmodels/ComponentListViewModel.h
    src/ui/viewmodels/ExportSettingsViewModel.h
    src/ui/viewmodels/ExportProgressViewModel.h
    src/ui/viewmodels/ThemeSettingsViewModel.h
)

# QML 文件列表
set(QML_FILES
    Main.qml
    src/ui/qml/MainWindow.qml
    src/ui/qml/components/Card.qml
    src/ui/qml/components/ComponentListItem.qml
    src/ui/qml/components/ResultListItem.qml
    src/ui/qml/styles/AppStyle.qml
)

# 创建可执行文件
qt_add_executable(appEasyKiconverter_Cpp_Version
    ${SOURCES}
    ${HEADERS}
)

# 添加编译定义
if(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
    message(STATUS "Symbol and footprint debug export is ENABLED")
    target_compile_definitions(appEasyKiconverter_Cpp_Version PRIVATE ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT)
else()
    message(STATUS "Symbol and footprint debug export is DISABLED")
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
set_target_properties(appEasyKiconverter_Cpp_Version PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "EasyKiConverter"
)

# 链接 Qt 库
target_link_libraries(appEasyKiconverter_Cpp_Version
    PRIVATE
        Qt6::Quick
        Qt6::Network
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::QuickControls2
        ${ZLIB_LIBRARY}
)

# 包含目录
target_include_directories(appEasyKiconverter_Cpp_Version
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/models
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers
)

# 编译选项
if(MSVC)
    target_compile_options(appEasyKiconverter_Cpp_Version PRIVATE /W4)
else()
    target_compile_options(appEasyKiconverter_Cpp_Version PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# Installation Rules (Deployment)
# ==============================================================================

# 1. 安装主程序
install(TARGETS appEasyKiconverter_Cpp_Version
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION bin # 安装到 bin 目录
)

# 2. 安装资源文件
install(FILES "resources/icons/app_icon.png" DESTINATION "resources/icons")

# 3. Windows 专用部署逻辑
if(WIN32)
    # DLL 位置
    get_filename_component(COMPILER_BIN_PATH ${CMAKE_CXX_COMPILER} DIRECTORY)

    # 定义需要拷贝的 MinGW DLL 列表
    set(MINGW_LIBS
        "libgcc_s_seh-1.dll"
        "libstdc++-6.dll"
        "libwinpthread-1.dll"
    )

    foreach(LIB ${MINGW_LIBS})
        find_file(PATH_${LIB} NAMES ${LIB} HINTS ${COMPILER_BIN_PATH} NO_DEFAULT_PATH)
        if(PATH_${LIB})
            message(STATUS "Found MinGW library: ${PATH_${LIB}}")
            install(FILES "${PATH_${LIB}}" DESTINATION bin)
        else()
            message(WARNING "Could not find MinGW library: ${LIB}. You may need to copy it manually.")
        endif()
    endforeach()

    # 3.2 在安装时运行 windeployqt
    # 这确保了 CPack 打包时，Qt 的依赖已经被拷贝到了安装目录
    # 注意：这里使用 install(CODE) 在安装阶段执行命令
    find_package(Qt6 COMPONENTS Core REQUIRED)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        message(STATUS "Found windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

        # 在 CPack 生成临时安装目录后，对该目录下的 exe 运行 windeployqt
        install(CODE "
            message(STATUS \"Running windeployqt...\")
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                        --no-compiler-runtime # 我们上面已经手动处理了编译器运行时
                        --qmldir \"${CMAKE_CURRENT_SOURCE_DIR}/src/ui/qml\"
                        --dir \"\${CMAKE_INSTALL_PREFIX}/bin\"
                        \"\${CMAKE_INSTALL_PREFIX}/bin/EasyKiConverter.exe\"
            )
        ")
    else()
        message(WARNING "windeployqt not found! The package will likely happen to fail.")
    endif()
endif()

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt version: ${Qt6_VERSION}")

# 生成 compile_commands.json 用于 VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==============================================================================
# Tests (可选)
# ==============================================================================
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Tests enabled")
endif()

# ==============================================================================
# CPack Configuration (Packaging)
# ==============================================================================

# 1. 设置包的基本信息
set(CPACK_PACKAGE_NAME "EasyKiConverter")
set(CPACK_PACKAGE_VENDOR "EasyKiConverter Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Electronic component converter for KiCad (LCSC/EasyEDA to KiCad)")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/tangsangsimida/EasyKiConverter_QT")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "EasyKiConverter") # 安装路径: C:/Program Files/EasyKiConverter
set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.ico") # 通用图标

# 2. 选择打包生成器
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP") # Windows: 生成安装程序 (.exe) 和 压缩包 (.zip)

    # NSIS 特定配置 (Windows 安装程序)
    set(CPACK_NSIS_DISPLAY_NAME "EasyKiConverter QT") # 控制面板中显示的名称
    set(CPACK_NSIS_PACKAGE_NAME "EasyKiConverter")
    set(CPACK_NSIS_HELP_LINK "https://github.com/tangsangsimida/EasyKiConverter_QT")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/tangsangsimida/EasyKiConverter_QT")
    set(CPACK_NSIS_CONTACT "tangsangsimida")
    set(CPACK_NSIS_MODIFY_PATH ON) # 允许用户将安装路径添加到系统 PATH
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

    # 设置安装程序的图标 (必须是 .ico 格式)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app_icon.ico")

    # 创建开始菜单快捷方式
    # 格式: "目标文件名;快捷方式名称"
    set(CPACK_NSIS_MENU_LINKS
        "bin/EasyKiConverter.exe" "EasyKiConverter"
    )

elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop") # macOS: 生成 .dmg
    set(CPACK_DMG_VOLUME_NAME "EasyKiConverter")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/mac/dmg_setup.scpt") # 可选：自定义 dmg 布局
else()
    set(CPACK_GENERATOR "TGZ;DEB") # Linux: 生成 .tar.gz 和 .deb
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "tangsangsimida")
endif()

# 3. 包含 CPack 模块 (必须放在最后)
include(CPack)
