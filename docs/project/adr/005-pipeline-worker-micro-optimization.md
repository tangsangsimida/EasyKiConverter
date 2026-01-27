# ADR-005: 流水线 Worker 微观性能优化

## 状态
已接受 (Accepted)

## 日期
2026-01-27

## 背景 (Context)

在 ADR-003 中，我们通过宏观架构调整（如移除 ProcessWorker 中的网络请求、使用 QSharedPointer、WriteWorker 并行写入）显著提升了性能。然而，在后续的性能分析（2026-01-27）中发现，尽管架构正确，但微观实现上存在“过度并行”和资源浪费问题，导致并行优势未完全发挥，甚至在某些场景下反噬性能。

具体问题：
1.  **FetchWorker (过度初始化)**: 每个组件任务都创建一个新的 `QNetworkAccessManager` (QNAM)。QNAM 初始化非常昂贵（涉及加载网络配置、代理、DNS 缓存等），在批量导出时（如 100 个组件）会重复初始化 100 次。
2.  **WriteWorker (过度并行)**: 每个组件的 `WriteWorker` 都在内部创建一个局部的 `QThreadPool` 来并行写入 3 个小文件（Symbol, Footprint, 3D Model）。线程池的创建与销毁开销远大于写入这 3 个小文件的收益，导致严重的上下文切换和 CPU 抖动。

## 决策 (Decision)

我们决定对 `FetchWorker` 和 `WriteWorker` 进行微观层面的优化：

1.  **WriteWorker: 移除内部线程池，改为串行写入**
    -   **变更**: 移除 `WriteWorker::run()` 中创建的局部 `QThreadPool`，直接按顺序调用 `writeSymbolFile`、`writeFootprintFile` 和 `write3DModelFile`。
    -   **理由**: 对于极小的文件写入操作，串行执行的开销远小于线程池管理和上下文切换的开销。Write 阶段本身的并行度已由流水线架构中的全局 `WriteThreadPool`（8 线程）保证。

2.  **FetchWorker: 使用线程局部存储 (Thread-Local Storage) 复用 QNAM**
    -   **变更**: 在 `FetchWorker::run()` 中使用 `static thread_local QNetworkAccessManager*` 来缓存 QNAM 实例。
    -   **理由**: `QThreadPool` 会复用线程。通过 TLS，我们可以确保每个工作线程只初始化一次 QNAM，并在后续任务中复用它。这消除了昂贵的初始化成本，同时避免了复杂的跨线程对象管理。

## 后果 (Consequences)

### 正面影响
*   **消除资源浪费**: 避免了数百次的 `QNetworkAccessManager` 和 `QThreadPool` 创建/销毁。
*   **降低 CPU 负载**: 减少了不必要的上下文切换和初始化计算。
*   **提升稳定性**: 减少了瞬间创建大量线程的压力，降低了系统资源耗尽的风险。
*   **代码简化**: `WriteWorker` 的代码逻辑变得更线性、更简单。

### 负面影响
*   **内存占用微增**: `FetchWorker` 中的 TLS 缓存意味着每个活跃的 Fetch 线程都会持有一个 QNAM 实例（最多 32 个），直到线程销毁。这被认为是可接受的权衡。
*   **隐式依赖**: TLS 的生命周期由线程池管理，可能导致 QNAM 的析构时机不可控（通常在应用退出或线程过期时），但在本应用场景下无副作用。

## 参考资料
- [ADR-003: 流水线性能优化](./003-pipeline-performance-optimization.md)
- [PERFORMANCE_ANALYSIS_20260127.md](../../developer/PERFORMANCE_ANALYSIS_20260127.md)
