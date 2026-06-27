# Bug Report: Stale `m_fetchingComponents` Entries Causing Validation Requests to Be Silently Dropped

## Problem Description

When `m_fetchingComponents` contains stale entries with "no reusable data and no active requests", subsequent normal validation requests for the same component are silently skipped by `shouldSkipAsDuplicate`, without emitting `cadDataReady` or `fetchError`, causing UI list items to remain stuck in `validationPhase = "validating"` state indefinitely.

## Root Cause Code

### 1. Silent Skip Path

`src/services/ComponentService.cpp:173-214` — `fetchComponentDataInternal`

```
if (m_fetchingComponents.contains(normalizedId)) {
    ...
    if (hasReusableData) {
        // Reuse data, emit cadDataReady ✓
    } else if (m_parallelContext != nullptr) {
        // Clear stale entry in parallel mode ✓
        m_fetchingComponents.remove(normalizedId);
    } else {
        shouldSkipAsDuplicate = true;  // ← Normal mode: silently skip
    }
}

if (shouldSkipAsDuplicate) {
    return;  // No signal emitted, UI stuck
}
```

The bug triggers when all three conditions are met:
- `m_fetchingComponents` contains the entry
- `hasReusableData == false` (`lcscId` is empty, or `hasCadData=false` with no symbol/footprint data)
- `m_parallelContext == nullptr` (normal mode)

### 2. UI Side Depends on Signals to End Validation

- `src/ui/viewmodels/ComponentListViewModel.cpp:809` — `handleCadDataReady` sets `validationPhase = "completed"`
- `src/ui/viewmodels/ComponentListViewModel.cpp:842` — `handleFetchError` sets `validationPhase = "failed"`

If neither signal is emitted from the service layer, the list item remains in `"validating"` state forever.

### 3. Async Callbacks Resurrect Deleted Entries

`src/services/ComponentService.cpp:369` and `src/services/ComponentService.cpp:437`:

```cpp
FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
```

`operator[]` automatically creates an entry when the key doesn't exist. If the user removes a component during async loading (`cancelRequestForComponent` has deleted the entry), the callback recreates it. When `handleCadDataReady` returns early because `findItemData` returns null, an orphaned entry is left in `m_fetchingComponents`.

- If the orphaned entry has `hasCadData = true`: subsequent additions can reuse data, no bug triggered
- If the orphaned entry has `hasCadData = false` (reset by `initializeFetchingComponent` after cache load failure): subsequent additions trigger `shouldSkipAsDuplicate`

## Trigger Conditions

Stale entry formation requires:
1. Async cache load/network request starts (creates placeholder entry)
2. Entry is deleted by `cancelRequestForComponent` before request completes
3. Async callback recreates entry via `operator[]`
4. Recreated entry has `hasCadData = false` due to load failure (`initializeFetchingComponent` reset)

After this, any normal validation request for this component is silently dropped until:
- The entry is cleared by another path (e.g., `cancelAllPendingRequests`)
- The pending network request completes and updates the entry

## Impact Scope

- `addComponent` — directly calls `fetchComponentData`, affected
- `addComponentsBatch` → `startValidationQueue` — affected
- `refreshComponentInfo` — directly calls `fetchComponentData`, affected

## Suggested Fixes

### Option A: Clear Stale Entries in `shouldSkipAsDuplicate` Path (Recommended)

When encountering an unusable stale entry in normal mode, clear it and re-initiate the request:

```cpp
// src/services/ComponentService.cpp:189
} else {
    // Fix: clear stale entry, allow re-request
    m_fetchingComponents.remove(normalizedId);
}
```

### Option B: Use `find()` Instead of `operator[]` in Async Callbacks

Prevent callbacks from auto-recreating entries after they've been cleaned up:

```cpp
// src/services/ComponentService.cpp:436-443
// Before:
FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];

// After:
auto it = m_fetchingComponents.find(normalizedId);
if (it == m_fetchingComponents.end()) {
    return;  // Entry has been cleaned up, skip update
}
FetchingComponent& fetchingComponent = *it;
```

Apply the same change to `src/services/ComponentService.cpp:369`.

### Option C: Combined Fix (Most Robust)

Apply both A and B:
1. Clear and re-request when encountering inactive stale entries in normal mode
2. Use `find()` in async callbacks to prevent entry leaks

## Test Coverage

Current tests don't cover this race condition. Suggested new test:

```cpp
// 1. Add component, cache hit, async load starts
// 2. Remove component before async load completes (triggers cancelRequestForComponent)
// 3. Async load fails (simulate corrupted cache)
// 4. Add the same component again
// 5. Verify: should complete validation normally, not stuck in validating
```

## Related Files

| File | Line | Association |
|------|------|-------------|
| `src/services/ComponentService.cpp` | 173-214 | `fetchComponentDataInternal` — shouldSkipAsDuplicate logic |
| `src/services/ComponentService.cpp` | 369, 437 | `loadComponentDataFromCacheAsync` — operator[] leak |
| `src/services/ComponentService.cpp` | 1023-1039 | `cancelRequestForComponent` — entry cleanup |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 809-836 | `handleCadDataReady` — depends on cadDataReady to end validation |
| `src/ui/viewmodels/ComponentListViewModel.cpp` | 842-892 | `handleFetchError` — depends on fetchError to end validation |

## Status

**Fixed** — commit `d628d8e6` implements Option C (combined fix):
- Introduced `requestActive` flag to track request state
- Clear and re-request when encountering inactive stale entries in normal mode
- Use `find()` instead of `operator[]` in async callbacks to prevent entry recreation
- Added 3 new unit tests covering the race condition scenario
