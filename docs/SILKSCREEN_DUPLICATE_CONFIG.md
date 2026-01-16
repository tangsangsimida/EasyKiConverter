# 丝印层复制配置说明

## 概述

当导出的封装丝印层没有图形时，系统会自动将其他层的图形复制到顶层丝印层（F.SilkS）。本功能允许用户自定义需要复制的层列表。

## 默认配置

默认情况下，**只复制板框层（BoardOutLine，层10）**。

## 配置方法

### 方法一：使用静态方法（推荐）

在代码中调用 `ExporterFootprint` 的静态方法：

```cpp
#include "src/core/kicad/ExporterFootprint.h"

// 设置需要复制到丝印层的层列表
QSet<int> layers;
layers.insert(10);  // 板框层（BoardOutLine）
layers.insert(13);  // 顶层装配层（TopAssembly）
layers.insert(15);  // 机械层（Mechanical）
layers.insert(99);  // 元件外形层（ComponentShapeLayer）
ExporterFootprint::setSilkscreenDuplicateLayers(layers);

// 查看当前配置
QSet<int> currentLayers = ExporterFootprint::getSilkscreenDuplicateLayers();
qDebug() << "Current silkscreen duplicate layers:" << currentLayers;

// 重置为默认配置（只复制板框层）
ExporterFootprint::resetDuplicateLayersToDefault();
```

### 方法二：在 MainController 中配置

在 `MainController` 的构造函数中添加配置：

```cpp
MainController::MainController(QObject *parent)
    : QObject(parent)
    , m_configManager(new ConfigManager(this))
    // ... 其他初始化
{
    // 配置丝印层复制规则
    // 方式1：只复制板框层（默认）
    ExporterFootprint::resetDuplicateLayersToDefault();
    
    // 方式2：复制多个层
    QSet<int> customLayers;
    customLayers.insert(10);  // 板框层
    customLayers.insert(13);  // 装配层
    customLayers.insert(15);  // 机械层
    ExporterFootprint::setSilkscreenDuplicateLayers(customLayers);
    
    // ... 其他初始化代码
}
```

## 常用层 ID 说明

| 层 ID | 层名称 | 说明 | 是否推荐复制 |
|-------|--------|------|-------------|
| 0 | 未定义 | 未定义层 | ❌ |
| 1 | TopLayer | 顶层铜层 | ❌ |
| 2 | BottomLayer | 底层铜层 | ❌ |
| 3 | TopSilkLayer | 顶层丝印层 | ❌（目标层） |
| 4 | BottomSilkLayer | 底层丝印层 | ❌ |
| 5 | TopPasteMaskLayer | 顶层助焊层 | ❌ |
| 6 | BottomPasteMaskLayer | 底层助焊层 | ❌ |
| 7 | TopSolderMaskLayer | 顶层阻焊层 | ❌ |
| 8 | BottomSolderMaskLayer | 底层阻焊层 | ❌ |
| 9 | Ratlines | 飞线 | ❌ |
| **10** | **BoardOutLine** | **板框层** | ✅ **（默认）** |
| 11 | Multi-Layer | 跨层对象 | ❌ |
| 12 | Document | 文档层 | ⚠️ |
| **13** | **TopAssembly** | **顶层装配层** | ✅ |
| 14 | Edge.Cuts | 边缘切割层 | ❌ |
| **15** | **Mechanical** | **机械层** | ✅ |
| 19 | 3DModel | 3D 模型层 | ❌ |
| **99** | **ComponentShapeLayer** | **元件外形层** | ✅ |
| **100** | **LeadShapeLayer** | **引脚形状层** | ✅ |
| **101** | **ComponentPolarityLayer** | **元件极性层** | ✅ |
| 102 | Hole | 孔层 | ❌ |

## 配置示例

### 示例 1：只复制板框层（默认）

```cpp
ExporterFootprint::resetDuplicateLayersToDefault();
```

### 示例 2：复制板框层和装配层

```cpp
QSet<int> layers;
layers.insert(10);  // 板框层
layers.insert(13);  // 顶层装配层
ExporterFootprint::setSilkscreenDuplicateLayers(layers);
```

### 示例 3：复制所有非电气层

```cpp
QSet<int> layers;
layers.insert(10);  // 板框层
layers.insert(12);  // 文档层
layers.insert(13);  // 装配层
layers.insert(15);  // 机械层
layers.insert(99);  // 元件外形层
layers.insert(100); // 引脚形状层
layers.insert(101); // 元件极性层
ExporterFootprint::setSilkscreenDuplicateLayers(layers);
```

### 示例 4：复制所有层（包括电气层，不推荐）

```cpp
QSet<int> layers;
layers.insert(1);   // 顶层铜层
layers.insert(2);   // 底层铜层
layers.insert(10);  // 板框层
layers.insert(13);  // 装配层
// ... 添加所有需要的层
ExporterFootprint::setSilkscreenDuplicateLayers(layers);
```

## 调试输出

启用调试导出功能后，可以在控制台看到详细的复制日志：

```
Silkscreen check - Has graphics: false Count: 0
Silkscreen has no graphics, duplicating graphics from other layers to F.SilkS
=== Silkscreen duplicate layers configuration ===
Configured layers: QSet(10)
  - Layer 10 (Edge.Cuts)
=== End configuration ===
=== Checking all layers for graphics ===
Layers with graphics:
  Layer 10 (Edge.Cuts): 4 graphics [tracks: 4 circles: 0 rects: 0 arcs: 0]
=== End layer check ===
  Duplicated track from layer 10 to F.SilkS
  Duplicated track from layer 10 to F.SilkS
  Duplicated track from layer 10 to F.SilkS
  Duplicated track from layer 10 to F.SilkS
Total duplicated graphics to F.SilkS: 4
```

## 注意事项

1. **不要复制电气层**：铜层（1, 2）和跨层对象（11）不应该复制到丝印层
2. **不要复制目标层**：不要将丝印层（3, 4）复制到自身
3. **不要复制功能层**：Paste层（5, 6）、Mask层（7, 8）和边缘切割层（14）不应该复制
4. **板框层最安全**：板框层（10）是最安全的选择，通常包含封装的外形轮廓
5. **装配层推荐**：装配层（13）通常包含封装的主体图形，适合复制

## 推荐配置

### 最小配置（默认）
```cpp
QSet<int> layers = {10};  // 只复制板框层
```

### 推荐配置
```cpp
QSet<int> layers = {10, 13, 15};  // 板框层 + 装配层 + 机械层
```

### 完整配置
```cpp
QSet<int> layers = {10, 13, 15, 99, 100, 101};  // 所有非电气层
```

## API 参考

### 静态方法

#### `setSilkscreenDuplicateLayers(const QSet<int> &layers)`
设置需要复制到丝印层的层列表。

**参数**：
- `layers`：层 ID 列表

**示例**：
```cpp
QSet<int> layers = {10, 13};
ExporterFootprint::setSilkscreenDuplicateLayers(layers);
```

#### `getSilkscreenDuplicateLayers()`
获取当前需要复制到丝印层的层列表。

**返回值**：`QSet<int>` 层 ID 列表

**示例**：
```cpp
QSet<int> layers = ExporterFootprint::getSilkscreenDuplicateLayers();
qDebug() << "Layers:" << layers;
```

#### `resetDuplicateLayersToDefault()`
重置为默认配置（只复制板框层）。

**示例**：
```cpp
ExporterFootprint::resetDuplicateLayersToDefault();
```

## 相关文件

- `src/core/kicad/ExporterFootprint.h` - 导出器头文件
- `src/core/kicad/ExporterFootprint.cpp` - 导出器实现文件
- `src/ui/controllers/MainController.cpp` - 主控制器（可在此配置）

## 版本信息

- **添加版本**：3.0.0
- **最后更新**：2026年1月16日
- **作者**：EasyKiConverter 团队