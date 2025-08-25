# 📁 Project Structure

```
EasyKiConverter/
├── src/                               # C++ source code directory
│   ├── main.cpp                       # Command-line tool main entry
│   ├── gui_main.cpp                   # GUI application main entry
│   ├── easyeda/                       # EasyEDA API and data processing
│   │   ├── EasyedaApi.cpp            # EasyEDA API client
│   │   ├── EasyedaApi.h              # EasyEDA API header
│   │   ├── EasyedaImporter.cpp       # Data importer
│   │   └── EasyedaImporter.h         # Data importer header
│   ├── kicad/                        # KiCad export engine
│   │   ├── KicadSymbolExporter.cpp   # Symbol exporter
│   │   ├── KicadSymbolExporter.h     # Symbol exporter header
│   │   ├── export_kicad_footprint.cpp # Footprint exporter (to be implemented)
│   │   └── export_kicad_3d_model.cpp # 3D model exporter (to be implemented)
│   └── gui/                          # Qt GUI interface
│       ├── MainWindow.cpp            # Main window implementation
│       └── MainWindow.h              # Main window header
├── docs/                              # Documentation directory
│   ├── README.md                     # Documentation index
│   ├── project_structure.md          # Project structure detailed description
│   ├── development_guide.md          # Development guide
│   ├── contributing.md               # Contribution guide
│   ├── performance.md                # Performance optimization guide
│   ├── system_requirements.md        # System requirements
│   ├── project_structure_en.md       # Project structure (English version)
│   ├── development_guide_en.md       # Development guide (English version)
│   ├── contributing_en.md            # Contribution guide (English version)
│   ├── performance_en.md             # Performance optimization guide (English version)
│   └── system_requirements_en.md     # System requirements (English version)
├── test_data/                         # Test data
│   └── C2040.json                    # Sample component data
├── CMakeLists.txt                     # CMake build configuration
├── LICENSE                           # GPL-3.0 License
├── README.md                         # Chinese documentation
└── README_en.md                      # English documentation
```

## 📋 Core Modules Description

### 🎯 Command Line Tool
| File | Description |
|------|-------------|
| **main.cpp** | Command-line interface main entry, handles parameter parsing, validation, and coordinates the entire conversion process |
| **gui_main.cpp** | GUI application entry, initializes the Qt application |

### 🖼️ GUI Interface
| File | Description |
|------|-------------|
| **MainWindow.cpp/.h** | Qt main window implementation, provides graphical user interface and user interaction functionality |

### 📚 Documentation Directory
| File | Description |
|------|-------------|
| **README.md** | Documentation index, provides links and brief descriptions of all documents |
| **project_structure.md** | Detailed project structure and module descriptions |
| **development_guide.md** | Development environment setup and workflow guide |
| **contributing.md** | Project contribution process and guidelines |
| **performance.md** | Performance optimization guide |
| **system_requirements.md** | System requirements and supported component types |

### 🔧 Core Engine
| Module | Description |
|--------|-------------|
| **easyeda/** | EasyEDA API client and data processing module |
| **kicad/** | KiCad format export engine, supports symbols, footprints, and 3D models |

### 📦 Data Processing Flow
1. **API Fetching**: Get component data from EasyEDA/LCSC
2. **Data Parsing**: Parse symbol, footprint, and 3D model information
3. **Format Conversion**: Convert to KiCad compatible format
4. **File Generation**: Output .kicad_sym, .kicad_mod and other files

### 🛠️ Build System
The C++ version uses CMake as the build system, supporting cross-platform compilation:
- **CMakeLists.txt**: CMake configuration file in the project root directory
- **src/CMakeLists.txt**: CMake configuration file in the source code directory
- **src/easyeda/CMakeLists.txt**: CMake configuration file for the EasyEDA module
- **src/kicad/CMakeLists.txt**: CMake configuration file for the KiCad module
- **src/gui/CMakeLists.txt**: CMake configuration file for the GUI module

### 🎯 Executable Files
The following executable files will be generated after building:
- **easykiconverter**: Command-line version of the conversion tool
- **easykiconverter-gui**: Graphical interface version of the conversion tool