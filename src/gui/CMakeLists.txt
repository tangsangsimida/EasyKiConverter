# 查找Qt5组件
find_package(Qt5 COMPONENTS Core Widgets Gui Network REQUIRED)

# 设置自动处理moc、uic和rcc文件
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 显式指定需要moc处理的文件
set(GUI_SOURCES
    MainWindow.cpp
)

set(GUI_HEADERS
    MainWindow.h
)

# 添加库
add_library(gui
    ${GUI_SOURCES}
    ${GUI_HEADERS}
)

# 链接库
target_link_libraries(gui
    easyeda
    kicad
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    Qt5::Network
)

# 包含目录
target_include_directories(gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)