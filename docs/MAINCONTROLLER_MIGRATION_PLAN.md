# MainController 迁移计划

## 概述

本文档描述了如何将 MainController 的功能逐步迁移到 ViewModel 和 Service 层。

## 当前 MainController 功能分析

### 1. 元件列表管理

**当前实现**: MainController 直接管理元件列表

**相关方法**:
- `addComponent()`
- `removeComponent()`
- `clearComponentList()`
- `pasteFromClipboard()`
- `selectBomFile()`

**目标 ViewModel**: ComponentListViewModel

**迁移状态**: ✅ 已完成 - ComponentListViewModel 已实现所有功能

### 2. 导出设置管理

**当前实现**: MainController 直接管理导出设置

**相关方法**:
- `setOutputPath()`
- `setLibName()`
- `setExportSymbol()`
- `setExportFootprint()`
- `setExportModel3D()`
- `setOverwriteExistingFiles()`
- `saveConfig()`
- `resetConfig()`

**目标 ViewModel**: ExportSettingsViewModel

**迁移状态**: ✅ 已完成 - ExportSettingsViewModel 已实现所有功能

### 3. 导出进度管理

**当前实现**: MainController 直接管理导出进度

**相关方法**:
- `startExport()`
- `cancelExport()`
- `progress` 属性
- `status` 属性
- `isExporting` 属性

**目标 ViewModel**: ExportProgressViewModel

**迁移状态**: ✅ 已完成 - ExportProgressViewModel 已实现所有功能

### 4. 深色模式管理

**当前实现**: MainController 管理深色模式状态

**相关方法**:
- `setDarkMode()`
- `isDarkMode` 属性

**目标 ViewModel**: ThemeSettingsViewModel

**迁移状态**: ✅ 已完成 - ThemeSettingsViewModel 已实现所有功能

### 5. 配置管理

**当前实现**: MainController 使用 ConfigManager 管理配置

**相关方法**:
- `saveConfig()`
- `resetConfig()`

**目标 Service**: ConfigService

**迁移状态**: ✅ 已完成 - ConfigService 已实现配置管理

### 6. 元件数据获取

**当前实现**: MainController 使用 EasyedaApi 获取数据

**相关方法**:
- 内部数据获取逻辑

**目标 Service**: ComponentService

**迁移状态**: ✅ 已完成 - ComponentService 已实现数据获取

### 7. 导出功能

**当前实现**: MainController 使用 Exporter 类进行导出

**相关方法**:
- 内部导出逻辑

**目标 Service**: ExportService

**迁移状态**: ✅ 已完成 - ExportService 已实现导出功能

## 迁移策略

### 阶段 1: 验证 ViewModel 功能（已完成）

**目标**: 确保所有 ViewModel 已正确实现

**检查清单**:
- [x] ComponentListViewModel 实现所有元件列表管理功能
- [x] ExportSettingsViewModel 实现所有导出设置功能
- [x] ExportProgressViewModel 实现所有进度管理功能
- [x] ThemeSettingsViewModel 实现主题管理功能

### 阶段 2: 更新 QML 绑定（进行中）

**目标**: 将 QML 从 MainController 迁移到 ViewModel

**步骤**:
1. 更新 MainWindow.qml 使用 ViewModel
2. 更新所有 QML 组件的绑定
3. 测试 UI 功能

**当前状态**: ⏳ 需要更新 QML 文件

### 阶段 3: 简化 MainController（待开始）

**目标**: 将 MainController 改为向后兼容层

**步骤**:
1. 保留 MainController 的公共接口
2. 将实现委托给 ViewModel
3. 添加弃用警告

**当前状态**: ⏳ 待开始

### 阶段 4: 移除冗余代码（待开始）

**目标**: 完全移除 MainController 的冗余代码

**步骤**:
1. 确认所有功能已迁移
2. 移除 MainController
3. 更新文档

**当前状态**: ⏳ 待开始

## QML 迁移指南

### 更新 MainWindow.qml

**当前绑定**:
```qml
MainController {
    id: controller
    
    componentList: controller.componentList
    outputPath: controller.outputPath
    // ... 其他绑定
}
```

**目标绑定**:
```qml
ComponentListViewModel {
    id: componentListViewModel
}

ExportSettingsViewModel {
    id: exportSettingsViewModel
}

ExportProgressViewModel {
    id: exportProgressViewModel
}

ThemeSettingsViewModel {
    id: themeSettingsViewModel
}
```

### 更新组件绑定

**元件列表组件**:
```qml
// 旧方式
ListView {
    model: controller.componentList
    onAddClicked: controller.addComponent(id)
}

// 新方式
ListView {
    model: componentListViewModel.componentList
    onAddClicked: componentListViewModel.addComponent(id)
}
```

**导出设置组件**:
```qml
// 旧方式
TextField {
    text: controller.outputPath
    onTextChanged: controller.setOutputPath(text)
}

// 新方式
TextField {
    text: exportSettingsViewModel.outputPath
    onTextChanged: exportSettingsViewModel.setOutputPath(text)
}
```

## MainController 向后兼容层设计

### 设计原则

1. **保持公共接口不变**: QML 绑定不需要修改
2. **委托实现**: 将所有操作委托给 ViewModel
3. **添加弃用警告**: 提示开发者使用新的 ViewModel

### 实现示例

```cpp
class MainController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList componentList READ componentList NOTIFY componentListChanged)
    
public:
    MainController(QObject *parent = nullptr) : QObject(parent) {
        // 创建 ViewModel
        m_componentListViewModel = new ComponentListViewModel(this);
        m_exportSettingsViewModel = new ExportSettingsViewModel(this);
        m_exportProgressViewModel = new ExportProgressViewModel(this);
        m_themeSettingsViewModel = new ThemeSettingsViewModel(this);
        
        // 代理信号
        connect(m_componentListViewModel, &ComponentListViewModel::componentListChanged,
                this, &MainController::componentListChanged);
        // ... 其他信号连接
    }
    
    // Getter 方法（委托给 ViewModel）
    QStringList componentList() const {
        return m_componentListViewModel->componentList();
    }
    
    // Slot 方法（委托给 ViewModel）
    Q_INVOKABLE void addComponent(const QString &componentId) {
        m_componentListViewModel->addComponent(componentId);
    }
    
private:
    ComponentListViewModel *m_componentListViewModel;
    ExportSettingsViewModel *m_exportSettingsViewModel;
    ExportProgressViewModel *m_exportProgressViewModel;
    ThemeSettingsViewModel *m_themeSettingsViewModel;
};
```

## 迁移检查清单

### ComponentListViewModel

- [x] `addComponent()` - 已实现
- [x] `removeComponent()` - 已实现
- [x] `clearComponentList()` - 已实现
- [x] `pasteFromClipboard()` - 已实现
- [x] `selectBomFile()` - 已实现
- [x] `componentList` 属性 - 已实现
- [x] `componentCount` 属性 - 已实现

### ExportSettingsViewModel

- [x] `setOutputPath()` - 已实现
- [x] `setLibName()` - 已实现
- [x] `setExportSymbol()` - 已实现
- [x] `setExportFootprint()` - 已实现
- [x] `setExportModel3D()` - 已实现
- [x] `setOverwriteExistingFiles()` - 已实现
- [x] `saveConfig()` - 已实现
- [x] `resetConfig()` - 已实现
- [x] `startExport()` - 已实现
- [x] `cancelExport()` - 已实现

### ExportProgressViewModel

- [x] `progress` 属性 - 已实现
- [x] `status` 属性 - 已实现
- [x] `isExporting` 属性 - 已实现
- [x] `successCount` 属性 - 已实现
- [x] `failureCount` 属性 - 已实现

### ThemeSettingsViewModel

- [x] `isDarkMode` 属性 - 已实现
- [x] `setDarkMode()` - 已实现

## 迁移时间表

### 第 1 周：验证 ViewModel 功能（已完成）

- [x] 验证 ComponentListViewModel
- [x] 验证 ExportSettingsViewModel
- [x] 验证 ExportProgressViewModel
- [x] 验证 ThemeSettingsViewModel

### 第 2 周：更新 QML 绑定（进行中）

- [ ] 更新 MainWindow.qml
- [ ] 更新所有 QML 组件
- [ ] 测试 UI 功能

### 第 3 周：简化 MainController（待开始）

- [ ] 创建向后兼容层
- [ ] 添加信号代理
- [ ] 添加弃用警告

### 第 4 周：移除冗余代码（待开始）

- [ ] 确认所有功能已迁移
- [ ] 移除 MainController
- [ ] 更新文档

## 风险评估

### 高风险

1. **QML 绑定更新**: 可能导致 UI 功能异常
   - **缓解措施**: 逐步更新，充分测试

2. **信号传递**: 信号代理可能导致性能问题
   - **缓解措施**: 使用 Qt::DirectConnection

### 中风险

1. **向后兼容**: 现有代码可能依赖 MainController
   - **缓解措施**: 保持公共接口不变

2. **测试覆盖**: 需要确保所有功能都被测试
   - **缓解措施**: 增加集成测试

### 低风险

1. **文档更新**: 需要更新所有相关文档
   - **缓解措施**: 提前准备文档模板

## 成功标准

### 功能完整性

- [ ] 所有现有功能正常工作
- [ ] UI 响应速度无明显下降
- [ ] 内存使用无明显增加

### 代码质量

- [ ] 代码可读性提高
- [ ] 代码可维护性提高
- [ ] 代码可测试性提高

### 性能指标

- [ ] 启动时间 < 3 秒
- [ ] UI 响应时间 < 100ms
- [ ] 内存使用 < 100MB

## 相关文档

- [重构计划](../docs/REFACTORING_PLAN.md)
- [架构文档](../docs/ARCHITECTURE.md)
- [单元测试指南](TESTING_GUIDE.md)
- [集成测试指南](INTEGRATION_TEST_GUIDE.md)

## 下一步行动

1. **立即行动**: 更新 QML 文件使用 ViewModel
2. **短期目标**: 完成向后兼容层实现
3. **长期目标**: 完全移除 MainController

## 联系方式

如有问题或建议，请联系项目维护者。