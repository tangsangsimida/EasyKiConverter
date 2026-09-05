# 文档维护与过时内容清理

## 文档事实来源

文档中的命令、选项、目录和导出能力必须以当前代码为准：

| 内容 | 事实来源 |
| --- | --- |
| CLI 参数和默认值 | `src/utils/CommandLineParser.cpp`、`src/utils/cli/CliContext.cpp` |
| 导出目标和能力 | `src/core/*/Exporter*.h`、`src/services/export/ExporterFactory.*` |
| 转换算法和字段映射 | `src/core/ir/*`、`docs/developer/CONVERSION_MAPPING.md` |
| GUI 设置项 | `src/ui/qml/components/*SettingsCard.qml` |
| 构建和测试命令 | `AGENTS.md`、`tools/python/build_project.py`、CI 工作流 |
| 版本变更 | `docs/developer/CHANGELOG.md` 及对应版本记录 |

## 更新触发条件

以下改动必须同步检查文档：

- 增加、删除或重命名 CLI 参数；
- 修改默认值、目标格式能力或 3D 模型约束；
- 改变 IR 字段、单位、坐标系、网格量化或类型映射；
- 修改导出文件扩展名、输出目录或嵌入策略；
- 重构 UI 组件、导出流水线或构建入口；
- 修复会影响用户操作或库文件兼容性的缺陷。

## 提交前审计清单

1. 用 `rg` 检查文档中是否仍引用已删除的类名、目录、命令和默认值。
2. 检查中文文档与英文文档是否同步；无法同步时明确标注语言范围。
3. 检查 Markdown 链接指向的文件是否存在。
4. 对命令行示例执行 `--help` 或对应测试，避免复制过时参数。
5. 对转换算法文档使用一个真实元件（当前基准为 C2040）核对坐标和字段。
6. 历史问题报告不删除，改为标注“已修复/历史记录”，并链接当前实现。

## 历史文档规则

历史分析、提案和缺陷报告可以保留，但必须在标题或开头标明状态。已落地的提案应链接到当前代码和设计文档；已修复的问题应记录修复版本，避免用户把历史现象当作当前限制。
