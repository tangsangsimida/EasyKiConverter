# 调试数据导出功能使用指南

## 功能说明

本功能通过宏定义 ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT 控制，启用后会在导出过程中自动保存以下调试数据到导出目录的 debug_data 文件夹中：

### 保存的调试数据

对于每个转换的元器件，会在 debug_data/{元器件ID}/ 目录下创建以下文件：

#### 1. 原始数据
- raw_cad_data.json - 从 EasyEDA API 获取的原始 JSON 数据

#### 2. 解析数据（摘要）
- parsed_symbol_data.txt - 解析后的符号数据摘要信息
- parsed_footprint_data.txt - 解析后的封装数据摘要信息

#### 3. 解析数据（详细 JSON）
- parsed_symbol_data_detail.json - 符号数据的完整详细信息（JSON 格式）
- parsed_footprint_data_detail.json - 封装数据的完整详细信息（JSON 格式）

#### 4. 导出数据
- exported_symbol_export.kicad_sym - 导出的 KiCad 符号库文件内容
- exported_footprint_export.kicad_mod - 导出的 KiCad 封装文件内容

## 启用方法

### 方法一：使用 CMake 命令行

bash
# 在配置项目时添加 -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON 选项
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON

# 编译项目
cmake --build . --config Debug


### 方法二：使用 Qt Creator

1. 打开 Qt Creator
2. 点击左侧的 "Projects" 按钮
3. 选择 "Build & Run" → "Build"
4. 在 "CMake arguments" 中添加：
   
   -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
   
5. 点击 "Apply Configuration Changes"
6. 重新编译项目

### 方法三：修改 CMakeLists.txt

在 CMakeLists.txt 文件中找到以下行：

cmake
option(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT "Enable debug export for symbol and footprint data" OFF)


将 OFF 改为 ON：

cmake
option(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT "Enable debug export for symbol and footprint data" ON)


然后重新配置和编译项目。

## 使用示例

### 启用调试导出后转换元器件

1. 启用调试导出功能并编译项目
2. 运行应用程序
3. 输入元器件编号（例如：C596524）
4. 选择输出路径（例如：D:/output）
5. 点击"开始转换"
6. 转换完成后，在输出目录下会生成 debug_data 文件夹

### 目录结构示例


D:/output/
 easyeda_convertlib.kicad_sym          # 符号库文件
 easyeda_convertlib.pretty/            # 封装库文件夹
    C596524.kicad_mod                 # 封装文件
 easyeda_convertlib.3dshapes/          # 3D 模型文件夹
    C596524.wrl                       # 3D 模型文件
 debug_data/                           # 调试数据文件夹
     C596524/                          # 元器件 ID
         raw_cad_data.json             # 原始 JSON 数据
         parsed_symbol_data.txt        # 解析的符号数据（摘要）
         parsed_symbol_data_detail.json # 解析的符号数据（详细 JSON）
         parsed_footprint_data.txt     # 解析的封装数据（摘要）
         parsed_footprint_data_detail.json # 解析的封装数据（详细 JSON）
         exported_symbol_export.kicad_sym  # 导出的符号内容
         exported_footprint_export.kicad_mod # 导出的封装内容


## 调试数据内容说明

### raw_cad_data.json

包含从 EasyEDA API 获取的完整 JSON 数据，包括：
- dataStr.head.c_para - 符号信息
- dataStr.shape - 几何图形数据
- dataStr.head.x/y - 边界框坐标
- 其他 API 返回的原始数据

### parsed_symbol_data.txt

包含解析后的符号数据摘要：
- 符号名称、前缀、封装
- 制造商、LCSC ID
- 引脚数量
- 各种几何图形数量（矩形、圆、圆弧、多边形等）
- 边界框坐标

### parsed_symbol_data_detail.json

包含解析后的符号数据完整详细信息（JSON 格式），包括：
- **info**: 符号基本信息（名称、前缀、封装、制造商、LCSC ID 等）
- **bbox**: 边界框详细信息（x, y, width, height）
- **pins**: 引脚详细信息数组，每个引脚包含：
  - settings: 引脚设置（位置、旋转、类型等）
  - dot: 引脚圆点信息
  - path: 引脚路径信息
  - name: 引脚名称信息
  - dotBis: 第二个引脚圆点信息
  - clock: 引脚时钟信息
- **polygons**: 多边形详细信息数组
- **polylines**: 多段线详细信息数组
- **circles**: 圆详细信息数组
- **arcs**: 圆弧详细信息数组
- **rectangles**: 矩形详细信息数组
- **ellipses**: 椭圆详细信息数组
- **paths**: 路径详细信息数组

### parsed_footprint_data.txt

包含解析后的封装数据摘要：
- 封装名称、描述、封装类型
- 焊盘、走线、孔的数量
- 各种几何图形数量
- 3D 模型 UUID 和名称
- 边界框坐标

### parsed_footprint_data_detail.json

包含解析后的封装数据完整详细信息（JSON 格式），包括：
- **info**: 封装基本信息（名称、类型、3D 模型名称）
- **bbox**: 边界框详细信息（x, y, width, height）
- **model3D**: 3D 模型详细信息（UUID、名称、类型）
- **pads**: 焊盘详细信息数组，每个焊盘包含：
  - shape: 焊盘形状
  - centerX, centerY: 焊盘中心坐标
  - width, height: 焊盘尺寸
  - layerId: 层 ID
  - net: 网络
  - number: 焊盘编号
  - holeRadius: 孔半径
  - points: 坐标点
  - rotation: 旋转角度
  - holeLength: 孔长度
  - isPlated: 是否金属化
- **tracks**: 走线详细信息数组
- **holes**: 孔详细信息数组
- **circles**: 圆详细信息数组
- **rectangles**: 矩形详细信息数组
- **arcs**: 圆弧详细信息数组
- **texts**: 文本详细信息数组
- **polygons**: 多边形详细信息数组

### exported_symbol_export.kicad_sym

导出的 KiCad 6 符号库文件的完整内容，可用于：
- 检查符号定义是否正确
- 对比原始数据和导出数据
- 调试符号转换问题

### exported_footprint_export.kicad_mod

导出的 KiCad 封装文件的完整内容，可用于：
- 检查封装定义是否正确
- 对比原始数据和导出数据
- 调试封装转换问题

## 禁用调试导出

### 方法一：使用 CMake 命令行

bash
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=OFF
cmake --build . --config Debug


### 方法二：修改 CMakeLists.txt

将 CMakeLists.txt 中的选项改回 OFF：

cmake
option(ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT "Enable debug export for symbol and footprint data" OFF)


## 注意事项

1. **性能影响**：启用调试导出会增加少量 I/O 操作，但对性能影响很小
2. **磁盘空间**：每个元器件会生成约 5-10 个调试文件，批量转换时注意磁盘空间
3. **清理**：调试数据不会自动清理，需要手动删除 debug_data 文件夹
4. **版本控制**：建议将 debug_data 文件夹添加到 .gitignore 中

## 故障排查

### 问题：调试数据未生成

**可能原因**：
- 宏定义未正确启用
- 项目未重新编译
- 输出路径不可写

**解决方法**：
1. 检查 CMake 配置输出，确认看到 "Symbol and footprint debug export is ENABLED"
2. 清理构建目录并重新编译
3. 检查输出路径的写入权限

### 问题：调试数据不完整

**可能原因**：
- 转换过程中出现错误
- 某些数据解析失败

**解决方法**：
1. 查看应用程序日志输出
2. 检查 raw_cad_data.json 是否包含完整数据
3. 检查 parsed_*_data.txt 中的错误信息

## 开发者信息

- **添加位置**：src/ui/controllers/MainController.h 和 MainController.cpp
- **宏定义**：ENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT
- **辅助方法**：
  - saveDebugData() - 保存调试数据到文件
  - getDebugDataDir() - 获取调试数据目录路径
- **保存位置**：{输出路径}/debug_data/{元器件ID}/