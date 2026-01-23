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

# 查找 zlib（NetworkUtils.cpp 使用 zlib API）
find_package(ZLIB QUIET)

# 输出 zlib 查找结果（用于调试）
message(STATUS "ZLIB_FOUND: ${ZLIB_FOUND}")
message(STATUS "ZLIB_INCLUDE_DIRS: ${ZLIB_INCLUDE_DIRS}")
message(STATUS "ZLIB_LIBRARIES: ${ZLIB_LIBRARIES}")
message(STATUS "ZLIB_VERSION: ${ZLIB_VERSION}")

# 设置包含路径
include_directories(${CMAKE_SOURCE_DIR}/../src)

# 定义一个宏，让源文件使用正确的包含路径
add_definitions(-DTESTING_MODE)

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
if(ZLIB_FOUND)
    target_link_libraries(test_component_service PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_component_service PRIVATE z)
endif()

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
if(ZLIB_FOUND)
    target_link_libraries(test_export_service PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_export_service PRIVATE z)
endif()

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
if(ZLIB_FOUND)
    target_link_libraries(test_export_service_pipeline PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_export_service_pipeline PRIVATE z)
endif()

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
if(ZLIB_FOUND)
    target_link_libraries(test_component_data_collector PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_component_data_collector PRIVATE z)
endif()

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

target_link_libraries(test_layer_mapping Qt6::Core)

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
if(ZLIB_FOUND)
    target_link_libraries(test_integration PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_integration PRIVATE z)
endif()

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
if(ZLIB_FOUND)
    target_link_libraries(test_performance PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_performance PRIVATE z)
endif()

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
if(ZLIB_FOUND)
    target_link_libraries(test_pipeline_baseline PRIVATE ZLIB::ZLIB)
else()
    target_link_libraries(test_pipeline_baseline PRIVATE z)
endif()

# 注册测试
add_test(NAME test_pipeline_baseline COMMAND test_pipeline_baseline)
# 设置 CTest 超时时间为 60 秒（流水线基准测试使用虚拟 ID）
set_property(TEST test_pipeline_baseline PROPERTY TIMEOUT 60)