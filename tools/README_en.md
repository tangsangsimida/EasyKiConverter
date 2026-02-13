# Tools

This directory contains various development and build tools for the EasyKiConverter project.

## Structure

```
tools/
├── windows/          # Windows-specific tools
│   └── format_code.bat
├── linux/            # Linux-specific tools (not yet implemented)
├── macos/            # macOS-specific tools (not yet implemented)
└── python/           # Platform-independent Python tools
    └── manage_version.py
```

## Windows Tools

### format_code.bat

Automatically formats C++ and QML source code using clang-format.

**Usage:**
```bash
cd tools/windows
format_code.bat
```

**Requirements:**
- clang-format must be installed and available in PATH
- `.clang-format` configuration file should be in the project root

**What it does:**
- Formats all `.cpp`, `.h`, and `.qml` files in the project
- Uses the `.clang-format` configuration file in the project root
- Ensures consistent code style across the codebase

## Python Tools

### manage_version.py

Automates project version synchronization across `vcpkg.json`, `CMakeLists.txt`, and `src/main.cpp`.

**Usage:**
```bash
# Check current version
python tools/python/manage_version.py --check

# Update version to 3.0.7
python tools/python/manage_version.py 3.0.7
```

**Requirements:**
- Python 3.x
- Recommended to run from the project root.

**Updated Files:**
- `vcpkg.json`: Updates the `"version-string"` field.
- `CMakeLists.txt`: Updates the default `VERSION_FROM_CI` and `qt_add_qml_module` version.
- `src/main.cpp`: Updates `app.setApplicationVersion`.

## Adding New Tools

When adding new tools:
1. Create the appropriate subdirectory (`windows/`, `linux/`, `macos/`, or `python/`)
2. Add the tool script or executable
3. Update this README with documentation
4. Consider adding usage examples and requirements
