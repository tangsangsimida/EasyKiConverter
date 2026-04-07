# 弱网支持分析报告

## 概述

本报告记录了 EasyKiConverter 项目在 v3.0.4 版本中针对弱网环境的支持能力分析，包括各网络组件的超时/重试/退避机制评估、已识别的问题和改进建议。

## 分析日期

2026-02-12

## 网络架构概览

项目中存在 **四套独立的网络请求实现**，各自弱网容错能力差异显著：

| 组件 | 用途 | 超时 | 重试 | 速率限制退避 |
|------|------|------|------|-------------|
| `FetchWorker` | 流水线批量导出（新版） | 是 8-10s | 是 3次（超时不重试） | 是（指数 ×2） |
| `NetworkUtils` | 单件预览获取 | 是 60s | 是 5次 | 是 |
| `NetworkWorker` | 旧版单件获取 | 是（QTimer） | 是 3次 | 否 |
| `ComponentService` | LCSC 预览图 | 是 15-45s | 是 3次 | 否 |
| `PreviewImageExporter` | 预览图导出 | **否** | 否 | 否 |

---

## 严重问题

### 问题 1: `FetchWorker` 超时后不重试

**严重程度**：严重

**问题描述**：

在 `FetchWorker::httpGet` 方法中，超时错误（`QNetworkReply::OperationCanceledError`）被视为不可恢复错误，直接跳过所有剩余重试：

```cpp
} else if (replyPtr->error() == QNetworkReply::OperationCanceledError) {
    if (m_isAborted.loadRelaxed()) {
        return QByteArray();
    }
    // 超时后不再重试
    return QByteArray();
}
```

对于弱网环境，超时是最常见的错误类型。不对超时进行重试意味着弱网下几乎所有请求都会一次失败，3次重试机制形同虚设。

作为对比，同项目的 `NetworkUtils` 对超时请求会正确执行重试。

**代码位置**：`src/workers/FetchWorker.cpp`

**影响范围**：流水线批量导出的所有网络请求

---

### 问题 2: `PreviewImageExporter` / `DatasheetExporter` 无超时和重试保护

**严重程度**：严重

**问题描述**：

`PreviewImageExporter` 和 `DatasheetExporter` 使用无保护的 `QEventLoop::exec()` 无限等待：

```cpp
// PreviewImageExporter.cpp:62-66
QEventLoop loop;
connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
loop.exec();  // 无超时、无重试
```

弱网下可能永久阻塞，且失败后无重试机制。

**代码位置**：`src/services/PreviewImageExporter.cpp`、`src/services/DatasheetExporter.cpp`

**影响范围**：预览图和 datasheet 下载场景

---

## 中等问题

### 问题 3: `FetchWorker` 超时时间过短

**严重程度**：中等

**问题描述**：

```cpp
static const int COMPONENT_INFO_TIMEOUT_MS = 8000;   // 组件信息超时
static const int MODEL_3D_TIMEOUT_MS = 10000;         // 3D模型超时
```

弱网环境下（RTT > 2秒），8秒对于一次完整的 HTTPS 请求（DNS + TLS + HTTP）来说非常紧张，特别是首次连接（无连接复用时）。

**代码位置**：`src/workers/FetchWorker.h`

---

### 问题 4: 流水线 Fetch 阶段惊群效应

**严重程度**：中等

**问题描述**：

`startFetchStage()` 一次性将所有组件的 `FetchWorker` 全部投入线程池。线程池限制为 5 个并发，但弱网下：

- 5 个并发请求同时竞争有限带宽
- 所有请求超时后几乎同时重试，形成惊群效应
- 速率限制退避延迟使用 `QThread::msleep()` 阻塞线程，可能导致线程池饥饿

**代码位置**：`src/services/ExportService_Pipeline.cpp`

---

### 问题 5: `FetchWorker` 重试延迟过短

**严重程度**：中等

**问题描述**：

```cpp
static const int HTTP_RETRY_DELAY_MS = 500;  // 固定500ms
```

对比 `NetworkUtils` 使用递增延迟策略（3s -> 5s -> 10s），`FetchWorker` 的 500ms 固定延迟在弱网环境下过于激进。

**代码位置**：`src/workers/FetchWorker.h`

---

### 问题 6: 预览图与 3D 模型超时不一致

**严重程度**：低

**问题描述**：

`ComponentService::downloadLcscImage` 下载图片（通常 10KB-100KB）设置 15s 超时，而 `FetchWorker` 下载可能数 MB 的 3D 模型数据却只有 10s 超时。数据量与超时时间不成比例。

---

## 已有的弱网支持

### 已实现的正面机制

1. **`NetworkWorker` 超时保护**：使用 QTimer + abort() 实现超时中断
2. **`NetworkUtils` 完善的重试体系**：递增延迟（3s -> 5s -> 10s）+ 超时重试 + 可重试状态码识别（429/500/502/503/504）
3. **`FetchWorker` 速率限制退避**：检测 HTTP 429 后全局退避（指数 ×2），所有 Worker 共享退避状态
4. **`LcscImageService` 层级容错**：主接口失败后有 Fallback 备用爬取方案
5. **`FetchWorker` 的中断支持**：通过 `QAtomicInt m_isAborted` 和 `m_currentReply->abort()` 支持取消
6. **网络诊断信息收集**：`ComponentExportStatus::NetworkDiagnostics` 记录每次请求的延迟、重试次数、状态码
7. **导出失败重试功能**：UI 支持对失败元件"重试导出"

---

## 弱网异常场景汇总

| 场景 | 触发条件 | 表现 | 严重程度 |
|------|---------|------|---------|
| 批量导出全部失败 | 弱网 + `FetchWorker` 超时不重试 | 所有元件标记失败 | **严重** |
| 预览图/datasheet 永久阻塞 | `PreviewImageExporter` 在弱网下使用 | UI 无响应 | **严重** |
| 首次请求超时 | RTT > 2s + 需 DNS/TLS | 8s 超时不够建立连接 | **中等** |
| 惊群效应 | 5 并发同时超时同时重试 | 带宽瞬间占满再超时 | **中等** |
| 重试无效 | 500ms 固定延迟 + 不重试超时 | 仅对非超时错误生效 | **中等** |

---

## 改进建议

### P0 改进（建议优先实施）

1. **`FetchWorker` 超时后应重试**：移除超时后直接返回的逻辑，对 `OperationCanceledError` 执行正常重试
2. **增加超时时间**：组件信息超时建议调整为 15-20s，3D 模型超时建议调整为 30s

### P1 改进（建议后续实施）

3. **为 `PreviewImageExporter`/`DatasheetExporter` 添加超时**：使用 QTimer 保护 `QEventLoop::exec()`
4. **使用递增重试延迟**：参考 `NetworkUtils` 的延迟策略（3s -> 5s -> 10s）替代 500ms 固定延迟
5. **添加抖动（Jitter）**：在重试延迟中加入随机抖动，避免惊群效应

---

## 参考资料

- [ADR-007: 弱网容错分析](project/adr/007-weak-network-resilience-analysis.md)
- [ADR-006: 网络性能优化](project/adr/006-network-performance-optimization.md)
- [性能优化报告](PERFORMANCE_OPTIMIZATION_REPORT.md)
- [架构文档](developer/ARCHITECTURE.md)
