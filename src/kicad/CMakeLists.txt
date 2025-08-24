# 创建kicad库
add_library(kicad
    KicadSymbolExporter.cpp
)

# 链接所需的库
target_link_libraries(kicad
    # nlohmann_json::nlohmann_json  # 移除这个引用
)

# 包含目录
target_include_directories(kicad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)