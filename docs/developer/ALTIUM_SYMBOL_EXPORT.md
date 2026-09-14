# Altium SchLib 符号导出能力

本文说明 EasyKiConverter 当前对 AD/Altium SchLib 符号的导出范围，以及中间表示与 SchLib 记录之间的映射关系。

## 已支持内容

- 符号主体图形：矩形、圆角矩形、线段、圆/椭圆、圆弧、椭圆弧、扇形、多边形、折线、Path、三次 Bézier 曲线、IEEE 图形、文本、文本框和图片。
- 图元样式：实线、虚线和点线，以及填充颜色、边框颜色和线宽。
- 引脚几何：位置、长度、方向、名称、编号、显示状态和部件归属。
- 引脚电气类型：输入、输出、双向、无源、电源、开集电极和开集发射极。
- 引脚装饰：覆盖 Altium IEEE 图形集合，包括取反圆点、时钟、低电平有效、开集电极/发射极、高阻、脉冲、延迟、移位、模拟/数字输入、总线、逻辑关系和信号方向标志。
- 符号参数：Value、Description、Manufacturer、Manufacturer Part Number、Datasheet、LCSC、JLCPCB、供应商及自定义参数。
- 参数控制：参数值、名称、显示/隐藏、只读、位置、旋转角度、字体编号和所属部件。
- 文本字体：普通文本支持字体族、字号、粗体和斜体，并在 SchLib `FileHeader` 中动态登记字体表。
- 多部件符号：图形、文本、参数和引脚按 `partIndex` 写入对应部件。
- 公共图元：IR 中图元的负 `partIndex` 写入 Altium Part Zero（`OWNERPARTID=-1`）；引脚仍通过 `SymbolPinIR::commonToAllParts` 表示公共连接点，且引脚名称、编号文字会继承该归属。
- `IndexInSheet`：每个组件内由图元和二进制引脚共享连续计数；首个内容记录省略值 `0`，引脚虽然没有文本字段但仍占用一个计数槽。
- 多候选封装：每个封装生成一个 SchLib implementation，并自动去重。
- 通用模型关联：支持通过 IR 或来源元数据写入 SPICE、SIM、STEP、VRML 等模型类型、数据文件实体、参数和引脚映射。
- 模型默认状态：仅第一个实现写入 `ISCURRENT=T`；没有数据文件实体的 SPICE/SIM 等模型不会错误生成 `.PcbLib` 路径。
- 符号别名：从 EasyEDA 数据或 IR 中读取，写入组件记录及参数字段。
- 来源元数据：未知键会作为自定义参数保留，避免供应商字段丢失。
- 独立 IEEE 图形：通过 `SymbolIeeeIR` 写入 Altium `RECORD=3`，支持缩放、旋转、镜像、颜色和部件归属。
- 图片图元：通过 `SymbolImageIR` 写入 `RECORD=30`；提供图片字节和文件名时会同时写入压缩后的 `/Storage` 嵌入资源，否则保留为外部链接。
- 非填充 SVG Path 的直线和 C/S/Q/T 曲线会保留为段级 IR；直线写入路径记录，三次/二次 Bézier 写入 Altium 原生 `RECORD=5`，避免曲线被无谓采样成大量折线。
- 嵌入图片文件名在单个库内按大小写不敏感方式去重；重复名称会稳定追加 `_2`、`_3` 等后缀，并同步更新图片记录和 `/Storage` 条目。
- 嵌入资源只使用文件名部分写入 `/Storage`，路径分隔符会被规范化；外部链接仍保留原始路径。空名称、路径遍历名称或超出 255 字节的嵌入名称会被跳过并记录警告。
- `SymbolData::validationErrors()` 可在导出前一次性报告空名称、非正尺寸、负圆角半径、无效点列、空路径和空文本等输入问题；`validate()` 保留只返回首个错误的兼容接口。

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

- 非填充来源 SVG Path 的直线、C/S/Q/T 曲线会保留原生段；A/a 圆弧及填充路径仍使用采样点列回退，以确保现有闭合和填充语义兼容。
- `FlagLeft` 和 `FlagRight` 使用 Altium 的左右信号流图形表达；不同 AD 版本的图形外观仍需目标版本实机确认。
- EasyEDA 当前数据源不一定提供完整的符号级 3D/仿真模型协议字段；调用方可通过 `SymbolModelIR` 或约定的来源元数据补充模型记录。
- EasyEDA 当前符号数据没有统一的文本框、图片、扇形和椭圆弧来源字段；这些图元可由调用方通过 IR 直接添加，圆角矩形的 `rx/ry` 会从来源数据自动保留。
- 最终兼容性仍需使用目标版本的 Altium Designer 打开并检查引脚连接、参数编辑和多部件显示。

## 验证

使用项目构建工具执行：

```bash
QT_QPA_PLATFORM=offscreen python3 tools/python/build_project.py --test
```

当前单元、集成和 UI 测试均应通过，且 SchLib 测试会验证参数记录、UTF-8 文本、别名和 `WEIGHT` 计数。
