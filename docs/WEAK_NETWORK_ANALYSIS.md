# 弱网支持分析报告

## 概述

本报告记录了 EasyKiConverter 项目针对弱网环境的支持能力分析，包括各网络组件的超时/重试/退避机制评估、已识别的问题和改进建议。

## 分析日期

2026-02-12（文档更新：2026-04-20）

## 网络架构概览

项目中存在 **多套独立的网络请求实现**，各自弱网容错能力差异显著：

| 组件 | 用途 | 超时 | 重试 | 速率限制退避 |
|------|------|------|------|-------------|
| `FetchWorker` | 流水线批量导出 | 是 15-30s | 是 3次（含超时重试） | 是（指数 ×2） |
| `NetworkUtils` | 单件预览获取 | 是 60s | 是 5次 | 是 |
| `NetworkWorker` | 旧版单件获取 | 是（QTimer） | 是 3次 | 否 |
| `ComponentService` | LCSC 预览图 | 是 15-45s | 是 3次 | 否 |
| `PreviewImageExporter` | 预览图导出 | 是（30s via NetworkClient） | 是（3次） | 是 |
| `DatasheetExporter` | Datasheet 导出 | 是（30s via NetworkClient） | 是（3次） | 是 |

---

## 已修复的问题

### ✅ 问题 1: `FetchWorker` 超时后不重试（已修复）

**原严重程度**：严重

**修复方案**：

`AsyncNetworkRequest::shouldRetryInternal` 方法现在包含 `QNetworkReply::TimeoutError` 作为可重试错误：

```cpp
switch (error) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    // ... 其他错误
        return true;
    default:
        return false;
}
```

**代码位置**：`src/core/network/AsyncNetworkRequest.cpp`

---

### ✅ 问题 2: `PreviewImageExporter` / `DatasheetExporter` 无超时保护（已修复）

**原严重程度**：严重

**修复方案**：

现在使用 `NetworkClient::instance().get()` 替代无保护的 `QEventLoop::exec()`：

```cpp
// Use NetworkClient with default retry policy (3 retries, 30s timeout)
NetworkResult result = NetworkClient::instance().get(QUrl(imageUrl), ResourceType::PreviewImage);
```

**代码位置**：`src/services/PreviewImageExporter.cpp`、`src/services/DatasheetExporter.cpp`

---

### ✅ 问题 3: `FetchWorker` 超时时间过短（已修复）

**原严重程度**：中等

**修复方案**：

超时时间已增加：

```cpp
static const int COMPONENT_INFO_TIMEOUT_MS = 15000;  // 组件信息超时（毫秒）
static const int MODEL_3D_TIMEOUT_MS = 30000;        // 3D模型超时（毫秒）
```

**代码位置**：`src/workers/FetchWorker.h`

---

### ✅ 问题 5: `FetchWorker` 重试延迟过短（已修复）

**原严重程度**：中等

**修复方案**：

现在使用递增延迟策略 + 随机抖动：

```cpp
static constexpr int RETRY_DELAYS_MS[] = {3000, 5000, 10000};

int FetchWorker::calculateRetryDelay(int retryCount) {
    int baseDelay = RETRY_DELAYS_MS[retryCount];
    int jitter = static_cast<int>(baseDelay * 0.2);
    int randomOffset = QRandomGenerator::global()->bounded(-jitter, jitter + 1);
    return qMax(100, baseDelay + randomOffset);
}
```

**代码位置**：`src/workers/FetchWorker.cpp`

---

### ✅ 问题 6: 预览图与 3D 模型超时不一致（已修复）

**原严重程度**：低

**修复方案**：

3D 模型超时时间已增加到 30s，与数据量成正比。

---

## 仍存在的问题

### ⚠️ 问题 4: 流水线 Fetch 阶段惊群效应

**严重程度**：中等

**问题描述**：

`startFetchStage()` 一次性将所有组件的 `FetchWorker` 全部投入线程池。线程池限制为 5 个并发，但弱网下：

- 5 个并发请求同时竞争有限带宽
- 所有请求超时后几乎同时重试，形成惊群效应
- 速率限制退避延迟使用 `QThread::msleep()` 阻塞线程，可能导致线程池饥饿

**代码位置**：`src/services/ExportService_Pipeline.cpp`

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

| 场景 | 触发条件 | 表现 | 严重程度 | 状态 |
|------|---------|------|----------|------|
| 批量导出全部失败 | ~~弱网 + `FetchWorker` 超时不重试~~ | ~~所有元件标记失败~~ | ~~**严重**~~ | ✅ **已修复** |
| 预览图/datasheet 永久阻塞 | ~~`PreviewImageExporter` 在弱网下使用~~ | ~~UI 无响应~~ | ~~**严重**~~ | ✅ **已修复** |
| 首次请求超时 | ~~RTT > 2s + 需 DNS/TLS + 8s 超时不够~~ | ~~超时~~ | ~~**中等**~~ | ✅ **已修复** |
| 惊群效应 | 5 并发同时超时同时重试 | 带宽瞬间占满再超时 | **中等** | ⚠️ **仍存在** |
| 重试无效 | ~~500ms 固定延迟 + 不重试超时~~ | ~~仅对非超时错误生效~~ | ~~**中等**~~ | ✅ **已修复** |

---

## 改进建议

### ✅ P0 改进（已完成）

1. **`FetchWorker` 超时后应重试** ✅ 已实现 - `AsyncNetworkRequest::shouldRetryInternal` 包含 `TimeoutError`
2. **增加超时时间** ✅ 已实现 - 组件信息 15s，3D 模型 30s

### ✅ P1 改进（已完成）

3. **为 `PreviewImageExporter`/`DatasheetExporter` 添加超时** ✅ 已实现 - 使用 `NetworkClient` 替代 `QEventLoop::exec()`
4. **使用递增重试延迟** ✅ 已实现 - 3s -> 5s -> 10s 递增延迟
5. **添加抖动（Jitter）** ✅ 已实现 - 20% 随机抖动

### ⚠️ 仍需改进

6. **解决惊群效应**：需要进一步优化 Fetch 阶段的并发策略，例如：
   - 分批提交请求，避免同时超时同时重试
   - 使用自适应并发控制
   - 改进退避策略的同步机制

---

## 参考资料

- [ADR-007: 弱网容错分析](project/adr/007-weak-network-resilience-analysis.md)
- [ADR-006: 网络性能优化](project/adr/006-network-performance-optimization.md)
- [性能优化报告](PERFORMANCE_OPTIMIZATION_REPORT.md)
- [架构文档](developer/ARCHITECTURE.md)
