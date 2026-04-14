# ADR 007: Weak Network Resilience Analysis

## Status

Accepted (2026-02-12)

## Context

A comprehensive analysis of the project's weak network resilience was conducted in version v3.0.4, revealing the following critical issues:

1. **Inconsistent network components**: The project has multiple independent network request implementations (`FetchWorker`, `NetworkUtils`, `NetworkWorker`, `ComponentService`) with inconsistent timeout/retry/backoff strategies
2. **`FetchWorker` does not retry after timeout**: Timeouts are the most common error under weak network conditions, but `FetchWorker` skips retries on timeout
3. **`PreviewImageExporter`/`DatasheetExporter` lack timeout protection**: Use unprotected `QEventLoop::exec()`, potentially blocking permanently under weak network
4. **Unreasonable timeout configuration**: `FetchWorker` timeouts (8-10s) are too short for weak network environments

## Analysis Conclusions

### Issue Severity Classification

| Severity | Issue | Impact |
|----------|-------|--------|
| **Critical** | `FetchWorker` no retry after timeout | All batch exports fail under weak network |
| **Critical** | `PreviewImageExporter`/`DatasheetExporter` no timeout | UI unresponsive |
| **Critical** | Timeout too short (8-10s) | Initial connection likely timeout when RTT > 2s |
| **Moderate** | Thundering herd effect | Bandwidth contention intensified |
| **Moderate** | Fixed 500ms retry delay | Too aggressive under weak network |
| **Low** | Timeout inconsistency | Preview images vs 3D models |

### Component Weak Network Capability Comparison

| Component | Timeout | Retry | Backoff Strategy | Weak Network Rating |
|-----------|---------|-------|-----------------|-------------------|
| `FetchWorker` | 8-10s | 3x (no retry on timeout) | Exponential ×2 | Poor |
| `NetworkUtils` | 60s | 5x (retries on timeout) | None (0ms) | Good |
| `NetworkWorker` | Yes (QTimer) | 3x | None | Fair |
| `ComponentService` | 15-45s | 3x | Random | Fair |
| `PreviewImageExporter` | None | None | None | Very Poor |

## Decision

### Improvement Directions

Based on the analysis conclusions, improvements are recommended in two priority levels:

#### P0 Improvements (Priority)

1. **Fix `FetchWorker` timeout retry logic**
   - Perform normal retries on `OperationCanceledError` (timeout)
   - Align with `NetworkUtils` timeout retry behavior

2. **Adjust timeout values**
   - Component info: 8s -> 15-20s
   - 3D models: 10s -> 30s
   - Align with `NetworkUtils` timeout configuration

3. **Add timeout protection to `PreviewImageExporter`/`DatasheetExporter`**
   - Use QTimer to protect `QEventLoop::exec()`

#### P1 Improvements (Follow-up)

4. **Unify retry delays**
   - Adopt `NetworkUtils` incremental delay pattern (3s -> 5s -> 10s) instead of 500ms fixed delay

5. **Add jitter**
   - Add random jitter to retry delays to avoid thundering herd effects

## Consequences

### Positive

- Clarified the current state of weak network support and improvement directions
- Provided clear priority guidance for network resilience improvements in future versions
- Standardized network strategy across components

### Negative

- Improvement work requires additional development and testing time
- Increasing timeout values may slightly increase waiting time under normal network conditions (mitigated by dynamic timeout strategies)

### Risks

- `FetchWorker` timeout retry logic changes require thorough testing to ensure abort scenarios don't cause infinite loops
- Timeout adjustments need to balance weak network resilience with user experience

## References

- [Weak Network Support Analysis Report](../../WEAK_NETWORK_ANALYSIS_en.md)
- [ADR-006: Network Performance Optimization](006-network-performance-optimization.md)
- [ADR-003: Pipeline Performance Optimization](003-pipeline-performance-optimization.md)
- [Performance Optimization Report](../../PERFORMANCE_OPTIMIZATION_REPORT_en.md)
