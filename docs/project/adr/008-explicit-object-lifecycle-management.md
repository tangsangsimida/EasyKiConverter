# ADR 008: 显式对象生命周期管理与安全关闭机制

## 状态
已接受

## 上下文
在应用程序关闭阶段，发生了 `SIGSEGV`（段错误），崩溃点通常位于 `QThreadPool::waitForDone` 或 `QThread` 内部。

分析发现，主要原因是：
1. **竞争条件**：异步日志线程（`FileAppender`）或后台工作线程在 `QApplication` 销毁后仍在尝试访问已回收的全局资源。
2. **父子对象销毁顺序不确定**：当 `ViewModels` 和 `Services` 挂载在 `app` 下时，它们在 `main` 函数返回后的极晚期才被销毁。此时 Qt 核心库可能已开始卸载 DLL 或清理基础设施。
3. **阻塞等待风险**：在析构函数中使用 `waitForDone` 等待全局线程池，在退出阶段是不稳定的。

## 决策
为了确保应用程序能够平稳退出，我们决定实施显式的生命周期管理策略：

1. **移除隐式父子关系**：在 `main.cpp` 中创建核心业务对象（Service, ViewModel）时不指定父对象（`nullptr`）。
2. **显式按序手动销毁**：在 `app.exec()` 返回（主事件循环结束）后，按照依赖关系的逆序手动销毁对象：
   - 先 `delete` ViewModels（停止 UI 触发的异步任务）。
   - 再 `delete` Services（停止后台 Pipeline 和工作线程）。
3. **日志系统保底退出**：在所有业务对象及线程安全退出后，最后执行：
   - `QtLogAdapter::uninstall()`：断开 Qt 内部消息到日志系统的连接。
   - `Logger::instance()->close()`：安全关闭异步写入线程。
4. **移除析构阻塞**：禁止在 ViewModels 的析构函数中调用 `QThreadPool::waitForDone`。改为使用 `QPointer` 结合异步 Invoke 的安全模式，确保任务在对象销毁后自动失效。

## 后果
### 积极影响
- 解决了程序退出时的 `SIGSEGV` 崩溃问题。
- 生命周期更加透明，消除了 Qt 晚期退出阶段的各种“悬挂指针”风险。
- 确保了退出过程中的日志能被完整保存。

### 消极影响
- 在 `main.cpp` 中需要手动维护对象销毁列表，增加了一点维护成本。
