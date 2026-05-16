# ADR 006: Network Performance Optimization

## Status
Accepted (2026-01-27)

## Context

In v3.0.0-v3.0.4, although the three-stage pipeline parallel architecture was implemented, batch export performance still had the following issues:

1. **Total time too long**: 21-component export took 263 seconds (~4.4 minutes)
2. **Many component timeouts**: All 21 components exceeded the 3-second target
3. **Severe rate limiting**: 16 concurrent threads triggered EasyEDA API rate limiting
4. **Lack of diagnostic information**: Unable to precisely locate network bottlenecks

## Decision

Implement the following optimizations:

### 1. Optimize Thread Pool Configuration

**Problem**: 16 concurrent threads triggered server rate limiting, causing sharp performance degradation.

**Solution**:
- Reduce `FetchWorker` thread pool from 16 threads to 3 threads
- After 3/5/7 thread comparison testing, 3 threads determined as optimal configuration

**Implementation**:
```cpp
// src/services/ExportService_Pipeline.cpp
m_fetchThreadPool->setMaxThreadCount(3);  // Changed from 16 to 3
```

**Result**:
- Total time reduced from 263s to 14s (94.5% improvement)
- Components exceeding 3s reduced from 21 to 3 (85.7% improvement)
- No timeout requests, no rate limiting

### 2. Reduce Timeout Duration

**Problem**: Excessive timeout duration (30-60s) caused slow components to block the entire pipeline.

**Solution**:
- Component info timeout: 30s → 15s
- 3D model timeout: 60s → 30s
- No retry after timeout to avoid wasting time

**Implementation**:
```cpp
// src/workers/FetchWorker.cpp
QByteArray componentInfoData = httpGet(componentInfoUrl, 15000);  // Changed from 30s to 15s
QByteArray objData = httpGet(objUrl, 30000);                       // Changed from 60s to 30s
QByteArray stepData = httpGet(stepUrl, 30000);                    // Changed from 60s to 30s
```

**Result**:
- Fail fast, avoid waiting for timeouts
- Overall throughput improved

### 3. Implement Rate Limiting Detection

**Problem**: Unable to detect and respond to EasyEDA API rate limiting.

**Solution**:
- Detect HTTP 429 responses
- Implement exponential backoff strategy
- Dynamically delay new requests

**Implementation**:
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
    s_backoffMs = qMin(s_backoffMs + 1000, 5000);  // Exponential backoff, max 5s
}
```

**Result**:
- No rate limiting hits (under 3-thread configuration)
- Adaptive capability, prevents future rate limiting

### 4. Enable Network Diagnostics

**Problem**: Lack of network performance diagnostic information, unable to locate bottlenecks.

**Solution**:
- Record diagnostic information for each network request
- Summarize network data in statistics report

**Implementation**:
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
// Collect network diagnostics in generateStatistics()
statistics.totalNetworkRequests++;
statistics.totalRetries += diag.retryCount;
statistics.avgNetworkLatencyMs += diag.latencyMs;
```

**Result**:
- Export reports include detailed network diagnostic information
- Can quickly locate performance bottlenecks

## Thread Count Comparison Test

| Threads | Total Time | Avg Fetch | Components >3s | Timeout Requests | Recommendation |
|---------|-----------|----------|----------------|-----------------|-----------------|
| 3 threads | 14.43s | 1,758ms | 3 | 0 | ★★★★★ |
| 5 threads | 12.17s | 2,023ms | 5 | 0 | ★★★★ |
| 7 threads | 14.96s | 3,400ms | 10 | 0 | ★★★ |
| 16 threads | 263.72s | 65,815ms | 21 | Unknown | ★ |

**Conclusion**:
- 3 threads best meets the "individual export ≤3 seconds" requirement
- 5 threads has shortest total time but has 5 components exceeding standard
- 7+ threads shows obvious performance degradation

## Performance Improvement Summary

| Metric | Before (16 threads) | After (3 threads) | Improvement |
|--------|-------------------|-------------------|--------------|
| Total time | 263.72s | 14.43s | [Reduction] 94.5% |
| Throughput | 0.08 components/s | 1.45 components/s | [Increase] 1712% |
| Avg fetch time | 65.8s | 1.76s | [Reduction] 97.3% |
| Avg network latency | ~5s (estimated) | 0.59s | [Reduction] 88.2% |
| Components >3s | 21 | 3 | [Reduction] 85.7% |
| Timeout requests | Unknown | 0 | [Completely eliminated] |

## Future Optimization Directions

1. **Adaptive concurrency strategy**: Dynamically adjust thread count based on network conditions
2. **Smart component ordering**: Prioritize processing known fast components
3. **Slow component blacklist**: Record slow components, handle separately
4. **Staged timeouts**: Automatically extend timeout for slow components

## References

- [ADR-002: Export Pipeline Parallelism](002-pipeline-parallelism-for-export.md)
- [ADR-003: Pipeline Performance Optimization](003-pipeline-performance-optimization.md)
- [Performance Optimization Report](../../PERFORMANCE_OPTIMIZATION_REPORT.md)