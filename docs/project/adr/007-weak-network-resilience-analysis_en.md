# ADR 007: Weak Network Resilience Analysis

## Status

Accepted (2026-02-12)

## Context

A comprehensive analysis of the project's weak network resilience was conducted in version v3.0.4, revealing the following critical issues:

1. **Inconsistent network components**: The project has four independent network request implementations (`FetchWorker`, `NetworkUtils`, `NetworkWorker`, `ComponentService`) with inconsistent timeout/retry/backoff strategies
2. **`NetworkWorker` lacks timeout**: Uses `QEventLoop::exec()` to wait indefinitely, potentially blocking threads permanently under weak network conditions
3. **`FetchWorker` does not retry after timeout**: Timeouts are the most common error under weak network conditions, but `FetchWorker` skips retries on timeout
4. **Unreasonable timeout configuration**: `FetchWorker` timeouts (8-10s) are too short for weak network environments

## Analysis Conclusions

### Issue Severity Classification

| Severity | Issue | Impact |
|----------|-------|--------|
| **Critical** | `NetworkWorker` no timeout, permanent block | Application unresponsive |
| **Critical** | `FetchWorker` no retry after timeout | All batch exports fail under weak network |
| **Critical** | Timeout too short (8-10s) | Initial connection likely timeout when RTT > 2s |
| **Moderate** | Thundering herd effect | Bandwidth contention intensified |
| **Moderate** | Linear backoff (not exponential) | Slow recovery from rate limiting |
| **Moderate** | Fixed 500ms retry delay | Too aggressive under weak network |
| **Low** | QNAM thread_local leak | Memory growth during long runs |

### Component Weak Network Capability Comparison

| Component | Timeout | Retry | Backoff Strategy | Weak Network Rating |
|-----------|---------|-------|-----------------|-------------------|
| `FetchWorker` | 8-10s | 3x (no retry on timeout) | Linear +1000ms | Poor |
| `NetworkUtils` | 30s | 3x (retries on timeout) | Incremental 3/5/10s | Good |
| `NetworkWorker` | None | None | None | Very Poor |
| `ComponentService` | 15s | 3x | Incremental 1/2/3s | Fair |

## Decision

### Improvement Directions

Based on the analysis conclusions, improvements are recommended in two priority levels:

#### P0 Improvements (Priority)

1. **Unify network timeout mechanism**
   - Add timeout protection to `NetworkWorker`
   - Evaluate migrating `NetworkWorker` to `FetchWorker` or `NetworkUtils`

2. **Fix `FetchWorker` timeout retry logic**
   - Perform normal retries on `OperationCanceledError` (timeout)
   - Align with `NetworkUtils` timeout retry behavior

3. **Adjust timeout values**
   - Component info: 8s -> 15-20s
   - 3D models: 10s -> 30s
   - Align with `NetworkUtils` timeout configuration

#### P1 Improvements (Follow-up)

4. **Unify backoff strategy**
   - Implement true exponential backoff (1s -> 2s -> 4s -> 8s)
   - Add random jitter to avoid thundering herd effects

5. **Unify retry delays**
   - Adopt `NetworkUtils` incremental delay pattern (3s -> 5s -> 10s)

6. **Fix QNAM memory management**
   - Use `std::unique_ptr` to manage `thread_local` objects

## Consequences

### Positive

- Clarified the current state of weak network support and improvement directions
- Provided clear priority guidance for network resilience improvements in future versions
- Standardized network strategy across components

### Negative

- Improvement work requires additional development and testing time
- Increasing timeout values may slightly increase waiting time under normal network conditions (mitigated by dynamic timeout strategies)

### Risks

- Changes to `NetworkWorker` require confirming whether it is still actively used to avoid unnecessary modifications
- Timeout adjustments need to balance weak network resilience with user experience

## References

- [Weak Network Support Analysis Report](../../WEAK_NETWORK_ANALYSIS_en.md)
- [ADR-006: Network Performance Optimization](006-network-performance-optimization.md)
- [ADR-003: Pipeline Performance Optimization](003-pipeline-performance-optimization.md)
- [Performance Optimization Report](../../PERFORMANCE_OPTIMIZATION_REPORT_en.md)
