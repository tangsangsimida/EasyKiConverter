# ADR 008: Explicit Object Lifecycle Management and Safe Shutdown Mechanism

## Status
Accepted

## Context
During application shutdown, `SIGSEGV` (segmentation fault) occurred, with crash points typically located in `QThreadPool::waitForDone` or inside `QThread`.

Analysis revealed the main causes:
1. **Race condition**: Asynchronous log thread (`FileAppender`) or background worker threads tried to access already-reclaimed global resources after `QApplication` destruction.
2. **Uncertain parent-child destruction order**: When ViewModels and Services were parented under `app`, they were destroyed very late after `main` function return. At this point, Qt core libraries may have already started unloading DLLs or cleaning up infrastructure.
3. **Blocking wait risk**: Using `waitForDone` in destructors to wait for global thread pools is unstable during exit phase.

## Decision
To ensure smooth application exit, we decided to implement explicit lifecycle management strategy:

1. **Remove implicit parent-child relationships**: When creating core business objects (Service, ViewModel) in `main.cpp`, do not specify a parent object (`nullptr`).
2. **Explicit manual destruction in dependency reverse order**: After `app.exec()` returns (main event loop ends):
   - First `delete` ViewModels (stop UI-triggered asynchronous tasks).
   - Then `delete` Services (stop background Pipeline and worker threads).
3. **Log system final cleanup**: After all business objects and threads safely exit, finally execute:
   - `QtLogAdapter::uninstall()`: Disconnect Qt internal messages from the log system.
   - `Logger::instance()->close()`: Safely close asynchronous write thread.
4. **Remove destructor blocking**: Prohibit calling `QThreadPool::waitForDone` in ViewModel destructors. Instead, use `QPointer` combined with asynchronous Invoke pattern to ensure tasks automatically invalidate after object destruction.

## Consequences
### Positive
- Resolved `SIGSEGV` crash on program exit.
- Lifecycle is more transparent, eliminating various "dangling pointer" risks during Qt late exit phase.
- Ensures logs during exit are completely saved.

### Negative
- Manual maintenance of object destruction list in `main.cpp` adds slight maintenance cost.