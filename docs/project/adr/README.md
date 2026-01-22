# 架构决策记录 (Architecture Decision Records)

本目录包含 EasyKiConverter 项目的架构决策记录（ADR）。

## 什么是 ADR？

ADR（Architecture Decision Records）是一种轻量级文档，用于记录"为什么我们做了某个重要的技术决策"。它帮助我们：

- 理解项目的历史决策
- 为新决策提供上下文
- 促进团队沟通
- 避免重复讨论相同的问题

## ADR 格式

每个 ADR 应包含以下部分：

1. **状态**：提议、已接受、已弃用、已替代
2. **上下文**：问题是什么？为什么需要做这个决策？
3. **决策**：我们决定做什么？
4. **后果**：这个决策的结果是什么？包括积极和消极的后果

## ADR 命名规范

ADR 文件应使用以下命名格式：

```
<序号>-<简短描述>.md
```

例如：
- `001-mvvm-architecture.md`
- `002-qt-quick-ui-framework.md`
- `003-two-stage-export-strategy.md`

## ADR 流程

### 创建新 ADR

1. 创建一个新的 ADR 文件，状态设为"提议"
2. 在团队中讨论该决策
3. 根据反馈修改 ADR
4. 如果决策被接受，将状态更新为"已接受"
5. 如果决策被拒绝，将状态更新为"已拒绝"

### 更新 ADR

如果决策发生变化：

1. 更新 ADR 的内容
2. 更新状态（如果需要）
3. 创建新的 ADR 记录新的决策
4. 在旧 ADR 中引用新的 ADR

## 现有 ADR

- [ADR 001: 选择 MVVM 架构](001-mvvm-architecture.md)
- [ADR 001: Choose MVVM Architecture](001-mvvm-architecture_en.md) (English)
- [ADR 002: 流水线并行架构](002-pipeline-parallelism-for-export.md)
- [ADR 002: Pipeline Parallelism for Export](002-pipeline-parallelism-for-export_en.md) (English)
- [ADR 003: 流水线性能优化](003-pipeline-performance-optimization.md)
- [ADR 004: 符号库更新导出修复](004-symbol-library-update-fix.md)

## 参考资料

- [Architecture Decision Records](https://adr.github.io/)
- [Recording Architecture Decisions](https://www.thoughtworks.com/radar/techniques/recording-architecture-decisions)