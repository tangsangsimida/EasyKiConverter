find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_fixture_to_kicad_export
    test_fixture_to_kicad_export.cpp
    ../common/TestPaths.hpp
)

target_link_libraries(test_fixture_to_kicad_export PRIVATE
    Qt6::Test
    EasyKiConverterLib
)

target_include_directories(test_fixture_to_kicad_export PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

add_test(NAME test_fixture_to_kicad_export COMMAND test_fixture_to_kicad_export)

set_tests_properties(test_fixture_to_kicad_export
    PROPERTIES
        LABELS "integration"
)
