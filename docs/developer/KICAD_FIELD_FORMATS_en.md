# KiCad Field Format Analysis

This document analyzes the field formats of KiCad symbol libraries (.kicad_sym) and footprint libraries (.kicad_mod), serving as a guide for implementing export functionality.

## Symbol Library (.kicad_sym) Format

### File Structure

```lisp
(kicad_symbol_lib (version 20220914) (generator kicad_symbol_editor)
  (symbol "ComponentName" (in_bom yes) (on_board yes)
    ;; Standard properties
    (property "Reference" "U" (at 3.81 6.35 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Value" "ComponentName" (at 3.81 -6.35 0)
      (effects (font (size 1.27 1.27)))
    )
    (property "Footprint" "" (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "Datasheet" "https://..." (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )

    ;; Description and keywords (optional, for library management)
    (property "ki_keywords" "CMOS BUFFER 3State" (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )
    (property "ki_description" "8-bit D-flip-flop with 3-State outputs" (at 0 0 0)
      (effects (font (size 1.27 1.27)) hide)
    )

    ;; Symbol graphic definition (unit 0 usually for power pins)
    (symbol "ComponentName_0_0"
      ;; Power pins, graphics, etc.
    )

    ;; Functional unit definition
    (symbol "ComponentName_1_1"
      ;; Pin definitions
      (pin input line (at -12.7 2.54 0) (length 7.62)
        (name "PinName" (effects (font (size 1.27 1.27))))
        (number "PinNumber" (effects (font (size 1.27 1.27))))
      )
    )
  )
)
```

### Field Descriptions

| Field | Description | Example |
|-------|-------------|---------|
| `ki_description` | Component description | `"8-bit D-flip-flop with 3-State outputs"` |
| `ki_keywords` | Keywords (space-separated) | `"CMOS BUFFER 3State"` |
| `Reference` | Reference designator prefix | `"U"` (IC), `"R"` (resistor), `"C"` (capacitor) |
| `Value` | Component value/model | `"40374"` |
| `Footprint` | Default footprint | `""` (can be empty) |
| `Datasheet` | Datasheet link | `"https://..."` |

### Real Example

Example extracted from `4xxx_IEEE.kicad_sym`:

```lisp
(symbol "40374" (in_bom yes) (on_board yes)
  (property "Reference" "U" (at 7.62 8.89 0)
    (effects (font (size 1.27 1.27)))
  )
  (property "Value" "40374" (at 6.35 -15.24 0)
    (effects (font (size 1.27 1.27)))
  )
  (property "Footprint" "" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  (property "Datasheet" "https://www.digchip.com/datasheets/download_datasheet.php?id=369790&part-number=HEF40374BDB" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  (property "ki_keywords" "CMOS BUFFER 3State" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  (property "ki_description" "8-bit D-flip-flop with 3-State outputs" (at 0 0 0)
    (effects (font (size 1.27 1.27)) hide)
  )
  ;; ... symbol definition ...
)
```

---

## Footprint Library (.kicad_mod) Format

### File Structure

```lisp
(footprint "FootprintName" (version 20211014) (generator pcbnew)
  (layer "F.Cu")
  (tedit 5A030281)
  (descr "Footprint description text")
  (tags "tag1 tag2")
  (attr through_hole)  ;; or smd

  ;; Reference designator
  (fp_text reference "REF**" (at 3.8 -7.2) (layer "F.SilkS")
    (effects (font (size 1 1) (thickness 0.15)))
  )

  ;; Component value
  (fp_text value "FootprintName" (at 3.8 7.4) (layer "F.Fab")
    (effects (font (size 1 1) (thickness 0.15)))
  )

  ;; Graphic elements (silkscreen, soldermask, assembly layer, etc.)
  (fp_line (start x1 y1) (end x2 y2) (layer "F.SilkS") (width 0.12))
  (fp_circle (center x y) (end x y) (layer "F.SilkS") (width 0.12))

  ;; Pad definition
  (pad "1" thru_hole rect (at 0 0) (size 2 2) (drill 1) (layers *.Cu *.Mask))
  (pad "2" smd rect (at 4 0) (size 2 4.2) (layers "F.Cu" "F.Paste" "F.Mask"))

  ;; 3D model reference
  (model "${KICAD6_3DMODEL_DIR}/path/model.wrl"
    (offset (xyz 0 0 0))
    (scale (xyz 1 1 1))
    (rotate (xyz 0 0 0))
  )
)
```

### Field Descriptions

| Field | Description | Example |
|-------|-------------|---------|
| `descr` | Footprint description | `"Generic Buzzer, D12mm height 9.5mm with RM7.6mm"` |
| `tags` | Tags (space-separated) | `"buzzer piezo"` |
| `attr` | Footprint attribute | `through_hole` / `smd` |

### Real Examples

#### Through-Hole Footprint Example

```lisp
(footprint "Buzzer_12x9.5RM7.6" (version 20211014) (generator pcbnew)
  (layer "F.Cu")
  (tedit 5A030281)
  (descr "Generic Buzzer, D12mm height 9.5mm with RM7.6mm")
  (tags "buzzer")
  (attr through_hole)
  ;; ...
  (pad "1" thru_hole rect (at 0 0) (size 2 2) (drill 1) (layers *.Cu *.Mask))
  (pad "2" thru_hole circle (at 7.6 0) (size 2 2) (drill 1) (layers *.Cu *.Mask))
)
```

#### SMD Footprint Example

```lisp
(footprint "Buzzer_CUI_CPT-9019S-SMT" (version 20211014) (generator pcbnew)
  (layer "F.Cu")
  (tedit 5C12D246)
  (descr "https://www.cui.com/product/resource/cpt-9019s-smt.pdf")
  (tags "buzzer piezo")
  (attr smd)
  ;; ...
  (pad "1" smd rect (at -4 0) (size 2 4.2) (layers "F.Cu" "F.Paste" "F.Mask"))
  (pad "2" smd rect (at 4 0) (size 2 4.2) (layers "F.Cu" "F.Paste" "F.Mask"))
)
```

---

## Current Code Export Status (based on v3.1.7)

### Symbol Library Export (ExporterSymbol.cpp)

Properties actually exported by current code:

| Property | Data Source | Description |
|----------|-------------|-------------|
| `Reference` | `symbolData.info().prefix` | Reference designator prefix (e.g., "U", "R", "C") |
| `Value` | `symbolData.info().name` | Component name (e.g., "4001", "RESISTOR") |
| `Footprint` | `symbolData.info().package` | Footprint name (format: `libName:packageName`) |
| `Datasheet` | `symbolData.info().datasheet` | Datasheet link |
| `Manufacturer` | `symbolData.info().manufacturer` | Manufacturer |
| `LCSC Part` | `symbolData.info().lcscId` | LCSC part number (e.g., "C12345") |

**Fields currently not exported (pending implementation):**
- `ki_description` - Symbol description
- `ki_keywords` - Keywords

### Footprint Library Export (ExporterFootprint.cpp)

Current code **does not export** description and tags fields:

| Field | Status | Description |
|-------|--------|-------------|
| `descr` | ❌ Not implemented | Footprint description |
| `tags` | ❌ Not implemented | Tags |
| `attr` | ✅ Implemented | Automatically determined from pad type `through_hole` or `smd` |

---

## Data Structure Field Analysis

### SymbolInfo Structure Fields

```cpp
struct SymbolInfo {
    QString name;           // Component name → Value
    QString prefix;         // Prefix → Reference
    QString package;        // Package → Footprint
    QString manufacturer;   // Manufacturer → Manufacturer
    QString description;    // Description → ki_description (pending)
    QString datasheet;      // Datasheet → Datasheet
    QString lcscId;         // LCSC part number → LCSC Part
    QString jlcId;          // JLC part number
    // ... other fields
};
```

### FootprintInfo Structure Fields

```cpp
struct FootprintInfo {
    QString name;           // Footprint name
    QString type;            // Type
    QString model3DName;     // 3D model name
    // Note: no description and keywords fields
    // ... other fields
};
```

---

## Data Mapping Table (Corrected)

### Symbol Library Mapping

| Data Source | KiCad Property | Code Location | Status |
|------------|----------------|---------------|--------|
| `symbolData.info().prefix` | `Reference` | ExporterSymbol.cpp:510 | ✅ Implemented |
| `symbolData.info().name` | `Value` | ExporterSymbol.cpp:524 | ✅ Implemented |
| `symbolData.info().package` | `Footprint` | ExporterSymbol.cpp:537 | ✅ Implemented |
| `symbolData.info().datasheet` | `Datasheet` | ExporterSymbol.cpp:554 | ✅ Implemented |
| `symbolData.info().manufacturer` | `Manufacturer` | ExporterSymbol.cpp:569 | ✅ Implemented |
| `symbolData.info().lcscId` | `LCSC Part` | ExporterSymbol.cpp:584 | ✅ Implemented |
| `symbolData.info().description` | `ki_description` | - | ❌ Pending |
| No data source | `ki_keywords` | - | ❌ Pending (needs field addition) |

### Footprint Library Mapping

| Data Source | KiCad Keyword | Code Location | Status |
|------------|--------------|---------------|--------|
| No data source | `descr` | - | ❌ Pending (FootprintInfo has no description field) |
| No data source | `tags` | - | ❌ Pending (FootprintInfo has no keywords field) |
| Pad type detection | `attr` | ExporterFootprint.cpp:186 | ✅ Implemented |

---

## Pending Features

### 1. Add ki_description to Symbol Library

Add in `ExporterSymbol.cpp`:

```cpp
// ki_description property (from SymbolInfo.description)
if (!symbolData.info().description.isEmpty()) {
    fieldOffset += 2.54;
    content += QString("    (property\n");
    content += QString("      \"ki_description\"\n");
    content += QString("      \"%1\"\n").arg(escapePropertyValue(symbolData.info().description));
    content += "      (id 6)\n";
    content += QString("      (at %1 %2 0)\n").arg(graphCenterOffsetX, 0, 'f', 2).arg(yLow - fieldOffset, 0, 'f', 2);
    content += QString("      (effects (font (size %1 %2) (thickness 0) ) hide)\n")
                   .arg(fontSize, 0, 'f', 2)
                   .arg(fontSize, 0, 'f', 2);
    content += "    )\n";
}
```

**Insert Location**: After LCSC Part property (~line 593)

### 2. Add descr to Footprint Library

First add `description` field to `FootprintInfo` struct:

```cpp
// FootprintData.h - add
struct FootprintInfo {
    // ... existing fields
    QString description;    // Footprint description
    QString keywords;       // Keywords
};
```

Then add export logic in `ExporterFootprint.cpp`.

### 3. Add ki_keywords Field

Need to add `keywords` field to `SymbolInfo` struct (if EasyEDA API provides it).

---

## Notes

1. **Escape Characters**: Quotes in description text need to be escaped as `\"`

2. **Empty Value Handling**: Current code uses `isEmpty()` check, only exports when value exists

3. **ID Numbers**: Property `id` must increment sequentially (0-5 used, new properties start from 6)

4. **Encoding**: All text uses UTF-8 encoding

5. **Coordinates**: Property coordinates `(at 0 0 0)` indicate default position, `hide` means hidden in schematic

---

## Reference Files

- Symbol library example: `/home/dennis/Desktop/4xxx_IEEE.kicad_sym`
- Footprint library example: `/home/dennis/Desktop/Buzzer_Beeper.pretty/`
- Symbol exporter: `src/core/kicad/ExporterSymbol.cpp`
- Footprint exporter: `src/core/kicad/ExporterFootprint.cpp`
- Symbol data model: `src/models/SymbolData.h`
- Footprint data model: `src/models/FootprintData.h`

---

*Last updated: 2026-05-02*