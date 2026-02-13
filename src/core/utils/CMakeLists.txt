# Utils 模块

add_library(EasyKiConverterUtils STATIC
    GeometryUtils.cpp
    NetworkUtils.cpp
    INetworkAdapter.cpp
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
        # 使用 Qt 的 zlib 库（本地 MinGW 环境）
        target_link_libraries(EasyKiConverterUtils PUBLIC ${ZLIB_LIBRARY})
    else()
        # 使用 vcpkg 或系统提供的 zlib（CI MSVC 环境）
        # 使用 ZLIB::ZLIB target 会自动包含正确的 include 路径
        if(TARGET ZLIB::ZLIB)
            target_link_libraries(EasyKiConverterUtils PUBLIC ZLIB::ZLIB)
        else()
            # 如果 ZLIB::ZLIB target 不存在，回退到使用 ZLIB_LIBRARIES
            target_link_libraries(EasyKiConverterUtils PUBLIC ${ZLIB_LIBRARIES})
            target_include_directories(EasyKiConverterUtils PRIVATE ${ZLIB_INCLUDE_DIRS})
        endif()
    endif()
endif()
