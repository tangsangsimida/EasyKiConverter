find_package(Qt6 REQUIRED COMPONENTS Test Gui Network)

add_executable(test_easyeda_api
    test_easyeda_api.cpp
    ../common/MockNetworkClient.hpp
)
target_link_libraries(test_easyeda_api PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)

# 确保能找到项目根目录下的包含路径
target_include_directories(test_easyeda_api PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME test_easyeda_api COMMAND test_easyeda_api)

add_executable(test_models test_models.cpp)
target_link_libraries(test_models PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_models PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_models COMMAND test_models)

add_executable(test_component_cache_service test_component_cache_service.cpp)
target_link_libraries(test_component_cache_service PRIVATE
    Qt6::Test
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_component_cache_service PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_component_cache_service COMMAND test_component_cache_service)

# 导出阶段测试
add_executable(test_export_type_stage test_export_type_stage.cpp)
target_link_libraries(test_export_type_stage PRIVATE
    Qt6::Test
    EasyKiConverterLib
)
target_include_directories(test_export_type_stage PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_type_stage COMMAND test_export_type_stage)

add_executable(test_export_progress_viewmodel test_export_progress_viewmodel.cpp)
target_link_libraries(test_export_progress_viewmodel PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Network
    EasyKiConverterLib
)
target_include_directories(test_export_progress_viewmodel PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_export_progress_viewmodel COMMAND test_export_progress_viewmodel)

# 日志系统测试
add_executable(test_logging test_logging.cpp)
target_link_libraries(test_logging PRIVATE
    Qt6::Test
    EasyKiConverterLogging
    EasyKiConverterLib
)
target_include_directories(test_logging PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_logging COMMAND test_logging)

# CLI 相关测试
add_executable(test_command_line_parser test_command_line_parser.cpp)
target_link_libraries(test_command_line_parser PRIVATE
    Qt6::Test
    EasyKiConverterLib
    EasyKiConverter::Cli
)
target_include_directories(test_command_line_parser PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_command_line_parser COMMAND test_command_line_parser)

add_executable(test_completion_generator test_completion_generator.cpp)
target_link_libraries(test_completion_generator PRIVATE
    Qt6::Test
    EasyKiConverter::Cli
    EasyKiConverterLib
)
target_include_directories(test_completion_generator PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_completion_generator COMMAND test_completion_generator)

add_executable(test_file_reader test_file_reader.cpp)
target_link_libraries(test_file_reader PRIVATE
    Qt6::Test
    EasyKiConverter::Cli
    EasyKiConverterLib
)
target_include_directories(test_file_reader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)
add_test(NAME test_file_reader COMMAND test_file_reader)
