# BOM File Parsing Guide

## Overview

EasyKiConverter provides comprehensive BOM (Bill of Materials) file parsing functionality, allowing users to import multiple LCSC component IDs at once for batch conversion.

## Supported Formats

- **CSV** - Comma-Separated Values format
- **TXT** - Plain text format
- **XLSX** - Excel 2007+ format
- **XLS** - Excel 97-2003 format

## Component ID Format

The system automatically identifies LCSC component IDs with the following format:
- Starts with the letter 'C'
- Followed by at least 4 digits
- Example: `C12345`, `C2040`, `C2858491`

## Usage

### Importing BOM Files

1. Click the "Import BOM" button in the UI
2. Select your BOM file (CSV, TXT, or Excel)
3. The system will automatically:
   - Parse the file content
   - Extract all valid LCSC component IDs
   - Remove duplicates
   - Add them to the component list

### Sample BOM File Format

**CSV Example:**
```csv
元器件编号,描述,数量
C8734,电容 100nF,10
C52717,电阻 1kΩ,5
C8323,电感 10uH,2
```

**Text Example:**
```
C8734 Capacitor 100nF
C52717 Resistor 1kΩ
C8323 Inductor 10uH
```

**Excel Example:**
| Component ID | Description | Quantity |
|--------------|-------------|----------|
| C8734 | Capacitor 100nF | 10 |
| C52717 | Resistor 1kΩ | 5 |
| C8323 | Inductor 10uH | 2 |

## Features

### Automatic Extraction
- Scans all cells in the file
- Extracts component IDs matching the LCSC format
- No need for specific column names or positions

### Duplicate Removal
- Automatically removes duplicate component IDs
- Prevents adding the same component multiple times

### Import Statistics
- Displays the number of components added
- Shows the number of duplicate components skipped

### Excel Format Support
The system supports two Excel parsing methods:
1. **QXlsx Library** - Native C++ Excel parsing (if available)
2. **Python Pandas** - Fallback using Python (if QXlsx is not available)

## API Reference

### ComponentService::parseBomFile()

```cpp
QStringList ComponentService::parseBomFile(const QString& filePath);
```

**Parameters:**
- `filePath` - Path to the BOM file

**Returns:**
- `QStringList` - List of extracted component IDs

**Supported File Extensions:**
- `.csv` - CSV format
- `.txt` - Plain text format
- `.xlsx` - Excel 2007+ format
- `.xls` - Excel 97-2003 format

### ComponentService::validateComponentId()

```cpp
bool ComponentService::validateComponentId(const QString& componentId) const;
```

**Parameters:**
- `componentId` - Component ID to validate

**Returns:**
- `bool` - `true` if valid, `false` otherwise

**Validation Rules:**
- Must start with 'C'
- Must be followed by at least 4 digits
- Cannot contain other characters

### ComponentService::extractComponentIdFromText()

```cpp
QStringList ComponentService::extractComponentIdFromText(const QString& text) const;
```

**Parameters:**
- `text` - Text to search for component IDs

**Returns:**
- `QStringList` - List of extracted component IDs

## Implementation Details

### CSV Parsing
- Uses Qt's `QTextStream` for reading
- Handles different delimiters (comma, semicolon, tab)
- Supports UTF-8 encoding

### Excel Parsing
- **Primary**: QXlsx library (C++ native)
- **Fallback**: Python pandas library
- Automatically converts Excel to CSV if needed
- Uses temporary files for conversion

### Component ID Extraction
- Uses regular expression: `C\d{4,}`
- Case-sensitive matching
- Scans all content in the file

## Troubleshooting

### No Component IDs Found
**Possible Causes:**
- File format is not supported
- Component IDs don't match LCSC format
- File is corrupted or empty

**Solutions:**
- Verify file format (CSV, TXT, XLSX, XLS)
- Check component ID format (C + at least 4 digits)
- Ensure file is not empty

### Excel File Not Parsing
**Possible Causes:**
- QXlsx library not available
- Python/pandas not installed
- File is corrupted

**Solutions:**
- Install QXlsx library
- Install Python and pandas library
- Try converting Excel to CSV manually

### Duplicates Not Removed
**Possible Causes:**
- Component IDs have different formatting
- Whitespace differences

**Solutions:**
- Check for leading/trailing spaces
- Ensure consistent formatting
- The system automatically trims whitespace

## Examples

### Example 1: Simple CSV Import

**File: `my_bom.csv`**
```csv
Component,Description
C12345,Resistor 10k
C2040,Capacitor 100nF
```

**Result:**
- Components added: 2
- Components skipped: 0

### Example 2: Mixed Content with Duplicates

**File: `mixed_bom.csv`**
```csv
ID,Name,Notes
C12345,Resistor,Main circuit
C2040,Capacitor,Power supply
C12345,Resistor,Duplicate entry
C8734,Inductor,Filter
```

**Result:**
- Components added: 3 (C12345, C2040, C8734)
- Components skipped: 1 (C12345 duplicate)

### Example 3: Excel File

**File: `bom.xlsx`**

| Component ID | Description | Package |
|--------------|-------------|---------|
| C8734 | Capacitor | 0805 |
| C52717 | Resistor | 0603 |
| C8323 | Inductor | 1210 |

**Result:**
- Components added: 3
- Components skipped: 0

## Future Enhancements

Potential improvements for BOM parsing:
- Support for more Excel features (formulas, charts)
- Custom column mapping
- Batch file processing
- Import progress tracking
- Error reporting with line numbers

## Related Documentation

- [User Guide](../user/USER_GUIDE_en.md)
- [API Documentation](ARCHITECTURE_en.md)
- [Component Service](ARCHITECTURE_en.md#component-service)

## Version History

- **v3.0.1** (2026-01-27) - Initial BOM parsing implementation
  - CSV and TXT support
  - Excel support (QXlsx and Python pandas)
  - Automatic component ID extraction
  - Duplicate removal
  - Import statistics