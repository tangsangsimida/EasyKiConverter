# 封装解析修复说明

## 问题总结

当前封装解析存在以下三个问题需要修复：

### 1. Type 判断错误
- **当前问题**：仅根据顶层 SMT 标志判断类型
- **正确方法**：检查焊盘是否有孔（holeRadius > 0 或 holeLength > 0）
- **修复位置**：`src/core/easyeda/EasyedaImporter.cpp` 的 `importFootprintData` 函数

### 2. 3D Model UUID 遗漏
- **当前问题**：未从原始数据中提取 UUID
- **正确方法**：从 `head.uuid_3d` 字段提取
- **修复位置**：`src/core/easyeda/EasyedaImporter.cpp` 的 `importFootprintData` 函数

### 3. BBox 不完整
- **当前问题**：只读取了中心点（x, y），缺少包围盒尺寸（width, height）
- **正确方法**：从 `dataStr.BBox` 对象读取完整的包围盒信息
- **修复位置**：
  - `src/models/FootprintData.h` - 添加 width 和 height 字段
  - `src/models/FootprintData.cpp` - 添加序列化方法
  - `src/core/easyeda/EasyedaImporter.cpp` - 修改解析逻辑

## 手动修复步骤

### 步骤 1：修改 FootprintData.h

在 `FootprintBBox` 结构中添加 width 和 height 字段：

```cpp
struct FootprintBBox {
    double x;
    double y;
    double width;   // 新增：包围盒宽度
    double height;  // 新增：包围盒高度

    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
};
```

### 步骤 2：修改 FootprintData.cpp

在 `FootprintBBox::toJson()` 方法中添加序列化：

```cpp
QJsonObject FootprintBBox::toJson() const
{
    QJsonObject json;
    json["x"] = x;
    json["y"] = y;
    json["width"] = width;   // 新增
    json["height"] = height; // 新增
    return json;
}
```

在 `FootprintBBox::fromJson()` 方法中添加反序列化：

```cpp
bool FootprintBBox::fromJson(const QJsonObject &json)
{
    x = json["x"].toDouble();
    y = json["y"].toDouble();
    width = json["width"].toDouble();   // 新增
    height = json["height"].toDouble(); // 新增
    return true;
}
```

在 `FootprintData::clear()` 方法中添加清理：

```cpp
void FootprintData::clear()
{
    m_info = FootprintInfo();
    m_bbox = FootprintBBox();  // 这会自动将 width 和 height 重置为 0
    m_pads.clear();
    // ... 其他清理代码
}
```

### 步骤 3：修改 EasyedaImporter.cpp

#### 3.1 修复 Type 判断逻辑

在 `importFootprintData` 函数中，找到判断类型的代码：

```cpp
// 先临时设置为 smd，稍后根据焊盘是否有孔来判断
bool isSmd = cadData.contains("SMT") && cadData["SMT"].toBool();
info.type = isSmd ? "smd" : "tht";
```

在形状解析循环结束后（在 `qDebug() << "Footprint imported - Pads:"` 之前），添加类型判断修正代码：

```cpp
// 修正类型判断：检查焊盘是否有孔
bool hasHole = false;
for (const FootprintPad &pad : footprintData->pads()) {
    if (pad.holeRadius > 0 || pad.holeLength > 0) {
        hasHole = true;
        break;
    }
}

if (hasHole) {
    FootprintInfo info = footprintData->info();
    info.type = "tht";
    footprintData->setInfo(info);
    qDebug() << "Corrected type to THT (through-hole) because pads have holes";
}

// 提取 3D Model UUID
if (head.contains("uuid_3d")) {
    QString uuid3d = head["uuid_3d"].toString();
    Model3DData model3d = footprintData->model3D();
    model3d.setUuid(uuid3d);
    footprintData->setModel3D(model3d);
    qDebug() << "3D Model UUID extracted from head.uuid_3d:" << uuid3d;
}
```

#### 3.2 修复 BBox 解析逻辑

找到导入边界框的代码，替换为：

```cpp
// 导入边界框（包围盒）
FootprintBBox footprintBbox;
if (dataStr.contains("BBox")) {
    QJsonObject bbox = dataStr["BBox"].toObject();
    footprintBbox.x = bbox["x"].toDouble();
    footprintBbox.y = bbox["y"].toDouble();
    footprintBbox.width = bbox["width"].toDouble();
    footprintBbox.height = bbox["height"].toDouble();
} else {
    // 如果没有 BBox，使用 head.x 和 head.y 作为中心点
    footprintBbox.x = head["x"].toDouble();
    footprintBbox.y = head["y"].toDouble();
    footprintBbox.width = 0;
    footprintBbox.height = 0;
}
footprintData->setBbox(footprintBbox);
```

## 验证修复

修复后，编译项目并运行测试，验证以下内容：

1. **Type 判断**：
   - 焊盘有孔的元件应被识别为 `tht`
   - 焊盘无孔的元件应被识别为 `smd`

2. **3D Model UUID**：
   - 应正确提取和显示 UUID

3. **BBox**：
   - 应正确显示包围盒的 x, y, width, height
   - 示例：`BBox: x=3947.5, y=2938.7, width=105.6, height=105.6`

## 预期输出

修复后的解析结果应如下：

```
=== Footprint Data ===
Name: RELAY-TH_8358-012-1HP
Type: tht
3D Model Name: RELAY-TH_8358-012-1HP
3D Model UUID: 6564f947c51b46459fed6fd8c26d021c
Pads count: 8
Tracks count: 0
Holes count: 0
Circles count: 9
Rectangles count: 1
Arcs count: 0
Texts count: 0
BBox: x=3947.5, y=2938.7, width=105.6, height=105.6
```

## 注意事项

1. **单位转换**：所有坐标和尺寸单位为 mil，导出到 KiCad 时需要转换为 mm（1 mil = 0.0254 mm）
2. **类型判断优先级**：焊盘是否有孔的判断优先于 SMT 标志
3. **BBox vs 中心点**：
   - `BBox` 是包围盒（左上角 + 尺寸）
   - `head.x` 和 `head.y` 是元件中心点
   - 两者不同，不要混淆

## 编译验证

修复完成后，运行以下命令编译：

```bash
cd build
cmake --build . --config Debug
```

如果编译成功，说明修复正确。如果出现错误，请检查：
- `FootprintBBox` 结构是否包含 width 和 height 字段
- 序列化方法是否正确处理这两个字段
- 类型判断代码是否正确调用 `setInfo` 方法