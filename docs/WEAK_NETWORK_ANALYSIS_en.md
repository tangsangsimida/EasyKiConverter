# Weak Network Support Analysis Report

## Overview

This report documents the weak network resilience analysis of the EasyKiConverter project at version v3.0.4, including evaluation of timeout/retry/backoff mechanisms across all network components, identified issues, and improvement recommendations.

## Analysis Date

2026-02-12

## Network Architecture Overview

The project contains **four independent network request implementations** with significantly different weak network resilience capabilities:

| Component | Purpose | Timeout | Retry | Rate Limit Backoff |
|-----------|---------|---------|-------|-------------------|
| `FetchWorker` | Pipeline batch export (new) | Yes 8-10s | Yes 3x | Yes |
| `NetworkUtils` | Single component preview | Yes 30s | Yes 3x | No |
| `NetworkWorker` | Legacy single fetch | No | No | No |
| `ComponentService` | LCSC preview images | Yes 15s | Yes 3x | No |

---

## Critical Issues

### Issue 1: `NetworkWorker` Lacks Timeout and Retry Mechanisms

**Severity**: Critical

**Description**:

The four network request methods in `NetworkWorker` (`fetchComponentInfo`, `fetchCadData`, `fetch3DModelObj`, `fetch3DModelMtl`) have the following problems:

- **No timeout**: Uses `QEventLoop::exec()` to wait indefinitely; may block threads permanently under weak network conditions
- **No retries**: Any network error results in immediate failure without retry attempts
- **Creates new `QNetworkAccessManager` per request**: Cannot reuse connection pools, adding extra DNS resolution and TCP handshake overhead

**Code Location**: `src/workers/NetworkWorker.cpp`

**Impact**: Single-component data fetching scenarios using `NetworkWorker`

**Expected Behavior**: Under weak network conditions, worker threads block permanently, thread pool resources cannot be released, potentially causing application unresponsiveness.

---

### Issue 2: `FetchWorker` Does Not Retry After Timeout

**Severity**: Critical

**Description**:

In the `FetchWorker::httpGet` method, timeout errors (`QNetworkReply::OperationCanceledError`) are treated as unrecoverable, skipping all remaining retries:

```cpp
} else if (reply->error() == QNetworkReply::OperationCanceledError) {
    retryCount = MAX_HTTP_RETRIES + 1;  // Skip retries
}
```

In weak network environments, timeouts are the most common error type. Not retrying on timeout means that under weak network conditions, nearly all requests fail on the first attempt, rendering the 3-retry mechanism ineffective.

For comparison, `NetworkUtils` in the same project correctly retries timed-out requests.

**Code Location**: `src/workers/FetchWorker.cpp`

**Impact**: All network requests in the pipeline batch export

---

### Issue 3: `FetchWorker` Timeout Values Too Short

**Severity**: Critical

**Description**:

```cpp
static const int COMPONENT_INFO_TIMEOUT_MS = 8000;   // Component info timeout
static const int MODEL_3D_TIMEOUT_MS = 10000;         // 3D model timeout
```

Under weak network conditions (RTT > 2s), 8 seconds is extremely tight for a complete HTTPS request (DNS + TLS + HTTP), especially for initial connections without connection reuse.

**Code Location**: `src/workers/FetchWorker.h`

---

## Moderate Issues

### Issue 4: Thundering Herd Effect in Pipeline Fetch Stage

**Severity**: Moderate

**Description**:

`startFetchStage()` submits all component `FetchWorker` instances to the thread pool at once. While the thread pool is limited to 5 concurrent workers, under weak network conditions:

- 5 concurrent requests compete for limited bandwidth simultaneously
- All requests timeout nearly simultaneously and retry together, creating a thundering herd effect
- Rate limit backoff delays use `QThread::msleep()` to block threads, potentially causing thread pool starvation

**Code Location**: `src/services/ExportService_Pipeline.cpp`

---

### Issue 5: Rate Limit Backoff Is Not Exponential

**Severity**: Moderate

**Description**:

```cpp
s_backoffMs = qMin(s_backoffMs + 1000, 5000);  // Linear growth, max 5s
```

Despite comments describing "exponential backoff", the actual implementation is linear (+1000ms per iteration), resulting in slower recovery when weak network conditions combine with server-side rate limiting.

**Code Location**: `src/workers/FetchWorker.cpp`

---

### Issue 6: `FetchWorker` Retry Delay Too Short

**Severity**: Moderate

**Description**:

```cpp
static const int HTTP_RETRY_DELAY_MS = 500;  // Fixed 500ms
```

Compared to `NetworkUtils` which uses an incremental delay strategy (3s -> 5s -> 10s), `FetchWorker`'s fixed 500ms delay is too aggressive for weak network environments.

**Code Location**: `src/workers/FetchWorker.h`

---

### Issue 7: Timeout Inconsistency Between Preview Images and 3D Models

**Severity**: Moderate

**Description**:

`ComponentService::downloadLcscImage` sets a 15s timeout for image downloads (typically 10KB-100KB), while `FetchWorker` sets only 10s for 3D model data downloads that may be several MB. The data volume and timeout values are disproportionate.

---

### Issue 8: `thread_local QNetworkAccessManager` Leak Risk

**Severity**: Low

**Description**:

```cpp
static thread_local QNetworkAccessManager* threadQNAM = nullptr;
if (!threadQNAM) {
    threadQNAM = new QNetworkAccessManager();
}
```

The `thread_local` `QNetworkAccessManager` is not properly destructed when threads exit (raw pointer `new` without `delete`). Frequent timeout retries under weak network conditions may exacerbate this issue.

**Code Location**: `src/workers/FetchWorker.cpp`

---

## Existing Weak Network Support

### Positive Mechanisms Already Implemented

1. **`FetchWorker` rate limit backoff**: Detects HTTP 429 and applies global backoff shared across all Workers
2. **`NetworkUtils` comprehensive retry system**: Incremental delays (3s -> 5s -> 10s) + timeout retries + retryable status code recognition (429/500/502/503/504)
3. **`ComponentService` preview image fallback**: Primary API failure triggers fallback web scraping approach
4. **`FetchWorker` abort support**: Supports cancellation via `QAtomicInt m_isAborted` and `m_currentReply->abort()`
5. **Network diagnostics collection**: `ComponentExportStatus::NetworkDiagnostics` records latency, retry counts, and status codes per request
6. **Export failure retry feature**: UI supports "retry export" for failed components

---

## Weak Network Anomaly Summary

| Scenario | Trigger Condition | Behavior | Severity |
|----------|------------------|----------|----------|
| Thread permanent block | `NetworkWorker` used under weak network | Application unresponsive | **Critical** |
| Batch export total failure | Weak network + `FetchWorker` no timeout retry | All components marked failed | **Critical** |
| Initial request timeout | RTT > 2s + DNS/TLS needed | 8s insufficient for connection | **Moderate** |
| Thundering herd | 5 concurrent timeouts + simultaneous retries | Bandwidth saturated then timeout | **Moderate** |
| Ineffective retries | Fixed 500ms delay + no timeout retry | Only effective for non-timeout errors | **Moderate** |
| QNAM memory leak | Long running + frequent timeouts | Gradual memory growth | **Low** |

---

## Improvement Recommendations

### P0 Improvements (Recommended Priority)

1. **Add timeout mechanism to `NetworkWorker`**: Use `QTimer` with `QEventLoop`, or migrate to `FetchWorker`/`NetworkUtils`
2. **`FetchWorker` should retry after timeout**: Remove `retryCount = MAX_HTTP_RETRIES + 1` logic, perform normal retries for timeout errors
3. **Increase timeout values appropriately**: Component info timeout recommended 15-20s, 3D model timeout recommended 30s

### P1 Improvements (Recommended Follow-up)

4. **Implement true exponential backoff**: Change linear backoff to exponential (1s -> 2s -> 4s -> 8s)
5. **Use incremental retry delays**: Reference `NetworkUtils` delay strategy (3s -> 5s -> 10s)
6. **Add jitter**: Introduce random jitter to retry delays to avoid thundering herd effects
7. **Fix QNAM memory leak**: Use `std::unique_ptr` or custom deleter to manage `thread_local` objects

---

## References

- [ADR-006: Network Performance Optimization](project/adr/006-network-performance-optimization.md)
- [ADR-007: Weak Network Resilience Analysis](project/adr/007-weak-network-resilience-analysis_en.md)
- [Performance Optimization Report](PERFORMANCE_OPTIMIZATION_REPORT_en.md)
- [Architecture](developer/ARCHITECTURE_en.md)
