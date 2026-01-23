# ADR-002: Adopt Multi-Stage Pipeline Parallelism Architecture for Export Functionality

## Status

**Accepted**

## Context

The core functionality of this application is to export a large number of electronic components (symbols, footprints, 3D models) from remote servers to local KiCad library files. The current export process is serial, requiring users to wait for a very long time when batch exporting components, resulting in poor user experience. To significantly improve application performance, maximize hardware utilization, and enhance user experience, we need to design a brand new high-performance parallel processing architecture. This architecture must also support precise real-time progress feedback and detailed export success/failure statistics.

## Decision

We will adopt a **Multi-Stage Pipeline Parallelism** architecture to refactor the entire export service. This architecture decomposes the workflow into three independent, concurrently executable stages:

1.  **Fetch Stage**:
    Use a specialized, large-scale I/O-intensive thread pool to concurrently download raw data for all components through asynchronous network requests.

2.  **Process Stage**:
    Use a specialized, CPU-intensive thread pool sized to match the number of CPU cores to concurrently parse raw data and convert it into structured KiCad data objects.

3.  **Write Stage**: Use a specialized I/O thread pool to write processed data objects to disk. For symbol libraries (`.kicad_sym`), they will be written to independent temporary files first.

Stages communicate through **thread-safe bounded queues**, enabling task decoupling and efficient flow. The `ExportService` will serve as the pipeline's scheduling center, responsible for initialization, status monitoring, and executing a final "merge symbol library temporary files" step after all tasks are completed.

Status feedback is implemented through a well-defined data structure `ComponentExportStatus`, which is progressively populated as it flows through the pipeline, ultimately aggregated by the write stage and fed back to `ExportService` for precise overall progress calculation and detailed statistics generation.

## Consequences

### Positive Impacts

*   **Ultimate Performance**:
    By maximizing overlap of network I/O, CPU computation, and disk I/O, we achieve extremely high concurrency, fully utilizing multi-core CPUs and high-speed networks/disks.

*   **Resource Efficiency**: Optimal thread pool configurations for different task types (I/O-intensive vs CPU-intensive) avoid resource waste.

*   **Precise Diagnostics and Feedback**:
    The architecture clearly reveals bottlenecks in specific stages and provides detailed failure reasons (e.g., whether it was a download failure or data parsing failure).

*   **Strong Scalability**: Future additions of new processing steps (such as DRC checks) only require adding a new stage to the pipeline.

### Negative Impacts

*   **Increased Complexity**: Compared to simple serial or parallel models, pipeline architecture is more complex in both code implementation and logic, requiring careful management of threads, queues, and states.

*   **Higher Debugging Difficulty**: Diagnosing race conditions or deadlocks in multi-threaded environments is more challenging than in single-threaded scenarios.

*   **Increased Code Volume**: Requires defining more work units (Workers), data structures, and management classes.

## Considered Alternatives

### 1. Component-Level Parallel Model (One Thread Per Component)

*   **Description**: Create an independent task for each component to be exported and submit it to a unified thread pool for execution.

*   **Rejection Reason**:
    While providing significant improvement over serial models, efficiency is not optimal. It doesn't distinguish between I/O and CPU-intensive tasks, preventing optimal thread pool configuration. Additionally, it cannot achieve "task overlap" of different components at different processing stages, failing to meet the goal of ultimate performance.

### 2. Component-Level Parallel Model with File Locks

*   **Description**: Based on the "one thread per component" model, use a global mutex lock when writing to symbol libraries (shared files).

*   **Rejection Reason**:
    The mutex becomes a serious performance bottleneck, requiring all threads to queue up when writing symbols, causing this critical step to degrade to serial execution, severely weakening parallelism effectiveness.

## Implementation Details

### Thread Pool Configuration

- **Fetch Stage**: 32 threads (I/O-intensive, configurable)
- **Process Stage**: CPU core count (CPU-intensive, matches CPU count)
- **Write Stage**: 8 threads (disk I/O-intensive, configurable)

### Core Components

1. **BoundedThreadSafeQueue**: Thread-safe bounded queue for inter-stage data transfer
2. **ComponentExportStatus**: Tracks execution status of each component across stages
3. **FetchWorker**: Responsible for network data download
4. **ProcessWorker**: Responsible for data parsing and conversion
5. **WriteWorker**: Responsible for file writing
6. **ExportServicePipeline**: Pipeline scheduling center

### Progress Calculation

- **Fetch Stage**: 30% weight
- **Process Stage**: 50% weight
- **Write Stage**: 20% weight

Overall Progress = (Fetch Progress × 30 + Process Progress × 50 + Write Progress × 20) / 100

## References

- [ADR-001: MVVM Architecture](./001-mvvm-architecture_en.md)
- [ARCHITECTURE.md](../../developer/ARCHITECTURE_en.md)