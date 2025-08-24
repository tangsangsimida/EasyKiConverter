# 创建easyeda库
add_library(easyeda
    EasyedaApi.cpp
    EasyedaImporter.cpp
)

# 查找zlib
find_package(ZLIB REQUIRED)

# 链接所需的库
target_link_libraries(easyeda
    ${CURL_LIBRARIES}
    ZLIB::ZLIB
)

# 包含目录
target_include_directories(easyeda PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)