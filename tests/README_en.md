# EasyKiConverter Test Suite

This directory contains all test code and configurations for the EasyKiConverter project. We use the **QtTest** framework for quality assurance during C++ development.

## Directory Structure

- `unit/`: Unit test projects. Independent verification for core logic (Core), models (Models), and services (Services).
  - `test_easyeda_api.cpp`: Verifies client logic for interacting with EasyEDA servers (supports Mock).
- `common/`: Test helper classes and common Mock objects.
  - `MockNetworkAdapter.hpp`: A mock adapter used to intercept network requests.
- `reports/`: (Git ignored) Storage for XML/HTML test reports generated during the build process.

## How to Run Tests

### 1. Enable Test Build
Enable the `EASYKICONVERTER_BUILD_TESTS` option when configuring with CMake:

```powershell
cmake -B build -S . -DEASYKICONVERTER_BUILD_TESTS=ON
```

### 2. Build and Execute
Run specific test targets directly using CMake:

```powershell
# Build and run a specific test only
cmake --build build --target test_easyeda_api
.\build\bin\test_easyeda_api.exe
```

Or use CTest to run all registered tests:

```powershell
cd build
ctest --output-on-failure
```

## Testing Standards

1. **Dependency Injection**: All classes involving IO (network, filesystem) should accept dependencies via interfaces (e.g., `INetworkAdapter`) to allow Mock injection during testing.
2. **Asynchronous Testing**: For tests involving signals and slots, always use `QSignalSpy::wait()` with appropriate timeouts. Synchronous polling loops are strictly prohibited.
3. **Isolation**: Each test case should reset Mock state in `init()` and perform necessary resource cleanup in `cleanup()`.

---
For more technical details, please refer to the [Developer Handbook: Testing Guide](../docs/developer/TESTING_GUIDE_en.md).
