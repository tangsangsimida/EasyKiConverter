# EasyKiConverter Internationalization Implementation Summary

## Overview

This document records the complete implementation of the EasyKiConverter application internationalization (i18n) feature. The implementation includes bilingual support for Chinese (Simplified, zh_CN) and English (en), along with automatic language detection and manual language switching.

## Implementation Date

February 3, 2026

## Tech Stack

- Qt 6.10.2
- C++17
- QML (Qt Quick)
- CMake 3.16+

## Core Components

### 1. LanguageManager Class

**File Location:**
- `src/core/LanguageManager.h`
- `src/core/LanguageManager.cpp`

**Functionality:**
- Singleton pattern implementation
- Manual language switching (Chinese/English)
- Language settings persistence (via ConfigService)
- QTranslator management and installation
- Support for translation file hot-reload (via `refreshRequired` signal)

**Key Interface:**
```cpp
// Get singleton instance
static LanguageManager* instance();

// Set language
Q_INVOKABLE void setLanguage(const QString& languageCode);

// Get current language
Q_INVOKABLE QString currentLanguage() const;

// Detect system language (disabled, returns English directly)
Q_INVOKABLE QString detectSystemLanguage() const;

// Signal
void languageChanged(const QString& language);

// Request UI refresh signal (notify QML to refresh after translation file update)
void refreshRequired();
```

**Supported Language Codes:**
- `zh_CN` - Simplified Chinese
- `en` - English
- `system` - Follow system (default)

### 2. Translation Files

**File Location:**
- `resources/translations/translations_easykiconverter_en.ts` (English)
- `resources/translations/translations_easykiconverter_zh_CN.ts` (Chinese)

**Translation Contexts:**
- `MainWindow` - Main window UI text
- `ResultListItem` - Result list item component
- `LanguageManager` - Language manager text

**Translation Entry Statistics:**
- Total ~50+ translation entries
- Covers all UI text, buttons, labels, and prompt messages

### 3. QML Internationalization

**Modified Files:**
- `src/ui/qml/MainWindow.qml` (Main window)
- `src/ui/qml/components/ResultListItem.qml` (Result list item)

**Internationalization Method:**
```qml
// Simple text
text: qsTr("开始转换")

// Text with parameters
text: qsTr("共 %1 个元器件").arg(componentCount)

// Conditional text
text: {
    if (isExporting) return qsTr("正在转换...");
    return qsTr("开始转换");
}
```

**Components Not Requiring Internationalization:**
- `StepItem.qml` - Labels passed via properties
- `StatItem.qml` - Labels passed via properties
- `ComponentListItem.qml` - No static text
- `Card.qml` - Title passed via properties
- `ModernButton.qml` - Text passed via properties
- `Icon.qml` - No text

### 4. CMake Configuration

**Modified Files:**
- `CMakeLists.txt` (root)
- `src/core/CMakeLists.txt` (core module)

**Key Changes:**

1. **CMakeLists.txt** - Add translation resources:
```cmake
set(TRANSLATION_FILES
    resources/translations/translations_easykiconverter_en.ts
    resources/translations/translations_easykiconverter_zh_CN.ts
)

qt_add_resources(appEasyKiconverter_Cpp_Version "translations"
    PREFIX "/"
    FILES
        ${TRANSLATION_FILES}
)
```

2. **src/core/CMakeLists.txt** - Add LanguageManager source files:
```cmake
target_sources(EasyKiConverterCore
    PRIVATE
        LanguageManager.cpp
        LanguageManager.h
)
```

### 5. main.cpp Integration

**Modified Files:**
- `main.cpp`

**Key Changes:**

1. **Include Header:**
```cpp
#include "src/core/LanguageManager.h"
```

2. **Initialize LanguageManager:**
```cpp
// Initialize language manager (before creating engine)
EasyKiConverter::LanguageManager::instance();
```

3. **Register as QML Singleton:**
```cpp
// Register LanguageManager to QML
qmlRegisterSingletonType<QObject>(
    "EasyKiconverter_Cpp_Version",
    1,
    0,
    "LanguageManager",
    [](QQmlEngine*, QJSEngine*) -> QObject* {
        return EasyKiConverter::LanguageManager::instance();
    });
```

## Translation Entry List

### MainWindow Context

| Chinese Original | English Translation | Context |
|-----------------|---------------------|---------|
| 选择 BOM 文件 | Select BOM File | Dialog title |
| 将嘉立创EDA元器件转换为KiCad格式 | Convert LCSC EDA Components to KiCad Format | Welcome title |
| 添加元器件 | Add Component | Card title |
| 输入LCSC元件编号 (例如: C2040) | Enter LCSC Component ID (e.g., C2040) | Placeholder |
| 添加 | Add | Button |
| 粘贴 | Paste | Button |
| 导入BOM文件 | Import BOM File | Card title |
| 选择BOM文件 | Select BOM File | Button |
| 未选择文件 | No File Selected | File status |
| 元器件列表 | Component List | Card title |
| 共 %1 个元器件 | Total %1 Components | Statistics text |
| 搜索元器件... | Search Components... | Placeholder |
| 清空列表 | Clear List | Button |
| 导出设置 | Export Settings | Card title |
| 输出路径 | Output Path | Label |
| 选择输出目录 | Select Output Directory | Placeholder |
| 浏览 | Browse | Button |
| 库名称 | Library Name | Label |
| 输入库 (例如: MyLibrary) | Enter Library Name (e.g., MyLibrary) | Placeholder |
| 符号库 | Symbol Library | Checkbox |
| 封装库 | Footprint Library | Checkbox |
| 3D模型 | 3D Model | Checkbox |
| 调试模式 | Debug Mode | Checkbox |
| 导出模式 | Export Mode | Label |
| 追加 | Append | Radio button |
| 保留已存在的元器件 | Preserve Existing Components | Description |
| 更新 | Update | Radio button |
| 覆盖已存在的元器件 | Overwrite Existing Components | Description |
| 转换进度 | Export Progress | Card title |
| 数据抓取 | Data Fetch | Step label |
| 数据处理 | Data Processing | Step label |
| 文件写入 | File Writing | Step label |
| 转换结果 | Export Results | Card title |
| 重试失败项 | Retry Failed Items | Button |
| 正在转换... | Exporting... | Button state |
| 开始转换 | Start Export | Button |
| 正在停止... | Stopping... | Button state |
| 停止转换 | Stop Export | Button |
| 导出统计 | Export Statistics | Card title |
| 基本统计 | Basic Statistics | Title |
| 总数 | Total | Statistics item |
| 成功 | Success | Statistics item |
| 失败 | Failed | Statistics item |
| 成功率 | Success Rate | Statistics item |
| 时间统计 | Time Statistics | Title |
| 总耗时 | Total Time | Statistics item |
| 平均抓取 | Avg Fetch | Statistics item |
| 平均处理 | Avg Process | Statistics item |
| 平均写入 | Avg Write | Statistics item |
| 符号 | Symbols | Statistics item |
| 封装 | Footprints | Statistics item |
| 网络统计 | Network Statistics | Title |
| 总请求数 | Total Requests | Statistics item |
| 重试次数 | Retries | Statistics item |
| 平均延迟 | Avg Latency | Statistics item |
| 速率限制 | Rate Limit | Statistics item |
| 打开详细统计报告 | Open Detailed Report | Button |
| 打开导出目录 | Open Export Directory | Button |

### ResultListItem Context

| Chinese Original | English Translation | Context |
|-----------------|---------------------|---------|
| 符号: %1 | Symbol: %1 | Tooltip |
| 封装: %1 | Footprint: %1 | Tooltip |
| 3D模型: %1 | 3D Model: %1 | Tooltip |
| 已导出 | Exported | Status |
| 未完成 | Pending | Status |
| 重试 | Retry | Tooltip |

### LanguageManager Context

| Chinese Original | English Translation | Context |
|-----------------|---------------------|---------|
| 语言设置 | Language Settings | Title |
| 跟随系统 | System Default | Option |
| 简体中文 | Simplified Chinese | Option |
| English | English | Option |
| 语言切换将在重启后完全生效 | Language changes will take full effect after restart | Hint |

## Usage

### 1. Using Translation in QML

```qml
import QtQuick
import EasyKiconverter_Cpp_Version 1.0

Text {
    text: qsTr("开始转换")
}

// With parameters
Text {
    text: qsTr("共 %1 个元器件").arg(componentCount)
}
```

### 2. Using Translation in C++

```cpp
#include <QCoreApplication>
#include "src/core/LanguageManager.h"

// Get translated string
QString text = QCoreApplication::translate("Context", "Source String");

// Switch language
EasyKiConverter::LanguageManager::instance()->setLanguage("en");
```

### 3. Switching Language in QML

```qml
import EasyKiconverter_Cpp_Version 1.0

Button {
    text: "English"
    onClicked: LanguageManager.setLanguage("en")
}

Button {
    text: "简体中文"
    onClicked: LanguageManager.setLanguage("zh_CN")
}
```

## Build and Deployment

### 1. Generate Translation Files (.qm)

In development environment, use Qt Linguist tools or command line to generate .qm files:

```bash
lrelease resources/translations/translations_easykiconverter_en.ts
lrelease resources/translations/translations_easykiconverter_zh_CN.ts
```

Generated .qm files should be placed at:
- `resources/translations/translations_easykiconverter_en.qm`
- `resources/translations/translations_easykiconverter_zh_CN.qm`

### 2. Update Translation Files

When adding new translation strings, update .ts files:

```bash
lupdate src/ui/qml/MainWindow.qml -ts resources/translations/translations_easykiconverter_en.ts
lupdate src/ui/qml/MainWindow.qml -ts resources/translations/translations_easykiconverter_zh_CN.ts
```

### 3. CMake Build Commands

```bash
# Configure
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# Build
cmake --build build --config Debug
```

## Technical Details

### 1. Qt 6 qmlRegisterSingletonType Changes

In Qt 6, `qmlRegisterSingletonType` parameter count reduced from 6 to 5:

```cpp
// Qt 5 style (6 parameters)
qmlRegisterSingletonType<QObject>(
    "ModuleUri",           // URI
    "VersionMajor",        // Version Major
    1,                     // Version Minor
    0,                     // Version Major
    "SingletonName",       // Name
    callback);             // Callback

// Qt 6 style (5 parameters)
qmlRegisterSingletonType<QObject>(
    "ModuleUri",           // URI
    1,                     // Version Major
    0,                     // Version Minor
    "SingletonName",       // Name
    callback);             // Callback
```

### 2. Language Detection Logic

System language detection is disabled, returns English directly:

```cpp
QString LanguageManager::detectSystemLanguage() const {
    // No longer using system language detection, returns English directly
    return "en";
}
```

### 3. Translation File Paths

Translation files use Qt resource system (auto-generated by `qt_add_translations`):

```cpp
// Translation file path format (using /i18n prefix)
QString resourcePath = QString(":/i18n/translations_easykiconverter_%1.qm").arg(languageCode);

// Fallback: filesystem path (useful during development)
QString localPath = QString("resources/translations/translations_easykiconverter_%1.qm").arg(languageCode);
```

### 4. ConfigService Persistence

Language settings are persisted via ConfigService to config file:

```cpp
// Load language settings
auto* configService = ConfigService::instance();
m_currentLanguage = configService->getLanguage();

// Save language settings
configService->setLanguage(m_currentLanguage);
```

## Known Issues and Limitations

1. **Dynamic Translation Update**: Current implementation supports translation file hot-reload via `refreshRequired` signal, but UI refresh may require restart to fully apply.

## Future Improvements

1. **Generate .qm files**: Add lrelease step in CMake to auto-generate .qm files
2. **Dynamic Translation Update**: Implement language switching without restart
3. **More Language Support**: Add other languages (e.g., Traditional Chinese, Japanese, Korean)
4. **Translation Completeness Check**: Add automated tests to ensure all strings are translated

## Testing Suggestions

1. **Manual Language Switching Test**: Test language switching via HeaderSection UI
2. **Translation Completeness Test**: Check all UI elements have translations
3. **Parameterized String Test**: Verify strings with parameters (e.g., "Total %1 Components") display correctly
4. **Cross-Platform Test**: Test language switching on Windows, Linux, and macOS
5. **Translation File Loading Test**: Verify translation file path resolution works correctly

## References

- [Qt Internationalization](https://doc.qt.io/qt-6/internationalization.html)
- [Qt Linguist Manual](https://doc.qt.io/qt-6/linguist-manual.html)
- [QTranslator Class](https://doc.qt.io/qt-6/qtranslator.html)
- [QLocale Class](https://doc.qt.io/qt-6/qlocale.html)

## Contributors

- iFlow CLI Assistant

## License

GPL-3.0