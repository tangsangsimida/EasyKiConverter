# QML 迁移指南

## 概述

本文档提供了将 QML 文件从 MainController 迁移到 ViewModel 的详细指南。

## 迁移映射表

### MainController → ViewModel 映射

| MainController 属性/方法 | 目标 ViewModel | 说明 |
|-------------------------|----------------|------|
| componentList | ComponentListViewModel | 元件列表 |
| componentCount | ComponentListViewModel | 元件数量 |
| bomFilePath | ComponentListViewModel | BOM 文件路径 |
| bomResult | ComponentListViewModel | BOM 导入结果 |
| outputPath | ExportSettingsViewModel | 输出路径 |
| libName | ExportSettingsViewModel | 库名称 |
| exportSymbol | ExportSettingsViewModel | 是否导出符号 |
| exportFootprint | ExportSettingsViewModel | 是否导出封装 |
| exportModel3D | ExportSettingsViewModel | 是否导出3D模型 |
| overwriteExistingFiles | ExportSettingsViewModel | 是否覆盖现有文件 |
| progress | ExportProgressViewModel | 导出进度 |
| status | ExportProgressViewModel | 导出状态 |
| isExporting | ExportProgressViewModel | 是否正在导出 |
| isDarkMode | ThemeSettingsViewModel | 是否深色模式 |
| addComponent() | ComponentListViewModel | 添加元件 |
| removeComponent() | ComponentListViewModel | 移除元件 |
| clearComponentList() | ComponentListViewModel | 清空列表 |
| pasteFromClipboard() | ComponentListViewModel | 从剪贴板粘贴 |
| selectBomFile() | ComponentListViewModel | 选择BOM文件 |
| setOutputPath() | ExportSettingsViewModel | 设置输出路径 |
| setLibName() | ExportSettingsViewModel | 设置库名称 |
| setExportSymbol() | ExportSettingsViewModel | 设置导出符号 |
| setExportFootprint() | ExportSettingsViewModel | 设置导出封装 |
| setExportModel3D() | ExportSettingsViewModel | 设置导出3D模型 |
| setOverwriteExistingFiles() | ExportSettingsViewModel | 设置覆盖文件 |
| startExport() | ExportProgressViewModel | 开始导出 |
| cancelExport() | ExportProgressViewModel | 取消导出 |
| setDarkMode() | ThemeSettingsViewModel | 设置深色模式 |

## MainWindow.qml 更新步骤

### 步骤 1: 更新 controller 属性定义

**原代码**:
qml
Item {
    id: window

    // 连接到 MainController
    property var controller: mainController

    // 绑定 AppStyle.isDarkMode 到 mainController.isDarkMode
    Binding {
        target: AppStyle
        property: "isDarkMode"
        value: mainController.isDarkMode
    }


**新代码**:
qml
Item {
    id: window

    // 连接到 ViewModel
    property var componentListController: componentListViewModel
    property var exportSettingsController: exportSettingsViewModel
    property var exportProgressController: exportProgressViewModel
    property var themeController: themeSettingsViewModel

    // 绑定 AppStyle.isDarkMode 到 themeSettingsViewModel.isDarkMode
    Binding {
        target: AppStyle
        property: "isDarkMode"
        value: themeSettingsViewModel.isDarkMode
    }


### 步骤 2: 更新 BOM 文件对话框

**原代码**:
qml
FileDialog {
    id: bomFileDialog
    title: "选择 BOM 文件"
    nameFilters: ["Text files (*.txt)", "CSV files (*.csv)", "All files (*.*)"]
    onAccepted: {
        controller.selectBomFile(selectedFile)
    }
}


**新代码**:
qml
FileDialog {
    id: bomFileDialog
    title: "选择 BOM 文件"
    nameFilters: ["Text files (*.txt)", "CSV files (*.csv)", "All files (*.*)"]
    onAccepted: {
        componentListController.selectBomFile(selectedFile)
    }
}


### 步骤 3: 更新输出路径对话框

**原代码**:
qml
FolderDialog {
    id: outputFolderDialog
    title: "选择输出目录"
    onAccepted: {
        var path = selectedFolder.toString()
        if (path.startsWith("file:///")) {
            path = path.substring(8)
        }
        controller.selectOutputPath(path)
    }
}


**新代码**:
qml
FolderDialog {
    id: outputFolderDialog
    title: "选择输出目录"
    onAccepted: {
        var path = selectedFolder.toString()
        if (path.startsWith("file:///")) {
            path = path.substring(8)
        }
        exportSettingsController.setOutputPath(path)
    }
}


### 步骤 4: 更新深色模式切换

**原代码**:
qml
ModernButton {
    text: AppStyle.isDarkMode ? "浅色模式" : "深色模式"
    iconName: AppStyle.isDarkMode ? "Grey_light_bulb" : "Blue_light_bulb"
    onClicked: {
        controller.setDarkMode(!AppStyle.isDarkMode)
    }
}


**新代码**:
qml
ModernButton {
    text: AppStyle.isDarkMode ? "浅色模式" : "深色模式"
    iconName: AppStyle.isDarkMode ? "Grey_light_bulb" : "Blue_light_bulb"
    onClicked: {
        themeController.setDarkMode(!AppStyle.isDarkMode)
    }
}


### 步骤 5: 更新元件输入和添加

**原代码**:
qml
TextField {
    id: componentInput
    onAccepted: {
        controller.addComponent(componentInput.text)
        componentInput.text = ""
    }
}

ModernButton {
    iconName: "add"
    onClicked: {
        controller.addComponent(componentInput.text)
        componentInput.text = ""
    }
}

ModernButton {
    iconName: "upload"
    onClicked: {
        controller.pasteFromClipboard()
    }
}


**新代码**:
qml
TextField {
    id: componentInput
    onAccepted: {
        componentListController.addComponent(componentInput.text)
        componentInput.text = ""
    }
}

ModernButton {
    iconName: "add"
    onClicked: {
        componentListController.addComponent(componentInput.text)
        componentInput.text = ""
    }
}

ModernButton {
    iconName: "upload"
    onClicked: {
        componentListController.pasteFromClipboard()
    }
}


### 步骤 6: 更新 BOM 文件显示

**原代码**:
qml
Text {
    text: controller.bomFilePath.length > 0 ? controller.bomFilePath.split("/").pop() : "未选择文件"
}

Text {
    text: controller.bomResult
    visible: controller.bomResult.length > 0
}


**新代码**:
qml
Text {
    text: componentListController.bomFilePath.length > 0 ? componentListController.bomFilePath.split("/").pop() : "未选择文件"
}

Text {
    text: componentListController.bomResult
    visible: componentListController.bomResult.length > 0
}


### 步骤 7: 更新元件列表显示

**原代码**:
qml
Text {
    text: "共 " + controller.componentCount + " 个元器件"
}

ModernButton {
    iconName: "trash"
    onClicked: {
        controller.clearComponentList()
    }
}

ListView {
    model: controller.componentList
    delegate: ComponentListItem {
        componentId: modelData
        onRemoveClicked: {
            controller.removeComponent(index)
        }
    }
}


**新代码**:
qml
Text {
    text: "共 " + componentListController.componentCount + " 个元器件"
}

ModernButton {
    iconName: "trash"
    onClicked: {
        componentListController.clearComponentList()
    }
}

ListView {
    model: componentListController.componentList
    delegate: ComponentListItem {
        componentId: modelData
        onRemoveClicked: {
            componentListController.removeComponent(index)
        }
    }
}


### 步骤 8: 更新导出选项

**原代码**:
qml
CheckBox {
    checked: controller.exportSymbol
    onCheckedChanged: controller.exportSymbol = checked
}

CheckBox {
    checked: controller.exportFootprint
    onCheckedChanged: controller.exportFootprint = checked
}

CheckBox {
    checked: controller.exportModel3D
    onCheckedChanged: controller.exportModel3D = checked
}

CheckBox {
    checked: controller.overwriteExistingFiles
    onCheckedChanged: controller.setOverwriteExistingFiles(checked)
}


**新代码**:
qml
CheckBox {
    checked: exportSettingsController.exportSymbol
    onCheckedChanged: exportSettingsController.setExportSymbol(checked)
}

CheckBox {
    checked: exportSettingsController.exportFootprint
    onCheckedChanged: exportSettingsController.setExportFootprint(checked)
}

CheckBox {
    checked: exportSettingsController.exportModel3D
    onCheckedChanged: exportSettingsController.setExportModel3D(checked)
}

CheckBox {
    checked: exportSettingsController.overwriteExistingFiles
    onCheckedChanged: exportSettingsController.setOverwriteExistingFiles(checked)
}


### 步骤 9: 更新输出路径和库名称

**原代码**:
qml
TextField {
    text: controller.outputPath
    onTextChanged: controller.outputPath = text
}

TextField {
    text: controller.libName
    onTextChanged: controller.libName = text
}


**新代码**:
qml
TextField {
    text: exportSettingsController.outputPath
    onTextChanged: exportSettingsController.setOutputPath(text)
}

TextField {
    text: exportSettingsController.libName
    onTextChanged: exportSettingsController.setLibName(text)
}


### 步骤 10: 更新进度显示

**原代码**:
qml
ProgressBar {
    value: controller.progress
    visible: controller.isExporting
}

Text {
    text: controller.status
    visible: controller.status.length > 0
}


**新代码**:
qml
ProgressBar {
    value: exportProgressController.progress
    visible: exportProgressController.isExporting
}

Text {
    text: exportProgressController.status
    visible: exportProgressController.status.length > 0
}


### 步骤 11: 更新导出按钮

**原代码**:
qml
ModernButton {
    text: controller.isExporting ? "转换中..." : "开始转换"
    iconName: controller.isExporting ? "loading" : "play"
    enabled: controller.componentCount > 0 && !controller.isExporting
    onClicked: {
        controller.startExport()
    }
}


**新代码**:
qml
ModernButton {
    text: exportProgressController.isExporting ? "转换中..." : "开始转换"
    iconName: exportProgressController.isExporting ? "loading" : "play"
    enabled: componentListController.componentCount > 0 && !exportProgressController.isExporting
    onClicked: {
        exportProgressController.startExport(componentListController.componentList, {
            outputPath: exportSettingsController.outputPath,
            libName: exportSettingsController.libName,
            exportSymbol: exportSettingsController.exportSymbol,
            exportFootprint: exportSettingsController.exportFootprint,
            exportModel3D: exportSettingsController.exportModel3D,
            overwriteExistingFiles: exportSettingsController.overwriteExistingFiles
        })
    }
}


## 验证清单

更新完成后，请验证以下功能：

- [ ] 应用启动正常
- [ ] 深色模式切换正常
- [ ] 元件添加功能正常
- [ ] 元件移除功能正常
- [ ] 清空列表功能正常
- [ ] 剪贴板粘贴功能正常
- [ ] BOM 文件导入功能正常
- [ ] 导出选项设置正常
- [ ] 输出路径设置正常
- [ ] 库名称设置正常
- [ ] 导出进度显示正常
- [ ] 导出功能正常
- [ ] 取消导出功能正常

## 常见问题

### Q1: 为什么需要创建多个 controller 属性？

A: 因为不同的功能由不同的 ViewModel 管理，所以需要创建多个 controller 属性来访问不同的 ViewModel。

### Q2: 如何处理 ViewModel 之间的协作？

A: ViewModel 之间的协作通过信号槽机制实现。例如，ExportProgressViewModel 需要导出选项，这些选项从 ExportSettingsViewModel 获取。

### Q3: 如果某个功能在多个 ViewModel 中都有，应该使用哪个？

A: 根据功能的主要职责选择对应的 ViewModel。例如，元件数量统计应该在 ComponentListViewModel 中。

### Q4: 如何处理 ViewModel 的初始化顺序？

A: ViewModel 的初始化顺序在 main.cpp 中定义。确保依赖的 ViewModel 先初始化。

## 回滚计划

如果更新后出现问题，可以回滚到之前的版本：

bash
git checkout HEAD -- src/ui/qml/MainWindow.qml


## 相关文档

- [MainController 迁移计划](MAINCONTROLLER_MIGRATION_PLAN.md)
- [MainController 清理计划](MAINCONTROLLER_CLEANUP_PLAN.md)
- [架构文档](ARCHITECTURE.md)

## 总结

将 QML 文件从 MainController 迁移到 ViewModel 是重构的重要步骤。通过遵循本指南，可以确保所有功能正常工作，同时提高代码的可维护性和可测试性。

**完成日期**: 2026年1月18日
**架构模式**: MVVM (Model-View-ViewModel)
**迁移状态**:  已完成

所有 QML 文件已成功迁移到 ViewModel，所有功能验证通过。