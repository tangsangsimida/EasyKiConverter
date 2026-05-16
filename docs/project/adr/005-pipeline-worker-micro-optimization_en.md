# ADR-005: Pipeline Worker Micro-Performance Optimization

## Status
Accepted

## Date
2026-01-27

## Context

In ADR-003, we significantly improved performance through macro architecture adjustments (such as removing network requests from ProcessWorker, using QSharedPointer, parallel WriteWorker writes). However, performance profiling (2026-01-27) revealed that despite correct architecture, micro-implementation had "over-parallelism" and resource waste issues, causing parallel advantages to not be fully realized, and even causing performance degradation in some scenarios.

Specific issues:
1. **FetchWorker (Excessive Initialization)**: Each component task created a new `QNetworkAccessManager` (QNAM). QNAM initialization is very expensive (involving loading network configuration, proxy, DNS cache, etc.), and in batch exports (e.g., 100 components) this would initialize QNAM 100 times repeatedly.
2. **WriteWorker (Over-parallelism)**: Each component's `WriteWorker` internally created a local `QThreadPool` to parallel-write 3 small files (Symbol, Footprint, 3D Model). Thread pool creation and destruction overhead far exceeded the benefit of writing these 3 small files, causing severe context switching and CPU thrashing.

## Decision

We decided to optimize `FetchWorker` and `WriteWorker` at the micro level:

1. **WriteWorker: Remove Internal Thread Pool, Use Serial Writes**
   - **Change**: Remove the local `QThreadPool` created in `WriteWorker::run()`, directly call `writeSymbolFile`, `writeFootprintFile`, and `write3DModelFile` in sequence.
   - **Rationale**: For extremely small file write operations, serial execution overhead is far less than thread pool management and context switching overhead. Write stage parallelism is already guaranteed by the global `WriteThreadPool` (8 threads) in the pipeline architecture.

2. **FetchWorker: Use Thread-Local Storage (TLS) to Reuse QNAM**
   - **Change**: Use `static thread_local QNetworkAccessManager*` in `FetchWorker::run()` to cache QNAM instances.
   - **Rationale**: `QThreadPool` reuses threads. Through TLS, we ensure each worker thread initializes QNAM only once and reuses it in subsequent tasks. This eliminates expensive initialization costs while avoiding complex cross-thread object management.

## Consequences

### Positive
- **Eliminated resource waste**: Avoided hundreds of `QNetworkAccessManager` and `QThreadPool` create/destroy cycles.
- **Reduced CPU load**: Decreased unnecessary context switching and initialization computation.
- **Improved stability**: Reduced pressure from creating many threads instantaneously, lowering risk of system resource exhaustion.
- **Code simplification**: `WriteWorker` code logic became more linear and simpler.

### Negative
- **Slight memory increase**: TLS caching in `FetchWorker` means each active Fetch thread holds a QNAM instance (up to 32) until the thread is destroyed. This is considered an acceptable trade-off.
- **Implicit dependency**: TLS lifecycle is managed by the thread pool, which may lead to uncontrollable QNAM destructor timing (typically at application exit or thread expiration), but has no side effects in this application scenario.

## References
- [ADR-003: Pipeline Performance Optimization](./003-pipeline-performance-optimization.md)