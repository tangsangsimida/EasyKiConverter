# ADR 007: 弱网容错分析

## 状态

已接受 (2026-02-12)

## 背景

在 v3.0.4 版本中，对项目的弱网支持能力进行了全面分析，发现以下关键问题：

1. **网络组件不统一**：项目存在多套独立的网络请求实现（`FetchWorker`、`NetworkUtils`、`NetworkWorker`、`ComponentService`），各自的超时/重试/退避策略不一致
2. **`FetchWorker` 超时不重试**：超时是弱网最常见的错误类型，但 `FetchWorker` 在超时后直接放弃重试
3. **`PreviewImageExporter`/`DatasheetExporter` 无超时保护**：使用无保护的 `QEventLoop::exec()`，弱网下可能永久阻塞
4. **超时时间配置不合理**：`FetchWorker` 的超时时间（8-10s）对弱网环境过短

## 分析结论

### 问题严重性分级

| 严重程度 | 问题 | 影响 |
|---------|------|------|
| **严重** | `FetchWorker` 超时不重试 | 弱网下批量导出全部失败 |
| **严重** | `PreviewImageExporter`/`DatasheetExporter` 无超时保护 | UI 无响应 |
| **严重** | 超时时间过短（8-10s） | RTT > 2s 时首次连接大概率超时 |
| **中等** | 惊群效应 | 带宽竞争加剧 |
| **中等** | 固定 500ms 重试延迟 | 弱网下过于激进 |
| **低** | 预览图与 3D 模型超时不一致 | 数据量与超时不成比例 |

### 各组件弱网能力对比

| 组件 | 超时 | 重试 | 退避策略 | 弱网评级 |
|------|------|------|----------|---------|
| `FetchWorker` | 8-10s | 3次（超时不重试） | 指数 ×2 | 差 |
| `NetworkUtils` | 60s | 5次（超时可重试） | 无（0ms） | 良好 |
| `NetworkWorker` | 有（QTimer） | 3次 | 无 | 中等 |
| `ComponentService` | 15-45s | 3次 | 随机 | 中等 |
| `PreviewImageExporter` | 无 | 无 | 无 | 极差 |

## 决策

### 改进方向

基于分析结论，建议分两个优先级实施改进：

#### P0 改进（优先实施）

1. **修复 `FetchWorker` 超时重试逻辑**
   - 对 `OperationCanceledError`（超时）执行正常重试
   - 与 `NetworkUtils` 的超时重试行为保持一致

2. **调整超时时间**
   - 组件信息：8s -> 15-20s
   - 3D 模型：10s -> 30s
   - 与 `NetworkUtils` 的超时配置保持一致

3. **为 `PreviewImageExporter`/`DatasheetExporter` 添加超时保护**
   - 使用 QTimer 保护 `QEventLoop::exec()`

#### P1 改进（后续实施）

4. **统一重试延迟**
   - 采用 `NetworkUtils` 的递增延迟模式（3s -> 5s -> 10s）替代 500ms 固定延迟

5. **添加抖动（Jitter）**
   - 在重试延迟中加入随机抖动，避免惊群效应

## 后果

### 正面

- 明确了项目弱网支持的现状和改进方向
- 为后续版本的网络容错改进提供了清晰的优先级指导
- 统一各组件的网络策略标准

### 负面

- 改进工作需要投入额外的开发和测试时间
- 增加超时时间可能导致正常网络下的等待时间略有增加（可通过动态超时策略缓解）

### 风险

- `FetchWorker` 超时重试逻辑修改需要充分测试，确保 abort 场景下不会死循环
- 超时时间调整需要在弱网容错和用户体验之间取得平衡

## 参考文档

- [弱网支持分析报告](../../WEAK_NETWORK_ANALYSIS.md)
- [ADR-006: 网络性能优化](006-network-performance-optimization.md)
- [ADR-003: 流水线性能优化](003-pipeline-performance-optimization.md)
- [性能优化报告](../../PERFORMANCE_OPTIMIZATION_REPORT.md)
