# Universal EDA Library Converter Plan

## Purpose

This document defines EasyKiConverter's long-term direction, scope, and boundaries for EDA library conversion. It is a project planning document; the formats listed here are not necessarily implemented yet.

## Long-term direction

EasyKiConverter aims to provide EDA library conversion through a common intermediate representation (IR):

```mermaid
flowchart LR
    Source[Supported source library] --> Importer[Importer]
    Importer --> IR[EasyKiConverter IR]
    IR --> Exporter[Exporter]
    Exporter --> Target[Supported target library]
```

Once a format has both an Importer and an Exporter, it can in principle interoperate with other integrated formats. Format adapters should handle format-specific syntax and semantics; dedicated pairwise converters should not be added.

The repository already contains the IR directory and foundational types such as `SymbolComponentIR`, `FootprintComponentIR`, and `Model3DIR`. See [ADR 012: Intermediate Representation Refactor](adr/012-intermediate-representation-refactor.md) and [Conversion Mapping](../developer/CONVERSION_MAPPING.md).

## Scope and phases

The first phase focuses on library data: symbols, footprints, component associations, 3D models, and common metadata. The intended flow is:

```mermaid
flowchart LR
    Select[Select library] --> Detect[Detect or select source format]
    Detect --> Import[Import to IR]
    Import --> Target[Select target format]
    Target --> Export[Export library]
```

After the library model is stable, a separate project-conversion phase may cover schematics, PCBs, nets, instances, wires, buses, hierarchy, stackups, tracks, vias, zones, and design rules. Project conversion has substantially more format-specific semantics and requires a separate model and compatibility review.

## Quality boundaries

The project can provide conversion paths where both source and target adapters are implemented. It should not promise 100% lossless conversion between every format because EDA data models differ. Multi-unit symbols, Alternate Body/De Morgan data, special pin types, custom pad stacks, regions, variants, embedded models, associations, properties, fonts, and text alignment may not have direct equivalents.

The conversion pipeline should preserve semantic information in IR where possible, check target capabilities, and explicitly report complete conversions, degradations, and unmapped data. Important information must not be silently discarded.

## Conversion reports

Conversion Report or Compatibility Report should record successful, skipped, and failed objects; degradations and their reasons; unmapped properties; missing or unconvertible 3D models; and warnings that identify the affected component, graphic, or field.

## Format-support boundary

The project should aim to support as many mainstream EDA library formats as practical, with documented scope and limitations—not every EDA product, version, or private native file.

Prefer native parsing for stable, readable formats. Where appropriate, support official ASCII, XML, or exchange formats instead. Any external tool dependency must document its version, platform requirements, and known limitations.

## Project definition

> If EasyKiConverter supports an EDA library as an input format and has an Exporter for the target format, the library can be converted through EasyKiConverter IR. Compatibility checks and conversion reports describe complete conversions, degradations, and unmapped data.
