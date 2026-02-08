# Debug Mode Configuration

## Overview

Debug mode allows you to export detailed debugging information during the export process, including:
- Raw component data (component info, CAD data, 3D model data)
- Symbol and footprint data
- Debug logs for each stage (fetch, process, write)
- Performance timing information
- Error details and stack traces

## Configuration Methods

### Method 1: Environment Variable (Recommended)

Debug mode is controlled by the `EASYKICONVERTER_DEBUG_MODE` environment variable.

#### Windows (PowerShell)

**Temporary (Current Session Only):**
```powershell
$env:EASYKICONVERTER_DEBUG_MODE="true"
```

**Permanent (System-Wide):**
```powershell
# Set for current user
[System.Environment]::SetEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "true", "User")

# Set for all users (requires administrator privileges)
[System.Environment]::SetEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "true", "Machine")

# Set for current process only
$env:EASYKICONVERTER_DEBUG_MODE="true"
```

**Windows (CMD):**
```cmd
REM Temporary
set EASYKICONVERTER_DEBUG_MODE=true

REM Permanent (requires restart)
setx EASYKICONVERTER_DEBUG_MODE "true"
```

#### Linux/macOS

**Temporary (Current Session Only):**
```bash
export EASYKICONVERTER_DEBUG_MODE=true
```

**Permanent (Add to ~/.bashrc or ~/.zshrc):**
```bash
echo 'export EASYKICONVERTER_DEBUG_MODE=true' >> ~/.bashrc
source ~/.bashrc
```

#### Accepted Values

- `true`, `1`, `yes` - Enable debug mode
- `false`, `0`, `no`, or not set - Disable debug mode

### Method 2: CMake Build Option

When building from source, you can enable debug mode symbol/footprint export:

```bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" -DENABLE_SYMBOL_FOOTPRINT_DEBUG_EXPORT=ON
```

This compiles the code with additional debug export capabilities.

### Method 3: Configuration File (Fallback)

If the environment variable is not set, debug mode can be controlled via the configuration file:
- **Windows**: `%APPDATA%\EasyKiConverter\EasyKiConverter\config.json`
- **Linux**: `~/.config/EasyKiConverter/EasyKiConverter/config.json`
- **macOS**: `~/Library/Application Support/EasyKiConverter/EasyKiConverter/config.json`

```json
{
  "debugMode": true
}
```

## Priority Order

Debug mode is enabled in the following priority order:

1. **Environment Variable** (Highest Priority)
   - `EASYKICONVERTER_DEBUG_MODE` environment variable

2. **Configuration File**
   - `debugMode` setting in config.json

3. **Default**
   - `false` (disabled)

## Debug Output Location

When debug mode is enabled, debug information is exported to:

```
<Output Directory>/debug/<ComponentID>/
├── cinfo_raw.json           # Raw component info
├── cad_raw.json             # Raw CAD data
├── adv_raw.json             # Advanced data
├── model3d_raw.obj          # Raw 3D model OBJ data
├── model3d_raw.step         # Raw 3D model STEP data
└── debug_info.json          # Debug summary and logs
```

## Debug Information Included

- **Component Information**: LCSC component ID, name, package, datasheet
- **CAD Data**: Pin details, shape definitions, layer mappings
- **3D Model Data**: OBJ/STEP model files and metadata
- **Timing Information**: Duration for each stage (fetch, process, write)
- **Error Logs**: Detailed error messages and stack traces
- **Validation Status**: Success/failure status for each stage

## Security Considerations

Debug mode may expose sensitive information:
- Component details and specifications
- Internal file structures
- Performance metrics

**Important:** Do not enable debug mode in production environments or when sharing debug output files.

## Troubleshooting

### Debug mode not working?

1. **Check environment variable is set:**
   ```bash
   echo $EASYKICONVERTER_DEBUG_MODE  # Linux/macOS
   echo %EASYKICONVERTER_DEBUG_MODE%  # Windows CMD
   ```

2. **Restart the application** after setting the environment variable

3. **Check application logs** for debug mode initialization messages

### Debug files not being created?

1. Ensure the output directory has write permissions
2. Check that the component export was successful
3. Verify debug mode is actually enabled (check logs)

## Best Practices

- **Development**: Enable debug mode during development and testing
- **Production**: Always disable debug mode in production
- **Debugging**: Enable debug mode when troubleshooting specific issues
- **Performance**: Disable debug mode for performance-critical operations
- **Security**: Never commit debug output files to version control

## Examples

### Enable Debug Mode for Testing (Windows PowerShell)

```powershell
# Set environment variable
$env:EASYKICONVERTER_DEBUG_MODE="true"

# Run application
.\build\bin\EasyKiConverter.exe
```

### Enable Debug Mode for Development (Linux)

```bash
# Add to ~/.bashrc
echo 'export EASYKICONVERTER_DEBUG_MODE=true' >> ~/.bashrc

# Reload shell
source ~/.bashrc

# Run application
./build/bin/EasyKiConverter
```

### Disable Debug Mode

```bash
# Unset environment variable (Linux/macOS)
unset EASYKICONVERTER_DEBUG_MODE

# Or set to false (Windows CMD)
set EASYKICONVERTER_DEBUG_MODE=false
```

## Related Documentation

- [BUILD_en.md](docs/developer/BUILD_en.md) - Build instructions with debug options
- [FAQ.md](docs/user/FAQ.md) - Common questions about debug mode
- [FEATURES.md](docs/user/FEATURES.md) - Debug mode feature details