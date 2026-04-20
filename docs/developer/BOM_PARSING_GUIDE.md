# BOM 文件解析功能指南

## 概述

EasyKiConverter 支持 BOM（物料清单）文件导入功能，可以自动从 CSV 或 Excel 文件中提取 LCSC 元件编号，并批量转换为 KiCad 库文件。

## 支持的文件格式

- **CSV 文件** (.csv, .txt)
- **Excel 文件** (.xlsx, .xls)

## 元件编号识别规则

系统会自动识别符合以下格式的 LCSC 元件编号：
- 以 `C` 或 `c` 开头（不区分大小写）
- 后跟至少 4 位数字
- 示例：`C1234`, `C12345`, `C1234567890`
- 系统会自动过滤常见的非 LCSC 元件编号（如 `C0402`, `C0603`, `C0805`, `C1206`）

## 使用方法

### 1. 通过 UI 导入

1. 在主界面中点击"导入 BOM"按钮
2. 选择 BOM 文件（.csv 或 .xlsx 格式）
3. 系统会自动解析文件并提取所有有效的元件编号
4. 提取的元件编号会自动添加到元件列表中

### 2. 通过代码使用

**方式一：使用 ComponentService**

```cpp
#include "services/ComponentService.h"

// 创建 ComponentService 实例
ComponentService* service = new ComponentService(this);

// 解析 BOM 文件
QString bomFilePath = "path/to/your/bom.csv";
QStringList componentIds = service->parseBomFile(bomFilePath);
```

**方式二：直接使用 BomParser**

```cpp
#include "services/BomParser.h"

// 创建 BomParser 实例
BomParser parser;

// 解析 BOM 文件
QString bomFilePath = "path/to/your/bom.csv";
QStringList componentIds = parser.parse(bomFilePath);
```

### 3. 验证元件编号

```cpp
// 使用 BomParser 验证单个元件编号
bool isValid = BomParser::validateId("C1234");  // 返回 true
bool isInvalid = BomParser::validateId("D1234"); // 返回 false

// 从文本中提取元件编号
QString text = "元件列表: C1234, C5678, C9012";
QStringList ids = ComponentService::extractComponentIdFromText(text);
// ids = ["C1234", "C5678", "C9012"]
```

## BOM 文件格式要求

### CSV 文件格式

CSV 文件可以是任意格式，系统会扫描所有单元格并提取匹配的元件编号。

**示例 CSV 文件：**

```csv
元件编号,数量,描述,封装,制造商
C12345,10,电阻 10k,0805,Yageo
C67890,5,电容 100nF,0805,Samsung
C90123,20,电感 10uH,1210,Murata
```

### Excel 文件格式

Excel 文件支持多个工作表，系统会遍历所有工作表的所有单元格。

**示例 Excel 文件结构：**

| 元件编号 | 数量 | 描述 | 封装 | 制造商 |
|---------|------|------|------|--------|
| C12345  | 10   | 电阻 10k | 0805 | Yageo |
| C67890  | 5    | 电容 100nF | 0805 | Samsung |
| C90123  | 20   | 电感 10uH | 1210 | Murata |

## 技术实现

### CSV 解析

CSV 解析使用 Qt 的 `QFile` 和 `QTextStream`，支持 UTF-8 编码。

```cpp
QStringList ComponentService::parseCsvBomFile(const QString& filePath) {
    // 读取 CSV 文件
    // 按逗号分割每行的单元格
    // 使用正则表达式匹配 LCSC 元件编号
    // 返回去重后的元件编号列表
}
```

### Excel 解析

Excel 解析使用 QXlsx 库直接读取 Excel 文件，支持 `.xlsx` 和 `.xls` 格式。

```cpp
// 位于 src/services/BomParser.cpp
QStringList BomParser::parseExcel(const QString& filePath) {
    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        return QStringList();
    }
    // 遍历所有工作表的所有单元格提取元件编号
    for (const QString& sheetName : xlsx.sheetNames()) {
        xlsx.selectSheet(sheetName);
        // ...
    }
    return componentIds;
}
```

## 限制和注意事项

1. **元件编号格式**：只识别以 `C` 或 `c` 开头的 LCSC 元件编号
2. **Excel 依赖**：Excel 解析需要 QXlsx 库支持
3. **排除规则**：系统会自动过滤 `C0402`, `C0603`, `C0805`, `C1206` 等常见非 LCSC 元件编号
4. **文件大小**：大型 BOM 文件可能需要较长的处理时间
5. **编码格式**：CSV 文件建议使用 UTF-8 编码

## 性能优化

- 使用正则表达式快速匹配元件编号
- 自动去重，避免重复处理
- 支持多工作表 Excel 文件并行遍历
- 内存高效，适合大型 BOM 文件

## 错误处理

- 文件不存在时返回空列表
- 文件格式不支持时输出警告信息
- 解析错误时跳过无效数据
- 提供详细的日志输出用于调试

## 示例代码

完整的示例代码可以参考 `src/ui/viewmodels/ComponentListViewModel.cpp`。

## 相关文档

- [CHANGELOG.md](CHANGELOG.md) - 版本更新日志
- [ARCHITECTURE.md](ARCHITECTURE.md) - 架构文档