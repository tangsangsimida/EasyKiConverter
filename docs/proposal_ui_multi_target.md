# UI Multi‑Target Export Proposal（已落地设计记录）

> 状态：已实现。本文保留原始设计意图；当前实现以 `ExportSettingsBaseCard.qml`、目标设置卡片、导出工厂和 `docs/developer/CONVERSION_MAPPING.md` 为准。

## 背景
- 当前 UI 已支持 **EasyEDA → KiCad** 和 **EasyEDA → Altium Designer (AD)**。
- 后续仍可按同一目标抽象扩展更多库格式。
- 直接在现有 QML 卡片中硬编码新选项会导致代码膨胀、维护难度大、 UI 不一致。

## 目标
1. **在同一界面** 支持多目标库的切换。
2. **插件化**：通过 JSON 注册表即可新增目标，无需改动 C++/QML 代码。
3. **保持高级视觉**（暗光模式、玻璃化、微动画）不受业务变更影响。
4. **保持 MVVM**：所有业务逻辑仍在 ViewModel 中，UI 只负责展示。

## 关键设计原则
| 原则 | 说明 |
|------|------|
| 单一职责 | 每个 QML 组件仅展示，不处理业务。 |
| 配置驱动 | 用 JSON 描述目标、图标、专属设置字段。 |
| 动态加载 | `Loader` + `Component` 在运行时注入目标‑特定卡片。 |
| 统一主题/动画 | 所有颜色、圆角、阴影抽取到 `AppStyle`，子卡片直接引用。

## 架构升级方案
### 1. 新增 `ExportTargetModel`
- **职责**：管理可用目标列表、当前选择、目标元数据（图标、对应 QML 组件路径）。
- **实现**：读取 `resources/export_plugins.json`（见示例），使用 `QMap<QString, QVariantMap>` 保存每个目标的配置。 
- **信号**：`currentTargetChanged`、`targetMetadataChanged`。

### 2. 抽象导出设置卡片
- **`ExportSettingsBaseCard.qml`**：通用 UI（输出路径、库名称、缓存目录）+ **目标切换下拉框**。
- **目标‑特定子卡**：`KiCadSettingsCard.qml`、`AltiumSettingsCard.qml` 等，仅包含该目标独有的开关、选项。
- **动态加载**：`Loader` 根据 `ExportTargetModel.currentTarget` 选择对应子卡组件。

### 3. ViewModel 适配
- `ExportSettingsViewModel` 持有 `QObject* targetOptions`，在目标切换时实例化对应的 **选项对象**（如 `KiCadExportOptions`、`AltiumExportOptions`），并通过 `Q_PROPERTY(QObject* targetOptions)` 暴露给 QML。
- 每个选项对象实现自己的 `Q_PROPERTY`（例如 KiCad 的 `exportSymbol`、Altium 的 `exportComponent`），并提供统一的 **`apply()`** 接口，供导出服务统一读取。

### 4. 插件化注册（JSON 示例）
```json
{
  "plugins": [
    {
      "id": "kicad",
      "displayName": "KiCad",
      "icon": "kicad.svg",
      "optionsComponent": "KiCadSettingsCard.qml"
    },
    {
      "id": "altium",
      "displayName": "Altium Designer",
      "icon": "altium.svg",
      "optionsComponent": "AltiumSettingsCard.qml"
    }
  ]
}
```
- 只需在此文件里新增条目，即可在 UI 中出现新目标并自动加载对应设置卡片。

### 5. 视觉统一
- 所有卡片使用 `AppStyle.Card`（背景、圆角、阴影）。
- 目标切换时使用 `OpacityAnimator` 实现淡入淡出，保持 **微动画** 体验。
- 图标、颜色通过 `ExportTargetModel.targetIcon()` 动态获取，保持统一风格。

## 实施步骤
1. **创建 `ExportTargetModel`**（C++）并在 `main.cpp` 注入到 QML 根对象。
2. **搬迁原 ExportSettings 卡**：
   - 将原有通用 UI 迁入 `ExportSettingsBaseCard.qml`。
   - 为 KiCad 创建 `KiCadSettingsCard.qml`（复制原有 KiCad 开关）。
   - 为 Altium 创建 `AltiumSettingsCard.qml`（后期实现）。
3. **改造 `ExportSettingsViewModel`**：加入 `targetOptions` 管理逻辑并响应 `ExportTargetModel` 信号。
4. **在 `MainWindow.qml`** 用 `<ExportSettingsBaseCard>` 替换原 `<ExportSettingsCard>`，注入 `exportTargetModel` 与 `exportSettingsViewModel`。
5. **编写 `export_plugins.json`** 并放置在 `resources/`。确保构建系统把它打包为资源（`qrc`）。
6. **测试**：
   - 切换目标，检查 UI 动态加载、属性绑定是否正常。
   - 运行一次 KiCad 导出，确保功能未受影响。
   - 新目标必须同时提供能力声明、设置卡片、导出器、测试和文档，不能仅通过 JSON 条目伪造可用目标。
7. **文档 & 代码审查**：更新 `README`、`docs/` 中的 UI 架构章节。

## 预期收益
- **可扩展性**：未来加入任何库格式仅需添加 JSON 条目与对应 QML/选项类。
- **维护成本下降**：所有业务分离，UI 代码不再臃肿。
- **用户体验提升**：统一的暗/亮模式、流畅的切换动画保持高级视觉品质。
- **代码一致性**：保持 MVVM、单一职责的架构风格。

---
**当前结果**：通用设置卡片及 KiCad/Altium 目标卡片已经存在。目标能力由目标模型和 C++ 导出工厂共同约束；新增格式时还需要实现相应 IR 导出器、能力声明、测试和文档，不能只修改 JSON。
