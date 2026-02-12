find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_easyeda_api
    test_easyeda_api.cpp
    ../common/MockNetworkAdapter.hpp
)
target_link_libraries(test_easyeda_api PRIVATE
    Qt6::Test
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
