cmake_minimum_required(VERSION 3.16)
project(tests)

# 启用测试
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用多线程编译（最多16个线程）
if(MSVC)
    # MSVC 使用 /MP 选项，设置最大并行作业数为16
    add_compile_options(/MP16)
    # 启用正确的 __cplusplus 宏值
    add_compile_options(/Zc:__cplusplus)
    # 启用严格一致性模式
    add_compile_options(/permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC 使用 -j 选项，但需要通过 make 的 -j 参数来控制
    # 设置 CMAKE_JOB_POOLS 来限制并行作业数
    set(CMAKE_JOB_POOLS "compile=16" "link=4")
    # 为所有编译作业使用 compile 池
    set_property(GLOBAL PROPERTY JOB_POOLS ${CMAKE_JOB_POOLS})
    message(STATUS "GCC compiler detected. Parallel compilation limited to 16 jobs")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang 也使用 -j 选项
    set(CMAKE_JOB_POOLS "compile=16" "link=4")
    set_property(GLOBAL PROPERTY JOB_POOLS ${CMAKE_JOB_POOLS})
    message(STATUS "Clang compiler detected. Parallel compilation limited to 16 jobs")
endif()

# 启用 AUTOMOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Test Network)

# CI 环境检测
if(DEFINED ENV{CI})
    message(STATUS "Tests: Running in CI environment")
    set(TESTS_CI_ENV TRUE)
else()
    message(STATUS "Tests: Running in local environment")
    set(TESTS_CI_ENV FALSE)
endif()

# 查找 zlib（NetworkUtils.cpp 使用 zlib API）
# 在 CI 环境中，使用 vcpkg 提供的 zlib
# 在本地环境中，使用 Qt 自带的 zlib
if(TESTS_CI_ENV)
    # CI 环境：使用 vcpkg 提供的 zlib
    find_package(ZLIB REQUIRED)
    set(ZLIB_USE_QT FALSE)
    message(STATUS "Tests: Using vcpkg zlib (CI environment)")
    message(STATUS "Tests: ZLIB_INCLUDE_DIRS = ${ZLIB_INCLUDE_DIRS}")
    message(STATUS "Tests: ZLIB_LIBRARIES = ${ZLIB_LIBRARIES}")
else()
    # 本地环境：使用 Qt 自带的 zlib
    find_package(ZLIB QUIET)
    if(NOT ZLIB_FOUND)
        message(STATUS "Tests: Using Qt's zlib library (local environment)")
        set(ZLIB_FOUND TRUE)
        set(ZLIB_LIBRARY z)
        set(ZLIB_INCLUDE_DIR "")
        set(ZLIB_USE_QT TRUE)
    else()
        set(ZLIB_USE_QT FALSE)
        message(STATUS "Tests: Found system zlib (local environment)")
    endif()
endif()

# 输出 zlib 查找结果（用于调试）
message(STATUS "Tests: ZLIB_FOUND = ${ZLIB_FOUND}")
message(STATUS "Tests: ZLIB_LIBRARY = ${ZLIB_LIBRARY}")
message(STATUS "Tests: ZLIB_USE_QT = ${ZLIB_USE_QT}")

# 设置包含路径
include_directories(${CMAKE_SOURCE_DIR}/../src)

# 在 CI 环境中，确保 vcpkg 的 include 路径被正确传递
if(TESTS_CI_ENV AND ZLIB_FOUND AND NOT ZLIB_USE_QT)
    message(STATUS "Tests: Adding vcpkg include directories")
    if(DEFINED ZLIB_INCLUDE_DIRS AND NOT "${ZLIB_INCLUDE_DIRS}" STREQUAL "")
        include_directories(${ZLIB_INCLUDE_DIRS})
        message(STATUS "Tests: Added include directories: ${ZLIB_INCLUDE_DIRS}")
    endif()
endif()

# 定义一个宏，让源文件使用正确的包含路径
add_definitions(-DTESTING_MODE)

# 辅助函数：链接 zlib（处理 Qt zlib 和 vcpkg zlib 两种情况）
function(link_zlib TARGET)
    if(ZLIB_FOUND)
        if(ZLIB_USE_QT)
            # 使用 Qt 的 zlib 库（本地 MinGW 环境）
            message(STATUS "Tests: Linking Qt zlib to ${TARGET}")
            target_link_libraries(${TARGET} PRIVATE ${ZLIB_LIBRARY})
        else()
            # 使用 vcpkg 或系统提供的 zlib（CI MSVC 环境）
            message(STATUS "Tests: Linking vcpkg zlib to ${TARGET}")
            if(TARGET ZLIB::ZLIB)
                # 使用 ZLIB::ZLIB target 会自动包含正确的 include 路径
                target_link_libraries(${TARGET} PRIVATE ZLIB::ZLIB)
                # 调试输出：检查 include 路径
                get_target_property(ZLIB_INCLUDES ZLIB::ZLIB INTERFACE_INCLUDE_DIRECTORIES)
                message(STATUS "Tests: ZLIB::ZLIB include directories = ${ZLIB_INCLUDES}")
            else()
                # 如果 ZLIB::ZLIB target 不存在，回退到使用 ZLIB_LIBRARIES
                message(STATUS "Tests: Using ZLIB_LIBRARIES directly")
                target_link_libraries(${TARGET} PRIVATE ${ZLIB_LIBRARIES})
                target_include_directories(${TARGET} PRIVATE ${ZLIB_INCLUDE_DIRS})
            endif()
        endif()
    else()
        message(WARNING "Tests: ZLIB not found, ${TARGET} may fail to compile")
    endif()
endfunction()

# ========== ComponentService 测试 ==========
add_executable(test_component_service test_component_service.cpp)

# 添加源文件
target_sources(test_component_service PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/JLCDatasheet.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
)

# 添加包含目录
target_include_directories(test_component_service PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接库（统一使用 PRIVATE 关键字风格）
target_link_libraries(test_component_service PRIVATE Qt6::Core Qt6::Test Qt6::Network)
link_zlib(test_component_service)

# 注册测试
add_test(NAME test_component_service COMMAND test_component_service)

# ========== ExportService 测试 ==========
add_executable(test_export_service test_export_service.cpp)

# 添加源文件
target_sources(test_export_service PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
)

target_include_directories(test_export_service PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 Qt Network 和 zlib（统一使用 PRIVATE 关键字风格）
target_link_libraries(test_export_service PRIVATE Qt6::Core Qt6::Test Qt6::Network)
link_zlib(test_export_service)

# 注册测试
add_test(NAME test_export_service COMMAND test_export_service)

# ========== ExportServicePipeline 测试 ==========
add_executable(test_export_service_pipeline test_export_service_pipeline.cpp)

# 添加源文件
target_sources(test_export_service_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService_Pipeline.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/FetchWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/ProcessWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/WriteWorker.cpp
)

target_include_directories(test_export_service_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_export_service_pipeline PRIVATE Qt6::Core Qt6::Test Qt6::Network)
link_zlib(test_export_service_pipeline)

# 注册测试
add_test(NAME test_export_service_pipeline COMMAND test_export_service_pipeline)
# 设置 CTest 超时时间为 120 秒（流水线测试需要较长时间）
set_property(TEST test_export_service_pipeline PROPERTY TIMEOUT 120)

# ========== ConfigService 测试 ==========
add_executable(test_config_service test_config_service.cpp)

# 添加源文件
target_sources(test_config_service PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
)

target_include_directories(test_config_service PRIVATE ${CMAKE_SOURCE_DIR}/../src)

target_link_libraries(test_config_service Qt6::Core Qt6::Test)

# 注册测试
add_test(NAME test_config_service COMMAND test_config_service)

# ========== ComponentDataCollector 测试 ==========
add_executable(test_component_data_collector test_component_data_collector.cpp)

# 添加源文件
target_sources(test_component_data_collector PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentDataCollector.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
)

target_include_directories(test_component_data_collector PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接库（统一使用 PRIVATE 关键字风格）
target_link_libraries(test_component_data_collector PRIVATE Qt6::Core Qt6::Test Qt6::Network)
link_zlib(test_component_data_collector)

# 注册测试
add_test(NAME test_component_data_collector COMMAND test_component_data_collector)
# 设置 CTest 超时时间为 60 秒（集成测试需要较长时间）
set_property(TEST test_component_data_collector PROPERTY TIMEOUT 60)

# ========== UUID 提取测试 ==========
add_executable(test_uuid_extraction test_uuid_extraction.cpp)
target_include_directories(test_uuid_extraction PRIVATE ${CMAKE_SOURCE_DIR}/../src)

target_link_libraries(test_uuid_extraction Qt6::Core)

# 注册测试
add_test(NAME test_uuid_extraction COMMAND test_uuid_extraction)

# ========== 图层映射测试 ==========
add_executable(test_layer_mapping test_layer_mapping.cpp)

# 添加源文件
target_sources(test_layer_mapping PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
)

target_include_directories(test_layer_mapping PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 Qt6::Test（重写后使用 QtTest 框架）
target_link_libraries(test_layer_mapping Qt6::Core Qt6::Test)

# 注册测试
add_test(NAME test_layer_mapping COMMAND test_layer_mapping)

# ========== 集成测试 ==========
add_executable(test_integration test_integration.cpp)

# 添加源文件
target_sources(test_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/JLCDatasheet.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
)

target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_integration PRIVATE Qt6::Core Qt6::Test Qt6::Network)
link_zlib(test_integration)

# 注册测试
add_test(NAME test_integration COMMAND test_integration)
# 设置 CTest 超时时间为 120 秒（集成测试需要较长时间）
set_property(TEST test_integration PROPERTY TIMEOUT 120)

# ========== 性能测试 ==========
add_executable(test_performance test_performance.cpp)

# 添加源文件
target_sources(test_performance PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
)

target_include_directories(test_performance PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_performance PRIVATE Qt6::Core Qt6::Test Qt6::Network)
link_zlib(test_performance)

# 注册测试
add_test(NAME test_performance COMMAND test_performance)
# 设置 CTest 超时时间为 180 秒（性能测试需要较长时间）
set_property(TEST test_performance PROPERTY TIMEOUT 180)

# ========== 流水线性能基准测试 ==========
add_executable(test_pipeline_baseline test_pipeline_baseline.cpp)

# 添加源文件
target_sources(test_pipeline_baseline PRIVATE
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ExportService_Pipeline.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ConfigService.cpp
    ${CMAKE_SOURCE_DIR}/../src/services/ComponentExportTask.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/ComponentData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/SymbolData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/FootprintData.cpp
    ${CMAKE_SOURCE_DIR}/../src/models/Model3DData.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaApi.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/easyeda/EasyedaImporter.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterSymbol.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/ExporterFootprint.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/kicad/Exporter3DModel.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/GeometryUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/LayerMapper.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/NetworkUtils.cpp
    ${CMAKE_SOURCE_DIR}/../src/core/utils/SvgPathParser.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/FetchWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/ProcessWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/workers/WriteWorker.cpp
    ${CMAKE_SOURCE_DIR}/../src/utils/BoundedThreadSafeQueue.h
)

target_include_directories(test_pipeline_baseline PRIVATE ${CMAKE_SOURCE_DIR}/../src)

# 链接 zlib
target_link_libraries(test_pipeline_baseline PRIVATE Qt6::Core Qt6::Test Qt6::Network)
link_zlib(test_pipeline_baseline)

# 注册测试
add_test(NAME test_pipeline_baseline COMMAND test_pipeline_baseline)
# 设置 CTest 超时时间为 60 秒（流水线模式下20秒不到就能导出）
set_property(TEST test_pipeline_baseline PROPERTY TIMEOUT 60)