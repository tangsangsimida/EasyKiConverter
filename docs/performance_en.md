# 🚀 Performance Optimization Guide

## 📊 Optimization Goals
- Improve data parsing speed
- Reduce memory usage
- Accelerate file generation process

## 🔧 Optimization Strategies

1. **Multithreading**
   - Use C++11's std::thread or Qt's QThread to implement multithreading
   - Move time-consuming operations (such as network requests, data parsing) to worker threads

2. **Data Structure Optimization**
   - Use more efficient data structures (e.g., std::unordered_map instead of std::map)
   - Avoid unnecessary object copies

3. **IO Optimization**
   - Use buffered IO operations
   - Batch write files to reduce disk access times

4. **Compiler Optimization**
   - Enable compiler optimization (-O2 or -O3) in release builds
   - Use Profile-Guided Optimization (PGO)

5. **Debugging Tools**
   - Use gprof for performance profiling
   - Use Valgrind to check for memory leaks

## 📈 Performance Testing

- Use Google Benchmark for benchmarking
- Write unit tests to verify the performance of critical functions
- Run performance tests regularly to monitor changes

## 📂 Directory Structure
Refer to [Project Structure](project_structure_en.md) document