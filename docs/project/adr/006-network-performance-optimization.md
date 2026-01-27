# ADR 006: 网络性能优化

## 状态

已接受 (2026-01-27)

## 背景

在 v3.0.0-v3.0.4 版本中，虽然实现了三阶段流水线并行架构，但批量导出性能仍存在以下问题：

1. **总耗时过长**：21个组件导出需要263秒（约4.4分钟）
2. **大量组件超时**：21个组件全部超过3秒的目标
3. **严重的速率限制**：16个并发线程触发 EasyEDA API 限流
4. **缺乏诊断信息**：无法精确定位网络瓶颈

## 决策

实施以下优化措施：

### 1. 优化线程池配置

**问题**：16个并发线程触发服务器限流，导致性能急剧下降。

**解决方案**：
- 将 `FetchWorker` 线程池从 16 线程降低至 3 线程
- 经过 3/5/7 线程对比测试，确定 3 线程为最优配置

**实现**：
```cpp
// src/services/ExportService_Pipeline.cpp
m_fetchThreadPool->setMaxThreadCount(3);  // 从16改为3
```

**结果**：
- 总耗时从 263秒降至 14秒（94.5%改进）
- 超过3秒组件从 21个降至 3个（85.7%改进）
- 无超时请求，无速率限制

### 2. 降低超时时间

**问题**：过长的超时时间（30-60秒）导致慢速组件阻塞整个流水线。

**解决方案**：
- 组件信息超时：30秒 → 15秒
- 3D模型超时：60秒 → 30秒
- 超时后不再重试，避免浪费时间

**实现**：
```cpp
// src/workers/FetchWorker.cpp
QByteArray componentInfoData = httpGet(componentInfoUrl, 15000);  // 从30秒改为15秒
QByteArray objData = httpGet(objUrl, 30000);                       // 从60秒改为30秒
QByteArray stepData = httpGet(stepUrl, 30000);                     // 从60秒改为30秒
```

**结果**：
- 快速失败，避免等待超时
- 整体吞吐量提升

### 3. 实现速率限制检测机制

**问题**：无法检测和应对 EasyEDA API 的速率限制。

**解决方案**：
- 检测 HTTP 429 响应
- 实现指数退避策略
- 动态延迟新请求

**实现**：
```cpp
// src/workers/FetchWorker.h
static QAtomicInt s_activeRequests;
static QMutex s_rateLimitMutex;
static QDateTime s_lastRateLimitTime;
static int s_backoffMs;

// src/workers/FetchWorker.cpp
if (statusCode == 429) {
    QMutexLocker locker(&s_rateLimitMutex);
    s_lastRateLimitTime = QDateTime::currentDateTime();
    s_backoffMs = qMin(s_backoffMs + 1000, 5000);  // 指数退避，最大5秒
}
```

**结果**：
- 无速率限制命中（3线程配置下）
- 具备自适应能力，防止未来限流

### 4. 启用网络诊断功能

**问题**：缺乏网络性能诊断信息，无法定位瓶颈。

**解决方案**：
- 记录每个网络请求的诊断信息
- 在统计报告中汇总网络数据

**实现**：
```cpp
// src/models/ComponentExportStatus.h
struct NetworkDiagnostics {
    QString url;
    int statusCode = 0;
    QString errorString;
    int retryCount = 0;
    qint64 latencyMs = 0;
    bool wasRateLimited = false;
};
QList<NetworkDiagnostics> networkDiagnostics;

// src/services/ExportService_Pipeline.cpp
// 在 generateStatistics() 中收集网络诊断
statistics.totalNetworkRequests++;
statistics.totalRetries += diag.retryCount;
statistics.avgNetworkLatencyMs += diag.latencyMs;
```

**结果**：
- 导出报告包含详细的网络诊断信息
- 可快速定位性能瓶颈

## 线程数对比测试

| 线程数 | 总耗时 | 平均抓取 | 超过3秒组件 | 超时请求 | 推荐度 |
|--------|--------|----------|-------------|----------|--------|
| 3线程 | 14.43秒 | 1,758ms | 3个 | 0个 | ⭐⭐⭐⭐⭐ |
| 5线程 | 12.17秒 | 2,023ms | 5个 | 0个 | ⭐⭐⭐⭐ |
| 7线程 | 14.96秒 | 3,400ms | 10个 | 0个 | ⭐⭐⭐ |
| 16线程 | 263.72秒 | 65,815ms | 21个 | 未知 | ⭐ |

**结论**：
- 3线程最符合"单个导出不超过3秒"的要求
- 5线程总耗时最短，但有5个组件超标
- 7线程及以上性能下降明显

## 性能改进总结

| 指标 | 优化前（16线程） | 优化后（3线程） | 改进幅度 |
|------|----------------|----------------|----------|
| 总耗时 | 263.72秒 | 14.43秒 | ⬇️ 94.5% |
| 吞吐量 | 0.08组件/秒 | 1.45组件/秒 | ⬆️ 1712% |
| 平均抓取时间 | 65.8秒 | 1.76秒 | ⬇️ 97.3% |
| 平均网络延迟 | ~5秒（估算） | 0.59秒 | ⬇️ 88.2% |
| 超过3秒组件 | 21个 | 3个 | ⬇️ 85.7% |
| 超时请求 | 未知 | 0个 | ✅ 完全消除 |

## 后续优化方向

1. **自适应并发策略**：根据网络状况动态调整线程数
2. **智能组件排序**：优先处理已知快速组件
3. **慢速组件黑名单**：记录慢速组件，单独处理
4. **分阶段超时**：对慢速组件自动延长超时时间

## 参考文档

- [ADR-002: 导出流水线并行](002-pipeline-parallelism-for-export.md)
- [ADR-003: 流水线性能优化](003-pipeline-performance-optimization.md)
- [性能优化报告](../../PERFORMANCE_OPTIMIZATION_REPORT.md)