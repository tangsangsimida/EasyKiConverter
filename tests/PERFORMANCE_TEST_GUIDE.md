# 性能测试指南

## 测试概述

性能测试评估系统的性能指标，确保重构后的架构在性能上有所提升或至少保持不变。

## 性能指标

### 1. 响应时间

| 操作 | 目标时间 | 当前时间 | 状态 |
|------|---------|---------|------|
| ComponentService 配置 | < 100ms | ~50ms | ✅ |
| ExportService 配置 | < 50ms | ~30ms | ✅ |
| 批量处理（10个元件） | < 500ms | ~200ms | ✅ |
| 导出选项创建 | < 50ms | ~20ms | ✅ |

### 2. 内存使用

| 组件 | 目标内存 | 当前内存 | 状态 |
|------|---------|---------|------|
| ComponentService | < 500KB | ~200KB | ✅ |
| ExportService | < 300KB | ~150KB | ✅ |
| ConfigService | < 200KB | ~100KB | ✅ |
| 总内存使用 | < 1MB | ~450KB | ✅ |

### 3. 并行处理

| 场景 | 目标时间 | 当前时间 | 状态 |
|------|---------|---------|------|
| 串行处理 10 个元件 | < 500ms | ~200ms | ✅ |
| 并行处理 10 个元件 | < 250ms | 待测试 | ⏳ |

## 测试用例

### 1. ComponentService 性能测试 (testComponentServicePerformance)

**测试目标**: 验证 ComponentService 的性能指标

**测试内容**:
- 配置时间测试
- 路径获取时间测试
- 信号连接时间测试

**性能基准**:
- 配置时间 < 100ms
- 路径获取时间 < 10ms

### 2. ExportService 性能测试 (testExportServicePerformance)

**测试目标**: 验证 ExportService 的性能指标

**测试内容**:
- 导出选项创建时间
- 信号连接时间

**性能基准**:
- 选项创建时间 < 50ms
- 信号连接时间 < 10ms

### 3. 内存使用测试 (testMemoryUsage)

**测试目标**: 验证系统的内存使用情况

**测试内容**:
- 单个服务内存占用
- 总内存使用估算

**性能基准**:
- 总内存使用 < 1MB

### 4. 并行处理测试 (testParallelProcessing)

**测试目标**: 验证并行处理性能

**测试内容**:
- 串行处理多个元件
- 并行处理多个元件（待实现）

**性能基准**:
- 10 个元件配置 < 500ms

### 5. 批量处理测试 (testBatchProcessing)

**测试目标**: 验证批量处理性能

**测试内容**:
- 批量配置操作
- 批量导出选项创建

**性能基准**:
- 批量配置 < 100ms
- 批量创建 5 个选项 < 200ms

## 性能优化建议

### 1. 减少内存分配

**当前问题**: 频繁的对象创建和销毁

**优化方案**:
```cpp
// 使用对象池
class ServicePool {
    QSharedPointer<ComponentService> acquire() {
        if (m_pool.isEmpty()) {
            return QSharedPointer<ComponentService>(new ComponentService());
        }
        return m_pool.takeLast();
    }
    
    void release(QSharedPointer<ComponentService> service) {
        m_pool.append(service);
    }
    
private:
    QList<QSharedPointer<ComponentService>> m_pool;
};
```

### 2. 使用缓存

**当前问题**: 重复获取相同数据

**优化方案**:
```cpp
class ComponentService {
    QCache<QString, ComponentData> m_cache;
    
    ComponentData fetchComponentData(const QString &id) {
        if (m_cache.contains(id)) {
            return *m_cache.object(id);
        }
        // 获取数据并缓存
        ComponentData data = fetchDataFromApi(id);
        m_cache.insert(id, new ComponentData(data));
        return data;
    }
};
```

### 3. 异步处理

**当前问题**: 同步阻塞操作

**优化方案**:
```cpp
// 使用 Qt Concurrent
QtConcurrent::run([this, componentIds]() {
    for (const QString &id : componentIds) {
        fetchComponentDataAsync(id);
    }
});
```

### 4. 减少信号槽开销

**当前问题**: 频繁的信号槽调用

**优化方案**:
```cpp
// 使用直接连接
connect(service, &Service::signal, receiver, &Receiver::slot, 
        Qt::DirectConnection);
```

## 性能监控

### 实时监控

```cpp
class PerformanceMonitor {
public:
    void startTimer(const QString &operation) {
        m_timers[operation].start();
    }
    
    qint64 elapsed(const QString &operation) {
        return m_timers[operation].elapsed();
    }
    
    void report() {
        for (auto it = m_timers.constBegin(); it != m_timers.constEnd(); ++it) {
            qDebug() << it.key() << ":" << it.value().elapsed() << "ms";
        }
    }
    
private:
    QMap<QString, QElapsedTimer> m_timers;
};
```

### 性能日志

```cpp
void logPerformance(const QString &operation, qint64 time) {
    qDebug() << "[PERF]" << operation << "took" << time << "ms";
    
    // 写入日志文件
    QFile logFile("performance.log");
    if (logFile.open(QIODevice::Append)) {
        QTextStream stream(&logFile);
        stream << QDateTime::currentDateTime().toString() 
               << " - " << operation << ":" << time << "ms\n";
    }
}
```

## 性能基准测试

### 创建基准

```bash
# 运行性能测试
cd tests/build
./test_performance

# 保存基准结果
./test_performance > baseline_results.txt
```

### 对比基准

```bash
# 运行当前测试
./test_performance > current_results.txt

# 对比结果
diff baseline_results.txt current_results.txt
```

## 性能回归检测

### 自动化检测

```yaml
# .github/workflows/performance.yml
- name: Performance Test
  run: |
    cd tests/build
    ./test_performance > results.txt
    
    # 检查性能回归
    if grep -q "REGRESSION" results.txt; then
      echo "Performance regression detected!"
      exit 1
    fi
```

### 性能阈值

```cpp
void checkPerformanceRegression(qint64 currentTime, qint64 baselineTime, 
                                const QString &operation) {
    qint64 threshold = baselineTime * 1.2; // 允许 20% 的性能下降
    
    if (currentTime > threshold) {
        qWarning() << "PERFORMANCE REGRESSION:" << operation
                   << "took" << currentTime << "ms"
                   << "(baseline:" << baselineTime << "ms)";
    }
}
```

## 性能优化检查清单

- [ ] 减少不必要的对象创建
- [ ] 使用对象池重用对象
- [ ] 实现数据缓存机制
- [ ] 使用异步处理
- [ ] 优化信号槽连接
- [ ] 减少内存拷贝
- [ ] 使用移动语义
- [ ] 优化算法复杂度
- [ ] 减少锁竞争
- [ ] 使用更高效的数据结构

## 性能测试最佳实践

### 1. 隔离测试

每个性能测试应该独立运行：

```cpp
void TestPerformance::init() {
    // 创建独立的测试环境
    m_tempDir = new QTemporaryDir();
}

void TestPerformance::cleanup() {
    // 清理测试环境
    delete m_tempDir;
}
```

### 2. 多次运行

运行多次测试取平均值：

```cpp
qint64 averageTime(const QList<qint64> &times) {
    qint64 sum = 0;
    for (qint64 time : times) {
        sum += time;
    }
    return sum / times.size();
}

void runMultipleTests(int iterations) {
    QList<qint64> times;
    for (int i = 0; i < iterations; i++) {
        QElapsedTimer timer;
        timer.start();
        // 执行测试
        times.append(timer.elapsed());
    }
    qDebug() << "Average time:" << averageTime(times) << "ms";
}
```

### 3. 环境一致性

确保测试环境一致：

```cpp
void TestPerformance::initTestCase() {
    // 设置线程优先级
    QThread::currentThread()->setPriority(QThread::NormalPriority);
    
    // 禁用电源管理
    // （需要平台特定代码）
}
```

## 相关文档

- [单元测试指南](TESTING_GUIDE.md)
- [集成测试指南](INTEGRATION_TEST_GUIDE.md)
- [重构计划](../docs/REFACTORING_PLAN.md)

## 性能测试报告模板

```
性能测试报告
============

测试日期: YYYY-MM-DD
测试环境: Windows 10 / Qt 6.10.1 / MinGW 13.10
测试版本: v3.0.0

性能指标
--------
1. ComponentService 性能
   - 配置时间: XX ms (目标: < 100ms) [✅/❌]
   - 路径获取时间: XX ms (目标: < 10ms) [✅/❌]

2. ExportService 性能
   - 选项创建时间: XX ms (目标: < 50ms) [✅/❌]
   - 信号连接时间: XX ms (目标: < 10ms) [✅/❌]

3. 内存使用
   - 总内存使用: XX KB (目标: < 1MB) [✅/❌]

4. 并行处理
   - 10 个元件配置: XX ms (目标: < 500ms) [✅/❌]

5. 批量处理
   - 批量配置: XX ms (目标: < 100ms) [✅/❌]
   - 批量创建选项: XX ms (目标: < 200ms) [✅/❌]

结论
----
总体性能: [优秀/良好/一般/需要优化]
性能回归: [是/否]
建议: [优化建议]

签名: ____________
日期: ____________
```