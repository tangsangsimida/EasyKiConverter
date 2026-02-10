# Tools

This directory contains various development and build tools for the EasyKiConverter project.

## Structure

```
tools/
├── windows/          # Windows-specific tools
│   └── format_code.bat
├── linux/            # Linux-specific tools (not yet implemented)
└── macos/            # macOS-specific tools (not yet implemented)
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

## Adding New Tools

When adding new tools:
1. Create the appropriate subdirectory (`windows/`, `linux/`, or `macos/`)
2. Add the tool script or executable
3. Update this README with documentation
4. Consider adding usage examples and requirements