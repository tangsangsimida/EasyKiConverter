# Testing Guide

This document describes the testing architecture, Mocking strategies, and how to write new test cases for EasyKiConverter.

## 1. Architecture Overview

We follow a tiered testing strategy:
- **Unit Tests**: Isolated verification for the smallest testable units (functions, classes).
- **Integration Tests**: Verifying collaboration between multiple modules (e.g., the full chain from network fetching to KiCad file generation).
- **QML/UI Tests**: Verifying view logic using `QtQuickTest`.

## 2. Dependency Injection & Mocking Strategy

For testability, core components (like `EasyedaApi`) should not use concrete network clients directly. Instead, they interact through interfaces.

### INetworkClient Interface
All network operations are defined in the `INetworkClient` interface, supporting Mock implementations.

```cpp
// Example: Injecting a Mock client
EasyedaApi api(new MockNetworkClient());
```

### Using MockNetworkClient
In tests, you can preset API results via `MockNetworkClient`:

```cpp
QJsonObject mockData;
mockData.insert("success", true);
m_mockClient->addJsonResponse("https://api.url", mockData);
```

## 3. Key Points for Asynchronous Testing

### Proper use of QSignalSpy
Always use the `wait()` method for asynchronous signal capture:

```cpp
QSignalSpy spy(api, &EasyedaApi::componentInfoFetched);
api->fetchComponentInfo("C123");

// Must use wait to allow event loop processing
QVERIFY2(spy.wait(2000), "Signal not triggered within 2s");
QCOMPARE(spy.count(), 1);
```

## 4. CMake Test Configuration

Unit tests are defined in `tests/unit/CMakeLists.txt`. When adding new tests, ensure:
1. Necessary `src` levels are included via `target_include_directories`.
2. Corresponding Mock headers are added to `add_executable` for proper `AUTOMOC` symbol generation.

## 5. Continuous Integration (CI)

GitHub Actions automatically runs all builds with testing enabled. If unit tests fail, the CI process will fail and prevent merging.

---
If you have questions, please consult the core development team.
