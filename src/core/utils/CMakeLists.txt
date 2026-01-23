# Utils 模块

add_library(EasyKiConverterUtils STATIC
    GeometryUtils.cpp
    NetworkUtils.cpp
    LayerMapper.cpp
    SvgPathParser.cpp
)

target_include_directories(EasyKiConverterUtils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterUtils
    PUBLIC
        Qt6::Core
        Qt6::Network
)