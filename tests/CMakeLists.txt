# EasyKiConverter 测试模块

# 查找测试所需的 Qt 组件
find_package(Qt6 REQUIRED COMPONENTS Test QuickTest)

# 添加子目录
add_subdirectory(unit)
# add_subdirectory(integration)
# add_subdirectory(benchmark)

# 打印测试配置信息
message(STATUS "Test project initialized with Qt6::Test and Qt6::QuickTest")
