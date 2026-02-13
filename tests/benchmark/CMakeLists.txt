
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_benchmark_core
    test_benchmark_core.cpp
)

target_link_libraries(test_benchmark_core PRIVATE
    Qt6::Test
    EasyKiConverterLib
)

target_include_directories(test_benchmark_core PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME test_benchmark_core COMMAND test_benchmark_core)
