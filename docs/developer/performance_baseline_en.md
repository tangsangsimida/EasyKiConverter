# EasyKiConverter Performance Baseline

This document records the core performance benchmarks for each version of the project. CI performs regression validation against this baseline.

## Core Conversion Performance Metrics

| Test Item | Baseline (ms) | Allowable Deviation (%) | Description |
| :--- | :--- | :--- | :--- |
| Single LCSC footprint parsing (simple component) | 50 | 15 | e.g., 0603 resistor |
| Single LCSC footprint parsing (complex IC) | 150 | 20 | e.g., LQFP-100 package |
| BOM full conversion (50 rows or less) | 1000 | 15 | Includes network fetching and local generation |
| KiCad library file serialization (100+ components) | 300 | 10 | Disk I/O performance |

## Validation Environment Reference

- **OS**: Windows 10/11
- **Qt Version**: 6.10.1
- **Compiler**: MSVC 2019+ / MinGW 11+
- **CPU**: Intel i5-11th Gen / AMD Ryzen 5 5000+ or equivalent CI Runner

## Update History

- **v0.1**: Initial baseline established.
