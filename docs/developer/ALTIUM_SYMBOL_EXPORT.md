# Altium SchLib 符号导出能力

本文说明 EasyKiConverter 当前对 AD/Altium SchLib 符号的导出范围，以及中间表示与 SchLib 记录之间的映射关系。

## 已支持内容

- 符号主体图形：矩形、线段、圆/椭圆、圆弧、多边形、折线、Path 和文本。
- 引脚几何：位置、长度、方向、名称、编号、显示状态和部件归属。
- 引脚电气类型：输入、输出、双向、无源、电源、开集电极和开集发射极。
- 引脚装饰：取反圆点、时钟、取反时钟、低电平有效、开集电极、开集发射极、高阻、脉冲和延迟。
- 符号参数：Value、Description、Manufacturer、Manufacturer Part Number、Datasheet、LCSC、JLCPCB、供应商及自定义参数。
- 参数控制：参数值、名称、显示/隐藏、只读、位置、旋转角度、字体编号和所属部件。
- 多部件符号：图形、文本、参数和引脚按 `partIndex` 写入对应部件。
- 多候选封装：每个封装生成一个 SchLib implementation，并自动去重。
- 符号别名：从 EasyEDA 数据或 IR 中读取，写入组件记录及参数字段。
- 来源元数据：未知键会作为自定义参数保留，避免供应商字段丢失。

## 数据流

```text
SymbolData
    ↓ IrBuilder::toSymbolIR
SymbolComponentIR
    ↓ ExporterAltiumSymbol::convertSymbol
AltiumSchComponent
    ↓ AltiumSchLibWriter
SchLib OLE/Data
```

## 参数字段约定

- `sourceMetadata["value"]` 优先作为 Altium `Comment` 字段；未提供时使用符号名称。
- `sourceMetadata["description"]` 优先作为 Description；未提供时使用 `SymbolComponentIR::description`。
- `sourceMetadata` 中的标准键使用稳定的 AD 显示名称。
- 未知键原样作为自定义参数名称写入。
- `SymbolParameterIR` 适用于需要显式控制显示、只读、位置和旋转的参数。
- 空名称或空值字段不会写入 SchLib。

## 已知限制

- 任意复杂 SVG Path 目前仍按解析后的点序列写入，不是完整的贝塞尔曲线协议记录。
- No Connect、模拟输入、总线组等无法直接由当前 Altium Pin Record 表达的语义，仍需要专用图形或协议记录支持。
- 符号级 3D/仿真模型目前保留在来源元数据中，尚未生成完整的 AD 仿真模型记录。
- 最终兼容性仍需使用目标版本的 Altium Designer 打开并检查引脚连接、参数编辑和多部件显示。

## 验证

使用项目构建工具执行：

```bash
QT_QPA_PLATFORM=offscreen python3 tools/python/build_project.py --test
```

当前单元、集成和 UI 测试均应通过，且 SchLib 测试会验证参数记录、UTF-8 文本、别名和 `WEIGHT` 计数。
