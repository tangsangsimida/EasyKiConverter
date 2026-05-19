# Bug Report: `m_fetchingComponents` 陈旧条目导致验证请求被静默吞掉

## 问题描述

当 `m_fetchingComponents` 中存在"无可复用数据、且实际没有活跃请求"的陈旧条目时，后续对同一元器件的普通验证请求会被 `shouldSkipAsDuplicate` 静默跳过，不发射 `cadDataReady` 或 `fetchError`，导致 UI 列表项永远停在 `validationPhase = "validating"` 状态。

## 根因代码

### 1. 静默跳过路径

`src/services/ComponentService.cpp:173-214` — `fetchComponentDataInternal`

```
if (m_fetchingComponents.contains(normalizedId)) {
    ...
    if (hasReusableData) {
        // 复用数据，发射 cadDataReady ✓
    } else if (m_parallelContext != nullptr) {
        // 并行模式下清除过期条目 ✓
        m_fetchingComponents.remove(normalizedId);
    } else {
        shouldSkipAsDuplicate = true;  // ← 普通模式：静默跳过
    }
}

if (shouldSkipAsDuplicate) {
    return;  // 无信号发射，UI 卡住
}
```

三个条件同时满足时触发：
- `m_fetchingComponents` 包含该条目
- `hasReusableData == false`（`lcscId` 为空，或 `hasCadData=false` 且无 symbol/footprint 数据）
- `m_parallelContext == nullptr`（普通模式）

### 2. UI 侧依赖信号结束验证

- `src/ui/viewmodels/ComponentListViewModel.cpp:809` — `handleCadDataReady` 设置 `validationPhase = "completed"`
- `src/ui/viewmodels/ComponentListViewModel.cpp:842` — `handleFetchError` 设置 `validationPhase = "failed"`

如果服务层两个信号都不发射，列表项永远停在 `"validating"`。

### 3. 异步回调复活已删除条目

`src/services/ComponentService.cpp:369` 和 `src/services/ComponentService.cpp:437`：

```cpp
FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
```

`operator[]` 在键不存在时自动创建条目。如果用户在异步加载期间移除了元器件（`cancelRequestForComponent` 已删除条目），回调会重新创建该条目。当 `handleCadDataReady` 因 `findItemData` 返回 null 而提前返回时，`m_fetchingComponents` 中留下孤立条目。

- 若孤立条目 `hasCadData = true`：后续添加可复用数据，不触发 bug
- 若孤立条目 `hasCadData = false`（缓存加载失败后 `initializeFetchingComponent` 重置）：后续添加触发 `shouldSkipAsDuplicate`

## 触发条件

陈旧条目形成需要：
1. 异步缓存加载/网络请求启动（创建占位条目）
2. 请求完成前条目被 `cancelRequestForComponent` 删除
3. 请求完成后回调通过 `operator[]` 重建条目
4. 重建的条目因加载失败而 `hasCadData = false`（`initializeFetchingComponent` 重置）

此后对该元器件的任何普通验证请求都会被静默吞掉，直到：
- 条目被其他路径清理（如 `cancelAllPendingRequests`）
- 或挂起的网络请求完成并更新条目

## 影响范围

- `addComponent` — 直接调用 `fetchComponentData`，受影响
- `addComponentsBatch` → `startValidationQueue` — 受影响
- `refreshComponentInfo` — 直接调用 `fetchComponentData`，受影响

## 修复建议

### 方案 A：`shouldSkipAsDuplicate` 路径清除过期条目（推荐）

普通模式下遇到不可复用的旧条目时，清除并重新发起请求：

```cpp
// src/services/ComponentService.cpp:189
} else {
    // 修复：清除过期条目，允许重新请求
    m_fetchingComponents.remove(normalizedId);
}
```

### 方案 B：异步回调中用 `find()` 代替 `operator[]`

防止回调在条目已被清理后自动重建：

```cpp
// src/services/ComponentService.cpp:436-443
// 修改前：
FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];

// 修改后：
auto it = m_fetchingComponents.find(normalizedId);
if (it == m_fetchingComponents.end()) {
    return;  // 条目已被清理，跳过更新
}
FetchingComponent& fetchingComponent = *it;
```

同样修改 `src/services/ComponentService.cpp:369`。

### 方案 C：组合修复（最稳妥）

同时应用 A 和 B：
1. 普通模式遇到不可复用旧条目时清除并重新请求
2. 异步回调中使用 `find()` 避免条目泄漏

## 测试覆盖

当前测试未覆盖此竞态场景。建议新增测试：

```cpp
// 1. 添加元器件，缓存命中，异步加载启动
// 2. 在异步加载完成前移除元器件（触发 cancelRequestForComponent）
// 3. 异步加载失败（模拟缓存损坏）
// 4. 再次添加同一元器件
// 5. 验证：应能正常完成验证，不应卡在 validating
```

## 相关文件

| 文件 | 行号 | 关联 |
|------|------|------|
| `src/services/ComponentService.cpp` | 173-214 | `fetchComponentDataInternal` — shouldSkipAsDuplicate 逻辑 |
| `src/services/ComponentService.cpp` | 369, 437 | `loadComponentDataFromCacheAsync` — operator[] 泄漏 |
| `src/services/ComponentService.cpp` | 1023-1039 | `cancelRequestForComponent` — 条目清理 |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 809-836 | `handleCadDataReady` — 依赖 cadDataReady 结束验证 |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 842-892 | `handleFetchError` — 依赖 fetchError 结束验证 |

## 状态

**已修复** — commit `d628d8e6` 实施方案 C（组合修复）：
- 引入 `requestActive` 标志跟踪请求状态
- 普通模式下遇到非活动陈旧条目时清除并重新发起请求
- 异步回调中使用 `find()` 替代 `operator[]` 防止条目重建
- 新增 3 个单元测试覆盖竞态场景
