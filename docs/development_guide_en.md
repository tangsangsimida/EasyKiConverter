# 🛠️ Development Guide

## 🌟 Development Environment Setup

### 🛠️ Toolchain
- **Compiler**: GCC (Linux/macOS), MSVC (Windows)
- **Build System**: CMake >= 3.10
- **Dependency Management**: vcpkg or system package manager
- **Qt Version**: Qt5 (recommended to use system-installed Qt libraries)

### 📦 Required Dependencies
```bash
# Linux/macOS
sudo apt-get install build-essential cmake qt5-default

# Windows
# Use vcpkg to install dependencies
./vcpkg install qt5
```

## 🚀 Development Workflow

### 1. Clone the Repository
```bash
git clone <repository-url>
cd EasyKiConverter
```

### 2. Create Build Directory
```bash
mkdir build
cd build
```

### 3. Configure CMake
```bash
cmake ..
```

### 4. Build the Project
```bash
make
```

### 5. Run Test Programs (Optional)
```bash
# Run test programs in the build directory
ctest
```

## 🖥️ IDE Setup

- **Visual Studio Code**:
  - Install C/C++ extension
  - Configure CMake Tools extension
  - Configure launch.json for debugging support

- **CLion**:
  - Import project by selecting CMakeLists.txt
  - Automatically configure build environment

## 🧪 Debugging Tips

- Use GDB or LLDB for debugging
- Configure VSCode's launch.json file to support breakpoint debugging
- Use CMake's Debug mode to build the project

## 📂 Directory Structure
Refer to [Project Structure](project_structure_en.md) document

## 🌐 Web UI Development

```bash
# Start development server
cd EasyKiConverter/Web_Ui
python app.py

# Access development interface
# http://localhost:8000
```

**Frontend Development:**
- Modify `index.html` - Page structure
- Modify `css/styles.css` - Styles and animations
- Modify `js/script.js` - Interaction logic

**Backend Development:**
- Modify `app.py` - API interfaces and routing
- Core conversion logic in `../` directory

## 🛠️ Command Line Development

```bash
# Run basic conversion test
cd EasyKiConverter
python main.py --lcsc_id C13377 --symbol --debug

# Test different component types
python main.py --lcsc_id C25804 --footprint --debug  # Test footprints
python main.py --lcsc_id C13377 --model3d --debug    # Test 3D models
```

## 🔧 Code Structure

- **easyeda/** - EasyEDA API and data processing
- **kicad/** - KiCad format export engines
- **Web_Ui/** - Flask web application
- **main.py** - Command-line entry point
- **helpers.py** - Shared utility functions

## 🔧 Command Line Options

```bash
python main.py [options]

Required parameters:
  --lcsc_id TEXT         LCSC part number to convert (e.g., C13377)

Export options (at least one required):
  --symbol               Export symbols (.kicad_sym)
  --footprint            Export footprints (.kicad_mod)
  --model3d              Export 3D models

Optional parameters:
  --output_dir PATH      Output directory path [default: ./output]
  --lib_name TEXT        Library file name [default: EasyKiConverter]
  --kicad_version INT    KiCad version (5 or 6) [default: 6]
  --overwrite            Overwrite existing files
  --debug                Enable detailed logging
  --help                 Show help information
```

### 📝 Usage Examples

```bash
# Export all content to default directory
python main.py --lcsc_id C13377 --symbol --footprint --model3d

# Export symbols only to specified directory
python main.py --lcsc_id C13377 --symbol --output_dir ./my_symbols

# Export to custom library name
python main.py --lcsc_id C13377 --symbol --footprint --lib_name MyComponents

# Enable debug mode
python main.py --lcsc_id C13377 --symbol --debug
```