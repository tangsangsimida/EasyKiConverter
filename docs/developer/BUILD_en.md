# Build Guide

This document provides detailed instructions for building the EasyKiConverter project locally.

## Prerequisites

### Operating System

- Windows 10/11 (Recommended)
- macOS 10.15+
- Ubuntu 20.04+ or other Linux distributions

### Required Software

#### Qt Framework

- **Version**: Qt 6.8 or higher (Recommended Qt 6.10.1)
- **Required Modules**:
  - Qt Quick
  - Qt Network
  - Qt Core
  - Qt Gui
  - Qt Widgets
  - Qt Quick Controls 2

**Download**: https://www.qt.io/download

#### CMake

- **Version**: CMake 3.16 or higher
- **Download**: https://cmake.org/download/

#### Compiler

**Windows**:
- MinGW 13.10 (Recommended)
- Or MSVC 2019+

**macOS**:
- Xcode 12+ (includes Clang)

**Linux**:
- GCC 9+ or Clang 10+

#### zlib

- Usually included with Qt on Windows
- Install via package manager on Linux:
  ```bash
  sudo apt-get install zlib1g-dev
  ```

## Installation Instructions

### Windows

#### 1. Install Qt

1. Download Qt Online Installer from https://www.qt.io/download
2. Run the installer and select Qt 6.10.1
3. Select MinGW 13.10 compiler
4. Select required modules:
   - Qt Quick
   - Qt Network
   - Qt Core
   - Qt Gui
   - Qt Widgets
   - Qt Quick Controls 2

#### 2. Install CMake

1. Download CMake from https://cmake.org/download/
2. Run the installer
3. Add CMake to system PATH

#### 3. Install MinGW (if not included with Qt)

1. Download MinGW-w64 from https://www.mingw-w64.org/
2. Extract to a directory (e.g., C:\mingw64)
3. Add bin directory to system PATH

### macOS

#### 1. Install Qt

```bash
brew install qt@6
```

Or download Qt installer from https://www.qt.io/download

#### 2. Install CMake

```bash
brew install cmake
```

#### 3. Install Xcode Command Line Tools

```bash
xcode-select --install
```

### Linux (Ubuntu)

#### 1. Install Qt 6

```bash
sudo apt-get update
sudo apt-get install qt6-base-dev qt6-declarative-dev qt6-network-dev
```

#### 2. Install CMake

```bash
sudo apt-get install cmake
```

#### 3. Install Compiler

```bash
sudo apt-get install build-essential
```

#### 4. Install zlib

```bash
sudo apt-get install zlib1g-dev
```

## Building the Project

### Clone the Repository

```bash
git clone https://github.com/tangsangsimida/EasyKiConverter_QT.git
cd EasyKiConverter_QT
```

### Windows + MinGW

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# Build project (Debug version)
cmake --build . --config Debug

# Build project (Release version)
cmake --build . --config Release

# Run application
./bin/EasyKiConverter.exe
```

### macOS

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -DCMAKE_PREFIX_PATH="/usr/local/Qt-6.10.1"

# Build project (Debug version)
cmake --build . --config Debug

# Build project (Release version)
cmake --build . --config Release

# Run application
./bin/EasyKiConverter
```

### Linux

```bash
# Create build directory
mkdir build
cd build

# Configure project
cmake .. -DCMAKE_PREFIX_PATH="/opt/Qt/6.10.1/gcc_64"

# Build project (Debug version)
cmake --build . --config Debug

# Build project (Release version)
cmake --build . --config Release

# Run application
./bin/EasyKiConverter
```

## Build Options

### Enable Debug Export

Enable debug export for symbol and footprint data:

```bash
cmake .. -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
```

### Disable Debug Export

Disable debug export (default is enabled):

```bash
cmake .. -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=OFF
```

### Build Type

Specify build type:

```bash
# Debug build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build
cmake .. -DCMAKE_BUILD_TYPE=Release

# RelWithDebInfo build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

### Parallel Compilation

Speed up compilation on multi-core systems:

```bash
# Use all available cores
cmake --build . --parallel

# Use specific number of cores
cmake --build . --parallel 4
```

## Using Qt Creator

1. Open Qt Creator
2. Select "Open File or Project"
3. Select `CMakeLists.txt` in the project root
4. Configure build kit:
   - Select Qt version (Qt 6.10.1 or higher)
   - Select compiler (MinGW, Clang, or GCC)
5. Click "Configure Project"
6. Click "Build" button (or press Ctrl+B)
7. Click "Run" button (or press F5) to launch the application

## Build Output

After successful build, the executable files are located at:

- **Debug version**: `build/bin/EasyKiConverter.exe` (Windows) or `build/bin/EasyKiConverter` (macOS/Linux)
- **Release version**: `build/bin/EasyKiConverter.exe` (Windows) or `build/bin/EasyKiConverter` (macOS/Linux)

## Cleaning Build

```bash
cd build
cmake --build . --target clean
```

Or remove the entire build directory:

```bash
rm -rf build
```

## Common Issues

### Issue: Qt not found

**Error**: `Could not find Qt6`

**Solution**:
1. Verify Qt installation path
2. Set CMAKE_PREFIX_PATH correctly:
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH="/path/to/Qt/6.10.1/compiler"
   ```

### Issue: zlib not found

**Error**: `Could not find ZLIB`

**Solution**:
- Windows: Qt usually includes zlib, ensure Qt path is correct
- Linux: Install zlib development package:
  ```bash
  sudo apt-get install zlib1g-dev
  ```

### Issue: QML module not found

**Error**: `module "EasyKiconverter_Cpp_Version" is not installed`

**Solution**:
1. Ensure CMakeLists.txt is configured correctly
2. Clean and rebuild:
   ```bash
   rm -rf build
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

### Issue: Linker errors

**Error**: `undefined reference to...`

**Solution**:
1. Ensure all required Qt modules are linked in CMakeLists.txt
2. Clean and rebuild the project
3. Check compiler compatibility

### Issue: Application crashes on startup

**Solution**:
1. Run in Debug mode to get stack trace
2. Check Qt plugin paths
3. Verify all required DLLs are in PATH (Windows)

## Development Build

For development, it's recommended to build in Debug mode with debug export enabled:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
cmake --build . --config Debug
```

## Release Build

For release builds, use Release mode:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Testing

After building, you can run the test programs:

```bash
cd tests
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"
cmake --build build

# Run tests
./build/test_layer_mapping.exe
./build/test_uuid_extraction.exe
```

## Deployment

### Windows Deployment

Use `windeployqt` to package the application:

```bash
cd build/bin
windeployqt EasyKiConverter.exe
```

This will copy all required Qt DLLs and plugins to the application directory.

### macOS Deployment

Create an application bundle:

```bash
cd build
cmake --install . --prefix /path/to/install
```

### Linux Deployment

Create a Debian package or AppImage:

```bash
# Using cpack
cpack -G DEB
```

## Next Steps

After successful build, refer to:
- [Getting Started Guide](GETTING_STARTED.md) - Learn how to use the application
- [Features Documentation](FEATURES.md) - Understand available features
- [Architecture Documentation](ARCHITECTURE.md) - Learn about project architecture

## Support

If you encounter any issues during the build process, please:
1. Check this documentation for solutions
2. Search existing GitHub Issues
3. Create a new issue with detailed information:
   - Operating system and version
   - Qt version
   - CMake version
   - Compiler version
   - Complete error message
   - Steps to reproduce