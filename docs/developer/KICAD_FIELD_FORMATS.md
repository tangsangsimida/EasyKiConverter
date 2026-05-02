# KiCad 字段格式分析

本文档分析了 KiCad 符号库 (.kicad_sym) 和封装库 (.kicad_mod) 的字段格式，用于指导导出功能的实现。

## 符号库 (.kicad_sym) 格式

### 文件结构

```lisp
(kicad_symbol_lib (version 20220914) (generator kicad_symbol_editor)
  (symbol "元件名" (in_bom yes) (on_board yes)
    ;; 标准属性
    (property "Reference" "U" (at 3.81 6.35 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Value" "元件名" (at 3.81 -6.35 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" "" (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "Datasheet" "https://..." (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    
    ;; 描述和关键词属性（可选，用于库管理）
    (property "ki_keywords" "CMOS BUFFER 3State" (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "ki_description" "8-bit D-flip-flop with 3-State outputs" (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    
    ;; 符号图形定义（unit 0 通常用于电源引脚）
    (symbol "元件名_0_0"
      ;; 电源引脚、图形等
    )
    
    ;; 功能单元定义
    (symbol "元件名_1_1"
      ;; 引脚定义
      (pin input line (at -12.7 2.54 0) (length 7.62)
        (name "引脚名" (effects (font (size 1.27 1.27))))
        (number "引脚号" (effects (font (size 1.27 1.27))))
      )
    )
  )
)
```

### 描述字段说明

| 字段 | 说明 | 示例 |
|------|------|------|
| `ki_description` | 元件描述 | `"8-bit D-flip-flop with 3-State outputs"` |
| `ki_keywords` | 关键词（空格分隔） | `"CMOS BUFFER 3State"` |
| `Reference` | 参考标识符前缀 | `"U"` (集成电路), `"R"` (电阻), `"C"` (电容) |
| `Value` | 元件值/型号 | `"40374"` |
| `Footprint` | 默认封装 | `""` (可为空) |
| `Datasheet` | 数据手册链接 | `"https://..."` |

### 实际示例

从 `4xxx_IEEE.kicad_sym` 文件中提取的示例：

```lisp
(symbol "40374" (in_bom yes) (on_board yes)
  (property "Reference" "U" (at 7.62 8.89 0)
    (effects (font (size 1.27 1.27)))
  )
  (property "Value" "40374" (at 6.35 -15.24 0)
    (effects (font (size 1.27 1.27)))
  )
  (property "Footprint" "" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  (property "Datasheet" "https://www.digchip.com/datasheets/download_datasheet.php?id=369790&part-number=HEF40374BDB" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  (property "ki_keywords" "CMOS BUFFER 3State" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  (property "ki_description" "8-bit D-flip-flop with 3-State outputs" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  ;; ... 符号定义 ...
)
```

---

## 封装库 (.kicad_mod) 格式

### 文件结构

```lisp
(footprint "封装名" (version 20211014) (generator pcbnew)
  (layer "F.Cu")
  (tedit 5A030281)
  (descr "封装描述文本")
  (tags "标签1 标签2")
  (attr through_hole)  ;; 或 smd
  
  ;; 参考标识符
  (fp_text reference "REF**" (at 3.8 -7.2) (layer "F.SilkS")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  
  ;; 元件值
  (fp_text value "封装名" (at 3.8 7.4) (layer "F.Fab")
    (effects (font (size 1 1) (thickness 0.15)))
  )
  
  ;; 图形元素（丝印、阻焊、装配层等）
  (fp_line (start x1 y1) (end x2 y2) (layer "F.SilkS") (width 0.12))
  (fp_circle (center x y) (end x y) (layer "F.SilkS") (width 0.12))
  
  ;; 焊盘定义
  (pad "1" thru_hole rect (at 0 0) (size 2 2) (drill 1) (layers *.Cu *.Mask))
  (pad "2" smd rect (at 4 0) (size 2 4.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  
  ;; 3D 模型引用
  (model "${KICAD6_3DMODEL_DIR}/路径/模型.wrl"
    (offset (xyz 0 0 0))
    (scale (xyz 1 1 1))
    (rotate (xyz 0 0 0))
  )
)
```

### 描述字段说明

| 字段 | 说明 | 示例 |
|------|------|------|
| `descr` | 封装描述 | `"Generic Buzzer, D12mm height 9.5mm with RM7.6mm"` |
| `tags` | 标签（空格分隔） | `"buzzer piezo"` |
| `attr` | 封装属性 | `through_hole` / `smd` |

### 实际示例

#### 通孔封装示例

```lisp
(footprint "Buzzer_12x9.5RM7.6" (version 20211014) (generator pcbnew)
  (layer "F.Cu")
  (tedit 5A030281)
  (descr "Generic Buzzer, D12mm height 9.5mm with RM7.6mm")
  (tags "buzzer")
  (attr through_hole)
  ;; ...
  (pad "1" thru_hole rect (at 0 0) (size 2 2) (drill 1) (layers *.Cu *.Mask))
  (pad "2" thru_hole circle (at 7.6 0) (size 2 2) (drill 1) (layers *.Cu *.Mask))
)
```

#### SMD 封装示例

```lisp
(footprint "Buzzer_CUI_CPT-9019S-SMT" (version 20211014) (generator pcbnew)
  (layer "F.Cu")
  (tedit 5C12D246)
  (descr "https://www.cui.com/product/resource/cpt-9019s-smt.pdf")
  (tags "buzzer piezo")
  (attr smd)
  ;; ...
  (pad "1" smd rect (at -4 0) (size 2 4.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  (pad "2" smd rect (at 4 0) (size 2 4.2) (layers "F.Cu" "F.Paste" "F.Mask"))
)
```

---

## 当前代码导出状态（基于 v3.1.7）

### 符号库导出（ExporterSymbol.cpp）

当前代码实际导出的属性：

| 属性名 | 数据来源 | 说明 |
|--------|----------|------|
| `Reference` | `symbolData.info().prefix` | 参考标识符前缀（如 "U", "R", "C"） |
| `Value` | `symbolData.info().name` | 元件名（如 "4001", "RESISTOR"） |
| `Footprint` | `symbolData.info().package` | 封装名（格式：`libName:packageName`） |
| `Datasheet` | `symbolData.info().datasheet` | 数据手册链接 |
| `Manufacturer` | `symbolData.info().manufacturer` | 制造商 |
| `LCSC Part` | `symbolData.info().lcscId` | LCSC 编号（如 "C12345"） |

**当前未导出的字段（待实现）**：
- `ki_description` - 符号描述
- `ki_keywords` - 关键词

### 封装库导出（ExporterFootprint.cpp）

当前代码**未导出**描述和标签字段：

| 字段 | 状态 | 说明 |
|------|------|------|
| `descr` | ❌ 未实现 | 封装描述 |
| `tags` | ❌ 未实现 | 标签 |
| `attr` | ✅ 已实现 | 根据焊盘类型自动判断 `through_hole` 或 `smd` |

---

## 数据结构字段分析

### SymbolInfo 结构体字段

```cpp
struct SymbolInfo {
    QString name;           // 元件名 → Value
    QString prefix;         // 前缀 → Reference
    QString package;        // 封装 → Footprint
    QString manufacturer;   // 制造商 → Manufacturer
    QString description;    // 描述 → ki_description（待实现）
    QString datasheet;      // 数据手册 → Datasheet
    QString lcscId;         // LCSC 编号 → LCSC Part
    QString jlcId;          // JLC 编号
    // ... 其他字段
};
```

### FootprintInfo 结构体字段

```cpp
struct FootprintInfo {
    QString name;           // 封装名
    QString type;           // 类型
    QString model3DName;    // 3D 模型名
    // 注意：没有 description 和 keywords 字段
    // ... 其他字段
};
```

---

## 数据映射表（修正版）

### 符号库映射

| 数据源 | KiCad 属性 | 代码位置 | 状态 |
|--------|-----------|----------|------|
| `symbolData.info().prefix` | `Reference` | ExporterSymbol.cpp:510 | ✅ 已实现 |
| `symbolData.info().name` | `Value` | ExporterSymbol.cpp:524 | ✅ 已实现 |
| `symbolData.info().package` | `Footprint` | ExporterSymbol.cpp:537 | ✅ 已实现 |
| `symbolData.info().datasheet` | `Datasheet` | ExporterSymbol.cpp:554 | ✅ 已实现 |
| `symbolData.info().manufacturer` | `Manufacturer` | ExporterSymbol.cpp:569 | ✅ 已实现 |
| `symbolData.info().lcscId` | `LCSC Part` | ExporterSymbol.cpp:584 | ✅ 已实现 |
| `symbolData.info().description` | `ki_description` | - | ❌ 待实现 |
| 无数据源 | `ki_keywords` | - | ❌ 待实现（需添加字段） |

### 封装库映射

| 数据源 | KiCad 关键字 | 代码位置 | 状态 |
|--------|-------------|----------|------|
| 无数据源 | `descr` | - | ❌ 待实现（FootprintInfo 无 description 字段） |
| 无数据源 | `tags` | - | ❌ 待实现（FootprintInfo 无 keywords 字段） |
| 焊盘类型判断 | `attr` | ExporterFootprint.cpp:186 | ✅ 已实现 |

---

## 待实现功能

### 1. 符号库添加 ki_description

在 `ExporterSymbol.cpp` 中添加：

```cpp
// ki_description 属性（从 SymbolInfo.description 获取）
if (!symbolData.info().description.isEmpty()) {
    fieldOffset += 2.54;
    content += QString("    (property\n");
    content += QString("      \"ki_description\"\n");
    content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().description));
    content += "      (id 6)\n";
    content += QString("      (at %1 %2 0)\n").arg(graphCenterOffsetX, 0, 'f', 2).arg(yLow - fieldOffset, 0, 'f', 2);
    content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2);
    content += "    )\n";
}
```

**插入位置**：在 LCSC Part 属性之后（约第 593 行）

### 2. 封装库添加 descr

需要先在 `FootprintInfo` 结构体中添加 `description` 字段：

```cpp
// FootprintData.h 中添加
struct FootprintInfo {
    // ... 现有字段
    QString description;    // 封装描述
    QString keywords;       // 关键词
};
```

然后在 `ExporterFootprint.cpp` 中添加导出逻辑。

### 3. 添加 ki_keywords 字段

需要在 `SymbolInfo` 结构体中添加 `keywords` 字段（如果 EasyEDA API 提供）。

---

## 注意事项

1. **转义字符**：描述文本中的引号需要转义为 `\"`

2. **空值处理**：当前代码已使用 `isEmpty()` 检查，只在有值时才导出

3. **ID 编号**：属性的 `id` 必须连续递增（0-5 已使用，新属性从 6 开始）

4. **编码**：所有文本使用 UTF-8 编码

5. **坐标**：属性的坐标 `(at 0 0 0)` 表示默认位置，hide 表示在原理图中隐藏

---

## 参考文件

- 符号库示例：`/home/dennis/Desktop/4xxx_IEEE.kicad_sym`
- 封装库示例：`/home/dennis/Desktop/Buzzer_Beeper.pretty/`
- 符号导出器：`src/core/kicad/ExporterSymbol.cpp`
- 封装导出器：`src/core/kicad/ExporterFootprint.cpp`
- 符号数据模型：`src/models/SymbolData.h`
- 封装数据模型：`src/models/FootprintData.h`

---

*最后更新：2026-05-02*
