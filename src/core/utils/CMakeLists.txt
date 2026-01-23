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

# 链接 zlib
if(ZLIB_FOUND)
    if(ZLIB_USE_QT)
        # 对于 Qt 的 zlib，链接 Qt6::Core 会自动提供 zlib 支持
        # 但仍然需要显式链接 zlib 库
        target_link_libraries(EasyKiConverterUtils PUBLIC z)
        # 添加 Qt 的 include 路径以获取 zlib.h
        target_include_directories(EasyKiConverterUtils PRIVATE ${CMAKE_PREFIX_PATH}/include)
    else()
        target_link_libraries(EasyKiConverterUtils PUBLIC ZLIB::ZLIB)
    endif()
else()
    target_link_libraries(EasyKiConverterUtils PUBLIC z)
endif()