# Weak Network Support Analysis Report

## Overview

This report documents the weak network resilience analysis of the EasyKiConverter project at version v3.0.4, including evaluation of timeout/retry/backoff mechanisms across all network components, identified issues, and improvement recommendations.

## Analysis Date

2026-02-12

## Network Architecture Overview

The project contains **four independent network request implementations** with significantly different weak network resilience capabilities:

| Component | Purpose | Timeout | Retry | Rate Limit Backoff |
|-----------|---------|---------|-------|-------------------|
| `FetchWorker` | Pipeline batch export (new) | Yes 8-10s | Yes 3x (no retry on timeout) | Yes (exponential ×2) |
| `NetworkUtils` | Single component preview | Yes 60s | Yes 5x | Yes |
| `NetworkWorker` | Legacy single fetch | Yes (QTimer) | Yes 3x | No |
| `ComponentService` | LCSC preview images | Yes 15-45s | Yes 3x | No |
| `PreviewImageExporter` | Preview image export | **No** | No | No |

---

## Critical Issues

### Issue 1: `FetchWorker` Does Not Retry After Timeout

**Severity**: Critical

**Description**:

In the `FetchWorker::httpGet` method, timeout errors (`QNetworkReply::OperationCanceledError`) are treated as unrecoverable, skipping all remaining retries:

```cpp
} else if (replyPtr->error() == QNetworkReply::OperationCanceledError) {
    if (m_isAborted.loadRelaxed()) {
        return QByteArray();
    }
    // No retry after timeout
    return QByteArray();
}
```

In weak network environments, timeouts are the most common error type. Not retrying on timeout means that under weak network conditions, nearly all requests fail on the first attempt, rendering the 3-retry mechanism ineffective.

For comparison, `NetworkUtils` in the same project correctly retries timed-out requests.

**Code Location**: `src/workers/FetchWorker.cpp`

**Impact**: All network requests in the pipeline batch export

---

### Issue 2: `PreviewImageExporter` / `DatasheetExporter` Lack Timeout and Retry Protection

**Severity**: Critical

**Description**:

`PreviewImageExporter` and `DatasheetExporter` use unprotected `QEventLoop::exec()` that waits indefinitely:

```cpp
// PreviewImageExporter.cpp:62-66
QEventLoop loop;
connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
loop.exec();  // No timeout, no retry
```

Under weak network conditions, these may block permanently with no retry mechanism.

**Code Location**: `src/services/PreviewImageExporter.cpp`, `src/services/DatasheetExporter.cpp`

**Impact**: Preview image and datasheet download scenarios

---

## Moderate Issues

### Issue 3: `FetchWorker` Timeout Values Too Short

**Severity**: Moderate

**Description**:

```cpp
static const int COMPONENT_INFO_TIMEOUT_MS = 8000;   // Component info timeout
static const int MODEL_3D_TIMEOUT_MS = 10000;         // 3D model timeout
```

Under weak network conditions (RTT > 2s), 8 seconds is extremely tight for a complete HTTPS request (DNS + TLS + HTTP), especially for initial connections without connection reuse.

**Code Location**: `src/workers/FetchWorker.h`

---

### Issue 4: Thundering Herd Effect in Pipeline Fetch Stage

**Severity**: Moderate

**Description**:

`startFetchStage()` submits all component `FetchWorker` instances to the thread pool at once. While the thread pool is limited to 5 concurrent workers, under weak network conditions:

- 5 concurrent requests compete for limited bandwidth simultaneously
- All requests timeout nearly simultaneously and retry together, creating a thundering herd effect
- Rate limit backoff delays use `QThread::msleep()` to block threads, potentially causing thread pool starvation

**Code Location**: `src/services/ExportService_Pipeline.cpp`

---

### Issue 5: `FetchWorker` Retry Delay Too Short

**Severity**: Moderate

**Description**:

```cpp
static const int HTTP_RETRY_DELAY_MS = 500;  // Fixed 500ms
```

Compared to `NetworkUtils` which uses an incremental delay strategy (3s -> 5s -> 10s), `FetchWorker`'s fixed 500ms delay is too aggressive for weak network environments.

**Code Location**: `src/workers/FetchWorker.h`

---

### Issue 6: Timeout Inconsistency Between Preview Images and 3D Models

**Severity**: Low

**Description**:

`ComponentService::downloadLcscImage` sets a 15s timeout for image downloads (typically 10KB-100KB), while `FetchWorker` sets only 10s for 3D model data downloads that may be several MB. The data volume and timeout values are disproportionate.

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

| Scenario | Trigger Condition | Behavior | Severity |
|----------|------------------|----------|----------|
| Batch export total failure | Weak network + `FetchWorker` no timeout retry | All components marked failed | **Critical** |
| Preview/datasheet permanent block | `PreviewImageExporter` used under weak network | UI unresponsive | **Critical** |
| Initial request timeout | RTT > 2s + DNS/TLS needed | 8s insufficient for connection | **Moderate** |
| Thundering herd | 5 concurrent timeouts + simultaneous retries | Bandwidth saturated then timeout | **Moderate** |
| Ineffective retries | Fixed 500ms delay + no timeout retry | Only effective for non-timeout errors | **Moderate** |

---

## Improvement Recommendations

### P0 Improvements (Recommended Priority)

1. **`FetchWorker` should retry after timeout**: Remove the logic that skips retry on `OperationCanceledError`
2. **Increase timeout values appropriately**: Component info timeout recommended 15-20s, 3D model timeout recommended 30s

### P1 Improvements (Recommended Follow-up)

3. **Add timeout to `PreviewImageExporter`/`DatasheetExporter`**: Use QTimer to protect `QEventLoop::exec()`
4. **Use incremental retry delays**: Reference `NetworkUtils` delay strategy (3s -> 5s -> 10s) instead of 500ms fixed delay
5. **Add jitter**: Introduce random jitter to retry delays to avoid thundering herd effects

---

## References

- [ADR-007: Weak Network Resilience Analysis](project/adr/007-weak-network-resilience-analysis_en.md)
- [ADR-006: Network Performance Optimization](project/adr/006-network-performance-optimization.md)
- [Performance Optimization Report](PERFORMANCE_OPTIMIZATION_REPORT_en.md)
- [Architecture](developer/ARCHITECTURE_en.md)
