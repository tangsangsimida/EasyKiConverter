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

        # 对于 MSVC，需要显式添加 zlib.h 的路径
        if(MSVC)
            # MSVC 下 Qt 的 zlib.h 可能在这些位置
            find_path(ZLIB_INCLUDE_DIR
                NAMES zlib.h
                PATHS
                    "${CMAKE_PREFIX_PATH}/include"
                    "${CMAKE_PREFIX_PATH}/include/QtCore"
                    "${CMAKE_PREFIX_PATH}/../Tools/mingw*/include"
                NO_DEFAULT_PATH
            )
            if(ZLIB_INCLUDE_DIR)
                message(STATUS "EasyKiConverterUtils: Found zlib.h at: ${ZLIB_INCLUDE_DIR}")
                target_include_directories(EasyKiConverterUtils PRIVATE ${ZLIB_INCLUDE_DIR})
            else()
                message(WARNING "EasyKiConverterUtils: Could not find zlib.h in Qt installation")
            endif()
        endif()
    else()
        target_link_libraries(EasyKiConverterUtils PUBLIC ZLIB::ZLIB)
    endif()
else()
    target_link_libraries(EasyKiConverterUtils PUBLIC z)
endif()