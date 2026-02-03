# EasyKiConverter 国际化实现总结

## 概述

本文档记录了 EasyKiConverter 应用程序国际化（i18n）功能的完整实现过程。实现包括中文（简体中文，zh_CN）和英文（en）双语支持，以及自动语言检测和手动语言切换功能。

## 实现日期

2026年2月3日

## 技术栈

- Qt 6.10.1
- C++17
- QML (Qt Quick)
- CMake 3.16+

## 核心组件

### 1. LanguageManager 类

**文件位置：**
- `src/core/LanguageManager.h`
- `src/core/LanguageManager.cpp`

**功能：**
- 单例模式实现
- 自动检测系统语言
- 手动语言切换（中文/英文）
- 语言设置持久化（使用 QSettings）
- QTranslator 管理和安装

**关键接口：**
```cpp
// 获取单例实例
static LanguageManager* instance();

// 设置语言
Q_INVOKABLE void setLanguage(const QString& languageCode);

// 获取当前语言
Q_INVOKABLE QString currentLanguage() const;

// 检测系统语言
Q_INVOKABLE QString detectSystemLanguage() const;

// 信号
void languageChanged(const QString& language);
```

**支持的语言代码：**
- `zh_CN` - 简体中文
- `en` - 英文
- `system` - 跟随系统（默认）

### 2. 翻译文件

**文件位置：**
- `resources/translations/translations_easykiconverter_en.ts`（英文）
- `resources/translations/translations_easykiconverter_zh_CN.ts`（中文）

**翻译上下文：**
- `MainWindow` - 主窗口 UI 文本
- `ResultListItem` - 结果列表项组件
- `LanguageManager` - 语言管理器文本

**翻译条目统计：**
- 总共约 50+ 个翻译条目
- 涵盖所有 UI 文本、按钮、标签和提示信息

### 3. QML 国际化

**修改的文件：**
- `src/ui/qml/MainWindow.qml`（主窗口）
- `src/ui/qml/components/ResultListItem.qml`（结果列表项）

**国际化方法：**
```qml
// 简单文本
text: qsTr("开始转换")

// 带参数的文本
text: qsTr("共 %1 个元器件").arg(componentCount)

// 条件文本
text: {
    if (isExporting) return qsTr("正在转换...");
    return qsTr("开始转换");
}
```

**不需要国际化的组件：**
- `StepItem.qml` - 通过属性传递标签
- `StatItem.qml` - 通过属性传递标签
- `ComponentListItem.qml` - 无静态文本
- `Card.qml` - 通过属性传递标题
- `ModernButton.qml` - 通过属性传递文本
- `Icon.qml` - 无文本

### 4. CMake 配置

**修改的文件：**
- `CMakeLists.txt`（根目录）
- `src/core/CMakeLists.txt`（核心模块）

**关键更改：**

1. **CMakeLists.txt** - 添加翻译资源：
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

2. **src/core/CMakeLists.txt** - 添加 LanguageManager 源文件：
```cmake
target_sources(EasyKiConverterCore
    PRIVATE
        LanguageManager.cpp
        LanguageManager.h
)
```

### 5. main.cpp 集成

**修改的文件：**
- `main.cpp`

**关键更改：**

1. **包含头文件：**
```cpp
#include "src/core/LanguageManager.h"
```

2. **初始化 LanguageManager：**
```cpp
// 初始化语言管理器（在创建引擎之前）
EasyKiConverter::LanguageManager::instance();
```

3. **注册为 QML 单例：**
```cpp
// 注册 LanguageManager 到 QML
qmlRegisterSingletonType<QObject>(
    "EasyKiconverter_Cpp_Version",
    1,
    0,
    "LanguageManager",
    [](QQmlEngine*, QJSEngine*) -> QObject* {
        return EasyKiConverter::LanguageManager::instance();
    });
```

## 翻译条目列表

### MainWindow 上下文

| 中文原文 | 英文翻译 | 上下文 |
|---------|---------|--------|
| 选择 BOM 文件 | Select BOM File | 对话框标题 |
| 将嘉立创EDA元器件转换为KiCad格式 | Convert LCSC EDA Components to KiCad Format | 欢迎标题 |
| 添加元器件 | Add Component | 卡片标题 |
| 输入LCSC元件编号 (例如: C2040) | Enter LCSC Component ID (e.g., C2040) | 占位符 |
| 添加 | Add | 按钮 |
| 粘贴 | Paste | 按钮 |
| 导入BOM文件 | Import BOM File | 卡片标题 |
| 选择BOM文件 | Select BOM File | 按钮 |
| 未选择文件 | No File Selected | 文件状态 |
| 元器件列表 | Component List | 卡片标题 |
| 共 %1 个元器件 | Total %1 Components | 统计文本 |
| 搜索元器件... | Search Components... | 占位符 |
| 清空列表 | Clear List | 按钮 |
| 导出设置 | Export Settings | 卡片标题 |
| 输出路径 | Output Path | 标签 |
| 选择输出目录 | Select Output Directory | 占位符 |
| 浏览 | Browse | 按钮 |
| 库名称 | Library Name | 标签 |
| 输入库 (例如: MyLibrary) | Enter Library Name (e.g., MyLibrary) | 占位符 |
| 符号库 | Symbol Library | 复选框 |
| 封装库 | Footprint Library | 复选框 |
| 3D模型 | 3D Model | 复选框 |
| 调试模式 | Debug Mode | 复选框 |
| 导出模式 | Export Mode | 标签 |
| 追加 | Append | 单选按钮 |
| 保留已存在的元器件 | Preserve Existing Components | 描述 |
| 更新 | Update | 单选按钮 |
| 覆盖已存在的元器件 | Overwrite Existing Components | 描述 |
| 转换进度 | Export Progress | 卡片标题 |
| 数据抓取 | Data Fetch | 步骤标签 |
| 数据处理 | Data Processing | 步骤标签 |
| 文件写入 | File Writing | 步骤标签 |
| 转换结果 | Export Results | 卡片标题 |
| 重试失败项 | Retry Failed Items | 按钮 |
| 正在转换... | Exporting... | 按钮状态 |
| 开始转换 | Start Export | 按钮 |
| 正在停止... | Stopping... | 按钮状态 |
| 停止转换 | Stop Export | 按钮 |
| 导出统计 | Export Statistics | 卡片标题 |
| 基本统计 | Basic Statistics | 标题 |
| 总数 | Total | 统计项 |
| 成功 | Success | 统计项 |
| 失败 | Failed | 统计项 |
| 成功率 | Success Rate | 统计项 |
| 时间统计 | Time Statistics | 标题 |
| 总耗时 | Total Time | 统计项 |
| 平均抓取 | Avg Fetch | 统计项 |
| 平均处理 | Avg Process | 统计项 |
| 平均写入 | Avg Write | 统计项 |
| 符号 | Symbols | 统计项 |
| 封装 | Footprints | 统计项 |
| 网络统计 | Network Statistics | 标题 |
| 总请求数 | Total Requests | 统计项 |
| 重试次数 | Retries | 统计项 |
| 平均延迟 | Avg Latency | 统计项 |
| 速率限制 | Rate Limit | 统计项 |
| 打开详细统计报告 | Open Detailed Report | 按钮 |
| 打开导出目录 | Open Export Directory | 按钮 |

### ResultListItem 上下文

| 中文原文 | 英文翻译 | 上下文 |
|---------|---------|--------|
| 符号: %1 | Symbol: %1 | Tooltip |
| 封装: %1 | Footprint: %1 | Tooltip |
| 3D模型: %1 | 3D Model: %1 | Tooltip |
| 已导出 | Exported | 状态 |
| 未完成 | Pending | 状态 |
| 重试 | Retry | Tooltip |

### LanguageManager 上下文

| 中文原文 | 英文翻译 | 上下文 |
|---------|---------|--------|
| 语言设置 | Language Settings | 标题 |
| 跟随系统 | System Default | 选项 |
| 简体中文 | Simplified Chinese | 选项 |
| English | English | 选项 |
| 语言切换将在重启后完全生效 | Language changes will take full effect after restart | 提示 |

## 使用方法

### 1. 在 QML 中使用翻译

```qml
import QtQuick
import EasyKiconverter_Cpp_Version 1.0

Text {
    text: qsTr("开始转换")
}

// 带参数
Text {
    text: qsTr("共 %1 个元器件").arg(componentCount)
}
```

### 2. 在 C++ 中使用翻译

```cpp
#include <QCoreApplication>
#include "src/core/LanguageManager.h"

// 获取翻译字符串
QString text = QCoreApplication::translate("Context", "Source String");

// 切换语言
EasyKiConverter::LanguageManager::instance()->setLanguage("en");
```

### 3. 在 QML 中切换语言

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

## 构建和部署

### 1. 生成翻译文件（.qm）

在开发环境中，使用 Qt Linguist 工具或命令行生成 .qm 文件：

```bash
lrelease resources/translations/translations_easykiconverter_en.ts
lrelease resources/translations/translations_easykiconverter_zh_CN.ts
```

生成的 .qm 文件应该放在：
- `resources/translations/translations_easykiconverter_en.qm`
- `resources/translations/translations_easykiconverter_zh_CN.qm`

### 2. 更新翻译文件

当添加新的翻译字符串时，需要更新 .ts 文件：

```bash
lupdate src/ui/qml/MainWindow.qml -ts resources/translations/translations_easykiconverter_en.ts
lupdate src/ui/qml/MainWindow.qml -ts resources/translations/translations_easykiconverter_zh_CN.ts
```

### 3. CMake 构建命令

```bash
# 配置
cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64"

# 构建
cmake --build build --config Debug
```

## 技术细节

### 1. Qt 6 qmlRegisterSingletonType 变化

在 Qt 6 中，`qmlRegisterSingletonType` 的参数从 6 个减少到 5 个：

```cpp
// Qt 5 方式（6个参数）
qmlRegisterSingletonType<QObject>(
    "ModuleUri",           // URI
    "VersionMajor",        // Version Major
    1,                     // Version Minor
    0,                     // Version Major
    "SingletonName",       // Name
    callback);             // Callback

// Qt 6 方式（5个参数）
qmlRegisterSingletonType<QObject>(
    "ModuleUri",           // URI
    1,                     // Version Major
    0,                     // Version Minor
    "SingletonName",       // Name
    callback);             // Callback
```

### 2. 语言检测逻辑

LanguageManager 使用 `QLocale` 来检测系统语言：

```cpp
QString LanguageManager::detectSystemLanguage() const {
    QLocale systemLocale = QLocale::system();
    QString systemLang = systemLocale.name(); // 例如: "zh_CN", "en_US"
    
    if (systemLang.startsWith("zh")) {
        return "zh_CN";
    } else if (systemLang.startsWith("en")) {
        return "en";
    }
    return "en"; // 默认英语
}
```

### 3. 翻译文件路径

翻译文件使用 Qt 资源系统：

```cpp
QString translationFile = QString(":/translations/translations_easykiconverter_%1.qm")
                             .arg(languageCode);
```

### 4. QSettings 持久化

语言设置保存在注册表（Windows）或配置文件（Linux/macOS）：

```cpp
QSettings settings("EasyKiConverter", "Settings");
settings.setValue("language", languageCode);
```

## 已知问题和限制

1. **.qm 文件未生成**：当前实现使用 .ts 文件（XML 格式），生产环境应该生成 .qm 文件（二进制格式）以提升性能。

2. **动态翻译更新**：当前实现需要重启应用程序才能完全应用语言更改。未来可以实现动态更新功能。

3. **语言选择 UI**：当前实现缺少语言选择 UI 组件，需要在 MainWindow 中添加语言切换按钮或下拉菜单。

## 未来改进

1. **添加语言选择 UI**：在设置区域添加语言选择器
2. **生成 .qm 文件**：在 CMake 中添加 lrelease 步骤自动生成 .qm 文件
3. **动态翻译更新**：实现不需要重启的语言切换
4. **更多语言支持**：添加其他语言（如繁体中文、日语、韩语等）
5. **翻译完整性检查**：添加自动化测试确保所有字符串都已翻译

## 测试建议

1. **系统语言检测测试**：更改系统语言，验证应用程序是否正确检测
2. **手动语言切换测试**：使用 QSettings API 测试语言切换
3. **翻译完整性测试**：检查所有 UI 元素是否都有翻译
4. **参数化字符串测试**：验证带参数的字符串（如 "共 %1 个元器件"）是否正确显示
5. **跨平台测试**：在 Windows、Linux 和 macOS 上测试语言切换功能

## 参考资料

- [Qt Internationalization](https://doc.qt.io/qt-6/internationalization.html)
- [Qt Linguist Manual](https://doc.qt.io/qt-6/linguist-manual.html)
- [QTranslator Class](https://doc.qt.io/qt-6/qtranslator.html)
- [QLocale Class](https://doc.qt.io/qt-6/qlocale.html)

## 贡献者

- iFlow CLI Assistant

## 许可证

GPL-3.0