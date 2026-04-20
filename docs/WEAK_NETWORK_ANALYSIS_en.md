# Weak Network Support Analysis Report

## Overview

This report documents the weak network resilience analysis of the EasyKiConverter project, including evaluation of timeout/retry/backoff mechanisms across all network components, identified issues, and improvement recommendations.

## Analysis Date

2026-02-12 (Document updated: 2026-04-20)

## Network Architecture Overview

The project contains **multiple independent network request implementations** with significantly different weak network resilience capabilities:

| Component | Purpose | Timeout | Retry | Rate Limit Backoff |
|-----------|---------|---------|-------|-------------------|
| `FetchWorker` | Pipeline batch export | Yes 15-30s | Yes 3x (with timeout retry) | Yes (exponential ×2) |
| `NetworkUtils` | Single component preview | Yes 60s | Yes 5x | Yes |
| `NetworkWorker` | Legacy single fetch | Yes (QTimer) | Yes 3x | No |
| `ComponentService` | LCSC preview images | Yes 15-45s | Yes 3x | No |
| `PreviewImageExporter` | Preview image export | Yes (30s via NetworkClient) | Yes (3x) | Yes |
| `DatasheetExporter` | Datasheet export | Yes (30s via NetworkClient) | Yes (3x) | Yes |

---

## Fixed Issues

### ✅ Issue 1: `FetchWorker` Does Not Retry After Timeout (FIXED)

**Original Severity**: Critical

**Fix**:

`AsyncNetworkRequest::shouldRetryInternal` now includes `QNetworkReply::TimeoutError` as a retryable error:

```cpp
switch (error) {
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    // ... other errors
        return true;
    default:
        return false;
}
```

**Code Location**: `src/core/network/AsyncNetworkRequest.cpp`

---

### ✅ Issue 2: `PreviewImageExporter` / `DatasheetExporter` Lack Timeout Protection (FIXED)

**Original Severity**: Critical

**Fix**:

Now uses `NetworkClient::instance().get()` instead of unprotected `QEventLoop::exec()`:

```cpp
// Use NetworkClient with default retry policy (3 retries, 30s timeout)
NetworkResult result = NetworkClient::instance().get(QUrl(imageUrl), ResourceType::PreviewImage);
```

**Code Location**: `src/services/PreviewImageExporter.cpp`, `src/services/DatasheetExporter.cpp`

---

### ✅ Issue 3: `FetchWorker` Timeout Values Too Short (FIXED)

**Original Severity**: Moderate

**Fix**:

Timeout values have been increased:

```cpp
static const int COMPONENT_INFO_TIMEOUT_MS = 15000;  // Component info timeout (ms)
static const int MODEL_3D_TIMEOUT_MS = 30000;         // 3D model timeout (ms)
```

**Code Location**: `src/workers/FetchWorker.h`

---

### ✅ Issue 5: `FetchWorker` Retry Delay Too Short (FIXED)

**Original Severity**: Moderate

**Fix**:

Now uses incremental delay strategy with random jitter:

```cpp
static constexpr int RETRY_DELAYS_MS[] = {3000, 5000, 10000};

int FetchWorker::calculateRetryDelay(int retryCount) {
    int baseDelay = RETRY_DELAYS_MS[retryCount];
    int jitter = static_cast<int>(baseDelay * 0.2);
    int randomOffset = QRandomGenerator::global()->bounded(-jitter, jitter + 1);
    return qMax(100, baseDelay + randomOffset);
}
```

**Code Location**: `src/workers/FetchWorker.cpp`

---

### ✅ Issue 6: Timeout Inconsistency Between Preview Images and 3D Models (FIXED)

**Original Severity**: Low

**Fix**:

3D model timeout increased to 30s, proportional to data volume.

---

## Remaining Issues

### ⚠️ Issue 4: Thundering Herd Effect in Pipeline Fetch Stage

**Severity**: Moderate

**Description**:

`startFetchStage()` submits all component `FetchWorker` instances to the thread pool at once. While the thread pool is limited to 5 concurrent workers, under weak network conditions:

- 5 concurrent requests compete for limited bandwidth simultaneously
- All requests timeout nearly simultaneously and retry together, creating a thundering herd effect
- Rate limit backoff delays use `QThread::msleep()` to block threads, potentially causing thread pool starvation

**Code Location**: `src/services/ExportService_Pipeline.cpp`

---

## Existing Weak Network Support

### Positive Mechanisms Already Implemented

1. **`NetworkWorker` timeout protection**: Uses QTimer + abort() for timeout interruption
2. **`NetworkUtils` comprehensive retry system**: Incremental delays (3s -> 5s -> 10s) + timeout retries + retryable status code recognition (429/500/502/503/504)
3. **`FetchWorker` rate limit backoff**: Detects HTTP 429 and applies global backoff (exponential ×2) shared across all Workers
4. **`LcscImageService` fallback hierarchy**: Primary API failure triggers fallback web scraping approach
5. **`FetchWorker` abort support**: Supports cancellation via `QAtomicInt m_isAborted` and `m_currentReply->abort()`
6. **Network diagnostics collection**: `ComponentExportStatus::NetworkDiagnostics` records latency, retry counts, and status codes per request
7. **Export failure retry feature**: UI supports "retry export" for failed components

---

## Weak Network Anomaly Summary

| Scenario | Trigger Condition | Behavior | Severity | Status |
|----------|------------------|----------|----------|--------|
| Batch export total failure | ~~Weak network + `FetchWorker` no timeout retry~~ | ~~All components marked failed~~ | ~~**Critical**~~ | ✅ **Fixed** |
| Preview/datasheet permanent block | ~~`PreviewImageExporter` used under weak network~~ | ~~UI unresponsive~~ | ~~**Critical**~~ | ✅ **Fixed** |
| Initial request timeout | ~~RTT > 2s + DNS/TLS needed + 8s insufficient~~ | ~~Timeout~~ | ~~**Moderate**~~ | ✅ **Fixed** |
| Thundering herd | 5 concurrent timeouts + simultaneous retries | Bandwidth saturated then timeout | **Moderate** | ⚠️ **Still exists** |
| Ineffective retries | ~~Fixed 500ms delay + no timeout retry~~ | ~~Only effective for non-timeout errors~~ | ~~**Moderate**~~ | ✅ **Fixed** |

---

## Improvement Recommendations

### ✅ P0 Improvements (COMPLETED)

1. **`FetchWorker` should retry after timeout** ✅ Implemented - `AsyncNetworkRequest::shouldRetryInternal` includes `TimeoutError`
2. **Increase timeout values appropriately** ✅ Implemented - Component info 15s, 3D model 30s

### ✅ P1 Improvements (COMPLETED)

3. **Add timeout to `PreviewImageExporter`/`DatasheetExporter`** ✅ Implemented - Uses `NetworkClient` instead of `QEventLoop::exec()`
4. **Use incremental retry delays** ✅ Implemented - 3s -> 5s -> 10s incremental delays
5. **Add jitter** ✅ Implemented - 20% random jitter

### ⚠️ Still Needs Improvement

6. **Resolve thundering herd effect**: Further optimization needed for Fetch stage concurrency strategy:
   - Submit requests in batches to avoid simultaneous timeouts and retries
   - Use adaptive concurrency control
   - Improve backoff strategy synchronization

---

## References

- [ADR-007: Weak Network Resilience Analysis](project/adr/007-weak-network-resilience-analysis_en.md)
- [ADR-006: Network Performance Optimization](project/adr/006-network-performance-optimization.md)
- [Performance Optimization Report](PERFORMANCE_OPTIMIZATION_REPORT_en.md)
- [Architecture](developer/ARCHITECTURE_en.md)
