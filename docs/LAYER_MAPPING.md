# 嘉立创 EDA -> KiCad 图层映射说明

## 概述

本文档详细说明了嘉立创 EDA 的 50+ 图层如何映射到 KiCad 图层，以及单位转换方法。

**重要提示**：
- 嘉立创 EDA 的图层是逻辑图层，不是 PCB 的物理铜层数
- 实际打样时，嘉立创会根据选择的 PCB 层数（如 2 层、4 层）自动映射
- 原始数据单位为 **mil**，导出时需要转换为 **mm**（1 mil = 0.0254 mm）

## 完整映射表

### 一、电气信号层

| 嘉立创 ID | 嘉立创名称 | KiCad ID | KiCad 名称 | 说明 |
|----------|-----------|---------|-----------|------|
| 1 | TopLayer | 0 | F.Cu | 顶层铜箔（信号层） |
| 2 | BottomLayer | 31 | B.Cu | 底层铜箔（信号层） |
| 21-52 | Inner1-32 | 1-30 | In1.Cu~In32.Cu | 内电层（用于 4 层及以上多层板） |

**注意**：
- Inner1~Inner32 对应多层板的第 3 到第 34 电气层
- 2 层板中这些层不可见也不输出
- KiCad 最多支持 32 个内层（共 34 层板）

### 二、丝印层

| 嘉立创 ID | 嘉立创名称 | KiCad ID | KiCad 名称 | 说明 |
|----------|-----------|---------|-----------|------|
| 3 | TopSilkLayer | 32 | F.SilkS | 顶层丝印（元件标识、文字） |
| 4 | BottomSilkLayer | 33 | B.SilkS | 底层丝印（元件标识、文字） |

### 三、阻焊层

| 嘉立创 ID | 嘉立创名称 | KiCad ID | KiCad 名称 | 说明 |
|----------|-----------|---------|-----------|------|
| 7 | TopSolderMaskLayer | 34 | F.Mask | 顶层阻焊（绿油开窗） |
| 8 | BottomSolderMaskLayer | 35 | B.Mask | 底层阻焊（绿油开窗） |

**重要**：
- 阻焊层扩展值（如 `~0.3`）表示 0.3 mil，需转换为 mm：`0.3 * 0.0254 = 0.00762 mm`

### 四、助焊层

| 嘉立创 ID | 嘉立创名称 | KiCad ID | KiCad 名称 | 说明 |
|----------|-----------|---------|-----------|------|
| 5 | TopPasteMaskLayer | 36 | F.Paste | 顶层锡膏层（SMT 钢网） |
| 6 | BottomPasteMaskLayer | 37 | B.Paste | 底层锡膏层（SMT 钢网） |

### 五、机械与结构层

| 嘉立创 ID | 嘉立创名称 | KiCad ID | KiCad 名称 | 说明 |
|----------|-----------|---------|-----------|------|
| 10 | BoardOutLine | 44 | Edge.Cuts | 板框轮廓（必须有） |
| 15 | Mechanical | 47 | F.Fab | 通用机械层（尺寸标注、工艺说明） |
| 99 | ComponentShapeLayer | 45 | F.CrtYd | 元件外形占位（用于布局避让） |
| 100 | LeadShapeLayer | 47 | F.Fab | 引脚形状（较少用） |
| 101 | ComponentPolarityLayer | 32 | F.SilkS | 极性标记（如电解电容 "+" 号） |

### 六、装配与文档层

| 嘉立创 ID | 嘉立创名称 | KiCad ID | KiCad 名称 | 说明 |
|----------|-----------|---------|-----------|------|
| 13 | TopAssembly | 47 | F.Fab | 顶层装配图（用于贴片厂） |
| 14 | BottomAssembly | 48 | B.Fab | 底层装配图（用于贴片厂） |
| 12 | Document | 49 | Dwgs.User | 文档注释（不输出到 Gerber） |

### 七、特殊功能层

| 嘉立创 ID | 嘉立创名称 | KiCad ID | KiCad 名称 | 说明 |
|----------|-----------|---------|-----------|------|
| 9 | Ratlines | 49 | Dwgs.User | 飞线（未布线网络的连接提示） |
| 11 | Multi-Layer | 0 | F.Cu | 跨层对象：通孔焊盘、过孔等 |
| 19 | 3DModel | 49 | Dwgs.User | 3D 模型引用层（关联 STEP/SLDPRT 模型） |
| 102 | Hole | 49 | Dwgs.User | 非金属化孔（槽孔、定位孔等） |
| 103 | DRCError | 50 | Cmts.User | DRC 错误高亮（仅编辑器显示） |

## 单位转换

### mil 转 mm

```cpp
double milValue = 9.8425;  // 焊盘直径（mil）
double mmValue = milValue * 0.0254;  // = 0.25 mm
```

### mm 转 mil

```cpp
double mmValue = 2.54;  // 焊盘直径（mm）
double milValue = mmValue * 39.3701;  // = 100 mil
```

### 常用转换示例

| mil | mm | 说明 |
|-----|----|----|
| 10 | 0.254 | 标准线宽 |
| 20 | 0.508 | 中等线宽 |
| 39.37 | 1.0 | 1 mm |
| 78.74 | 2.0 | 2 mm |
| 100 | 2.54 | 2.54 mm（0.1 英寸） |
| 200 | 5.08 | 5.08 mm（0.2 英寸） |

## 使用示例

### C++ 代码示例

```cpp
#include "src/core/utils/LayerMapper.h"

using namespace EasyKiConverter;

// 创建图层映射器
LayerMapper mapper;

// 映射图层
int easyedaLayerId = 1;  // TopLayer
int kicadLayerId = mapper.mapToKiCadLayer(easyedaLayerId);
QString kicadLayerName = mapper.getKiCadLayerName(kicadLayerId);

qDebug() << "EasyEDA Layer" << easyedaLayerId << "-> KiCad Layer" << kicadLayerId << "(" << kicadLayerName << ")";

// 单位转换
double padDiameterMil = 9.8425;
double padDiameterMm = LayerMapper::milToMm(padDiameterMil);
qDebug() << "Pad diameter:" << padDiameterMil << "mil =" << padDiameterMm << "mm";

// 判断图层类型
if (mapper.isSignalLayer(kicadLayerId)) {
    qDebug() << "This is a signal layer";
} else if (mapper.isSilkLayer(kicadLayerId)) {
    qDebug() << "This is a silk screen layer";
}
```

### 导出时的处理

```cpp
// 导出焊盘时转换单位
FootprintPad pad = footprintData.pads()[0];
double padDiameterMm = LayerMapper::milToMm(pad.width);

// 导出封装文件
QString padLine = QString("(pad %1 smd circle (at %2 %3) (size %4 %5) (layers %6))")
    .arg(pad.number)
    .arg(LayerMapper::milToMm(pad.centerX))
    .arg(LayerMapper::milToMm(pad.centerY))
    .arg(padDiameterMm)
    .arg(padDiameterMm)
    .arg(mapper.getKiCadLayerName(pad.layerId));
```

## 特殊情况处理

### 1. Multi-Layer 层的通孔焊盘

嘉立创的 Multi-Layer 层（ID=11）表示通孔焊盘，在 KiCad 中需要特殊处理：

```cpp
int easyedaLayerId = 11;
int kicadLayerId = mapper.mapToKiCadLayer(easyedaLayerId);  // 返回 F.Cu (0)

// KiCad 中通孔焊盘需要使用 '*' 表示所有层
QString kicadPadLayers = "*";  // 或者 "F.Cu B.Cu In1.Cu In2.Cu ..."
```

### 2. 内层的使用

内层（Inner1-32）仅在多层板中有效：

```cpp
int easyedaLayerId = 21;  // Inner1
int kicadLayerId = mapper.mapToKiCadLayer(easyedaLayerId);  // 返回 In1.Cu (1)

// 检查是否为内层
if (kicadLayerId >= In1_Cu && kicadLayerId <= In30_Cu) {
    qDebug() << "This is an inner layer, valid only for multi-layer boards";
    int innerLayerIndex = kicadLayerId - In1_Cu + 1;  // 1-32
    qDebug() << "Inner layer index:" << innerLayerIndex;
}
```

### 3. 未知图层的处理

如果遇到未知图层，默认映射到 Dwgs.User 层：

```cpp
int unknownLayerId = 999;
int kicadLayerId = mapper.mapToKiCadLayer(unknownLayerId);  // 返回 Dwgs.User (49)
qWarning() << "Unknown layer mapped to Dwgs.User";
```

## 完整的映射关系图

```
嘉立创 EDA 图层                  KiCad 图层
┌────────────────────┐           ┌────────────────────┐
│ 1. TopLayer        │──────────>│ 0. F.Cu            │
│ 2. BottomLayer     │──────────>│ 31. B.Cu           │
│ 21-52. Inner1-32   │──────────>│ 1-30. In1.Cu~In30.Cu│
├────────────────────┤           ├────────────────────┤
│ 3. TopSilkLayer    │──────────>│ 32. F.SilkS         │
│ 4. BottomSilkLayer │──────────>│ 33. B.SilkS         │
├────────────────────┤           ├────────────────────┤
│ 5. TopPasteMask    │──────────>│ 36. F.Paste         │
│ 6. BottomPasteMask │──────────>│ 37. B.Paste         │
├────────────────────┤           ├────────────────────┤
│ 7. TopSolderMask   │──────────>│ 34. F.Mask          │
│ 8. BottomSolderMask│──────────>│ 35. B.Mask          │
├────────────────────┤           ├────────────────────┤
│ 10. BoardOutLine   │──────────>│ 44. Edge.Cuts       │
│ 99. ComponentShape │──────────>│ 45. F.CrtYd         │
│ 101. Polarity      │──────────>│ 32. F.SilkS         │
├────────────────────┤           ├────────────────────┤
│ 11. Multi-Layer    │──────────>│ 0. F.Cu (through)   │
│ 102. Hole          │──────────>│ 49. Dwgs.User       │
│ 103. DRCError      │──────────>│ 50. Cmts.User       │
└────────────────────┘           └────────────────────┘
```

## 注意事项

1. **物理层数 vs 逻辑层数**：
   - 嘉立创有 50+ 个逻辑图层，但物理铜层数由用户选择（2 层、4 层、6 层等）
   - 内层（Inner1-32）仅在多层板中有效

2. **通孔焊盘**：
   - Multi-Layer 层的焊盘在 KiCad 中需要设置为 `through` 类型
   - 使用 `*` 表示所有层，或列出所有铜层

3. **阻焊扩展值**：
   - 原始数据中的阻焊扩展值（如 `~0.3`）单位为 mil
   - 导出时必须转换为 mm（`0.3 * 0.0254 = 0.00762 mm`）

4. **未知图层**：
   - 遇到未知图层时，默认映射到 `Dwgs.User` 层
   - 建议记录警告信息，便于调试

5. **3D 模型**：
   - 3D 模型本身不存储在任何图层
   - SVGNODE 中的 `3DModel` 层只是引用信息
   - 实际 3D 模型数据通过 UUID 单独下载

## 参考资源

- [KiCad 图层定义](https://docs.kicad.org/doxygen/layer__ids_8h.html)
- [嘉立创 EDA 图层说明](https://pro.lceda.cn/doc/)
- [PCB 设计规范](https://www.szlcsc.com/)

## 版本信息

- **文档版本**: 1.0
- **创建日期**: 2026年1月11日
- **适用版本**: EasyKiConverter 3.0.0+
- **KiCad 版本**: 6.x / 7.x