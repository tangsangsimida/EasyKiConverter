# MainController 重构方案

## 文档信息

- **版本**: 1.0
- **创建日期**: 2026年1月17日
- **作者**: EasyKiConverter Team
- **状态**: 待实施

## 问题分析

### 当前架构问题

MainController 类已经演变成了一个"上帝对象"(God Object),承担了过多的职责,违反了单一职责原则(SRP)和关注点分离原则。

#### 1. 职责过度集中

MainController 当前承担了以下职责:

- **视图逻辑**: 直接管理 QML 界面状态(进度、消息、组件列表)
- **业务逻辑**: 协调数据获取、解析、转换和导出的核心流程
- **流程控制**: 管理串行和并行的任务执行,处理复杂的任务状态机
- **数据持有**: 作为临时容器存储从网络获取的数据和待导出的数据
- **配置管理**: 管理应用程序配置
- **错误处理**: 处理各种错误情况

#### 2. 高度耦合

- **QML 视图与 MainController 紧密耦合**: 任何 UI 的微小变动都可能需要修改 MainController
- **核心转换逻辑无法独立测试**: 业务逻辑与 UI 控制器混合在一起
- **ComponentExportTask 直接依赖 MainController**: 任务持有 MainController 的原始指针

#### 3. 难以维护和测试

- **代码复杂度高**: MainController 混合了多种不相关的职责,内部逻辑异常复杂
- **单元测试困难**: 由于职责混合,几乎无法进行单元测试
- **修改风险大**: 修改代码时容易引入意想不到的副作用

#### 4. 性能瓶颈

- **QEventLoop 阻塞**: 在工作线程中使用 QEventLoop 实现了"伪异步",阻塞线程
- **无法发挥多线程优势**: 阻塞等待导致无法充分利用多核 CPU
- **UI 可能无响应**: 阻塞操作可能导致 UI 卡顿

## 重构目标

1. **职责分离**: 将 MainController 的职责分解到不同的类中
2. **降低耦合**: 通过引入中间层,降低各模块间的耦合度
3. **提高可测试性**: 使每个模块都可以独立测试
4. **提升性能**: 移除阻塞操作,实现真正的异步处理
5. **易于维护**: 使代码结构更清晰,易于理解和维护

## 重构方案

### 架构设计

采用 **MVVM (Model-View-ViewModel) + Service Layer** 架构:

```
┌─────────────────────────────────────────────────────────┐
│                    UI Layer (View)                      │
│  ┌──────────────────────────────────────────────────┐  │
│  │  QML Interface (MainWindow.qml, Card.qml, etc.) │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│              ViewModel Layer (ViewModel)               │
│  ┌──────────────────┐  ┌──────────────────┐            │
│  │ComponentListVM   │  │ExportSettingsVM  │            │
│  │- componentList   │  │- exportOptions   │            │
│  │- addComponent()  │  │- outputPath      │            │
│  │- removeComponent()│ │- startExport()   │            │
│  └──────────────────┘  └──────────────────┘            │
│  ┌──────────────────┐  ┌──────────────────┐            │
│  │ExportProgressVM  │  │ThemeSettingsVM   │            │
│  │- progress        │  │- isDarkMode      │            │
│  │- status          │  │- toggleTheme()   │            │
│  │- results         │  │                  │            │
│  └──────────────────┘  └──────────────────┘            │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│              Service Layer (Service)                    │
│  ┌──────────────────────────────────────────────────┐  │
│  │  ComponentService                                │  │
│  │  - fetchComponentData()                          │  │
│  │  - validateComponentId()                         │  │
│  │  - parseBomFile()                                │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │  ExportService                                   │  │
│  │  - exportSymbol()                                │  │
│  │  - exportFootprint()                             │  │
│  │  - export3DModel()                               │  │
│  │  - executeExportPipeline()                       │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │  ConfigService                                   │  │
│  │  - loadConfig()                                  │  │
│  │  - saveConfig()                                  │  │
│  │  - resetConfig()                                 │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                  Model Layer (Model)                    │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Data Models:                                    │  │
│  │  - ComponentData, SymbolData, FootprintData      │  │
│  │  - Model3DData                                   │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Core Engine:                                    │  │
│  │  - EasyedaApi, NetworkUtils                      │  │
│  │  - EasyedaImporter                                │  │
│  │  - ExporterSymbol, ExporterFootprint,            │  │
│  │    Exporter3DModel                               │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 详细设计

#### 1. ViewModel 层

##### 1.1 ComponentListViewModel

负责管理元件列表相关的 UI 状态和操作。

```cpp
// src/ui/viewmodels/ComponentListViewModel.h
class ComponentListViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList componentList READ componentList NOTIFY componentListChanged)
    Q_PROPERTY(int componentCount READ componentCount NOTIFY componentCountChanged)

public:
    explicit ComponentListViewModel(ComponentService *service, QObject *parent = nullptr);
    
    // UI 操作
    Q_INVOKABLE void addComponent(const QString &componentId);
    Q_INVOKABLE void removeComponent(int index);
    Q_INVOKABLE void clearComponentList();
    Q_INVOKABLE void pasteFromClipboard();
    Q_INVOKABLE void selectBomFile(const QString &filePath);

signals:
    void componentListChanged();
    void componentCountChanged();
    void componentAdded(const QString &componentId, bool success, const QString &message);

private:
    ComponentService *m_service;
    QStringList m_componentList;
};
```

##### 1.2 ExportSettingsViewModel

负责管理导出设置相关的 UI 状态和操作。

```cpp
// src/ui/viewmodels/ExportSettingsViewModel.h
class ExportSettingsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString libName READ libName WRITE setLibName NOTIFY libNameChanged)
    Q_PROPERTY(bool exportSymbol READ exportSymbol WRITE setExportSymbol NOTIFY exportSymbolChanged)
    Q_PROPERTY(bool exportFootprint READ exportFootprint WRITE setExportFootprint NOTIFY exportFootprintChanged)
    Q_PROPERTY(bool exportModel3D READ exportModel3D WRITE setExportModel3D NOTIFY exportModel3DChanged)
    Q_PROPERTY(bool overwriteExistingFiles READ overwriteExistingFiles WRITE setOverwriteExistingFiles NOTIFY overwriteExistingFilesChanged)

public:
    explicit ExportSettingsViewModel(QObject *parent = nullptr);
    
    // 配置管理
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void resetConfig();

signals:
    void outputPathChanged();
    void libNameChanged();
    void exportSymbolChanged();
    void exportFootprintChanged();
    void exportModel3DChanged();
    void overwriteExistingFilesChanged();

private:
    QString m_outputPath;
    QString m_libName;
    bool m_exportSymbol;
    bool m_exportFootprint;
    bool m_exportModel3D;
    bool m_overwriteExistingFiles;
};
```

##### 1.3 ExportProgressViewModel

负责管理导出进度和结果相关的 UI 状态。

```cpp
// src/ui/viewmodels/ExportProgressViewModel.h
class ExportProgressViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(int successCount READ successCount NOTIFY successCountChanged)
    Q_PROPERTY(int failureCount READ failureCount NOTIFY failureCountChanged)

public:
    explicit ExportProgressViewModel(ExportService *service, QObject *parent = nullptr);
    
    // 导出控制
    Q_INVOKABLE void startExport(const QStringList &componentIds, const ExportOptions &options);
    Q_INVOKABLE void cancelExport();

signals:
    void progressChanged();
    void statusChanged();
    void isExportingChanged();
    void successCountChanged();
    void failureCountChanged();
    void exportCompleted(int totalCount, int successCount);
    void componentExported(const QString &componentId, bool success, const QString &message);

private slots:
    void handleExportProgress(int current, int total);
    void handleComponentExported(const QString &componentId, bool success, const QString &message);

private:
    ExportService *m_service;
    int m_progress;
    QString m_status;
    bool m_isExporting;
    int m_successCount;
    int m_failureCount;
};
```

##### 1.4 ThemeSettingsViewModel

负责管理主题设置相关的 UI 状态。

```cpp
// src/ui/viewmodels/ThemeSettingsViewModel.h
class ThemeSettingsViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setDarkMode NOTIFY darkModeChanged)

public:
    explicit ThemeSettingsViewModel(QObject *parent = nullptr);

signals:
    void darkModeChanged();

private:
    bool m_isDarkMode;
};
```

#### 2. Service 层

##### 2.1 ComponentService

负责处理与元件相关的业务逻辑,不依赖任何 UI 组件。

```cpp
// src/services/ComponentService.h
class ComponentService : public QObject
{
    Q_OBJECT

public:
    explicit ComponentService(QObject *parent = nullptr);
    
    // 数据获取
    void fetchComponentData(const QString &componentId);
    
    // 验证和解析
    bool validateComponentId(const QString &componentId) const;
    QStringList extractComponentIdFromText(const QString &text) const;
    QStringList parseBomFile(const QString &filePath);
    
    // 数据访问
    ComponentData getComponentData(const QString &componentId) const;

signals:
    void componentDataFetched(const QString &componentId, const ComponentData &data);
    void componentDataFetchFailed(const QString &componentId, const QString &error);

private:
    EasyedaApi *m_api;
    EasyedaImporter *m_importer;
    QMap<QString, ComponentData> m_componentCache;
};
```

##### 2.2 ExportService

负责处理导出相关的业务逻辑,实现真正的异步处理。

```cpp
// src/services/ExportService.h
class ExportService : public QObject
{
    Q_OBJECT

public:
    explicit ExportService(QObject *parent = nullptr);
    
    // 导出操作
    void exportSymbol(const SymbolData &symbol, const QString &filePath);
    void exportFootprint(const FootprintData &footprint, const QString &filePath);
    void export3DModel(const Model3DData &model, const QString &filePath);
    
    // 批量导出
    void executeExportPipeline(const QStringList &componentIds, const ExportOptions &options);
    void cancelExport();
    
    // 配置
    void setExportOptions(const ExportOptions &options);

signals:
    void exportProgress(int current, int total);
    void componentExported(const QString &componentId, bool success, const QString &message);
    void exportCompleted(int totalCount, int successCount);
    void exportFailed(const QString &error);

private:
    ExporterSymbol *m_symbolExporter;
    ExporterFootprint *m_footprintExporter;
    Exporter3DModel *m_modelExporter;
    QThreadPool *m_threadPool;
    bool m_isExporting;
    ExportOptions m_options;
};
```

##### 2.3 ConfigService

负责配置管理。

```cpp
// src/services/ConfigService.h
class ConfigService : public QObject
{
    Q_OBJECT

public:
    static ConfigService* instance();
    
    // 配置管理
    bool loadConfig(const QString &path = QString());
    bool saveConfig(const QString &path = QString());
    void resetToDefaults();
    
    // 配置访问
    QString getOutputPath() const;
    void setOutputPath(const QString &path);
    
    QString getLibName() const;
    void setLibName(const QString &name);
    
    bool getExportSymbol() const;
    void setExportSymbol(bool enabled);
    
    bool getExportFootprint() const;
    void setExportFootprint(bool enabled);
    
    bool getExportModel3D() const;
    void setExportModel3D(bool enabled);
    
    bool getOverwriteExistingFiles() const;
    void setOverwriteExistingFiles(bool enabled);
    
    bool getDarkMode() const;
    void setDarkMode(bool enabled);

signals:
    void configChanged();

private:
    ConfigService(QObject *parent = nullptr);
    ~ConfigService() override;
    
    QJsonObject m_config;
    QString m_configPath;
};
```

#### 3. 重构异步操作

##### 3.1 移除 QEventLoop

当前实现(阻塞):
```cpp
QEventLoop loop;
connect(api, &EasyedaApi::model3DFetched, [&](const QString &uuid, const QByteArray &data) {
    objData = data;
    loop.quit();
});
loop.exec();  // 阻塞等待
```

改进方案(真正的异步):
```cpp
// 使用状态机管理异步流程
class ComponentDataCollector : public QObject
{
    Q_OBJECT
public:
    enum State {
        Idle,
        FetchingComponentInfo,
        FetchingCadData,
        FetchingObjData,
        FetchingStepData,
        Completed,
        Failed
    };
    
    explicit ComponentDataCollector(const QString &componentId, QObject *parent = nullptr);
    
    void start() {
        setState(FetchingComponentInfo);
        m_api->fetchComponentInfo(m_componentId);
    }
    
    State state() const { return m_state; }

signals:
    void stateChanged(State state);
    void dataCollected(const QString &componentId, const ComponentData &data);
    void errorOccurred(const QString &componentId, const QString &error);

private slots:
    void handleComponentInfoFetched(const QJsonObject &data) {
        // 处理组件信息
        setState(FetchingCadData);
        m_api->fetchCadData(m_componentId);
    }
    
    void handleCadDataFetched(const QJsonObject &data) {
        // 处理 CAD 数据
        if (m_export3DModel && !m_uuid.isEmpty()) {
            setState(FetchingObjData);
            m_api->fetch3DModelObj(m_uuid);
        } else {
            complete();
        }
    }
    
    void handleObjDataFetched(const QString &uuid, const QByteArray &data) {
        // 处理 OBJ 数据
        setState(FetchingStepData);
        m_api->fetch3DModelStep(m_uuid);
    }
    
    void handleStepDataFetched(const QString &uuid, const QByteArray &data) {
        // 处理 STEP 数据
        complete();
    }
    
    void handleError(const QString &error) {
        setState(Failed);
        emit errorOccurred(m_componentId, error);
    }

private:
    void setState(State state) {
        if (m_state != state) {
            m_state = state;
            emit stateChanged(state);
        }
    }
    
    void complete() {
        setState(Completed);
        emit dataCollected(m_componentId, m_componentData);
    }

    QString m_componentId;
    State m_state;
    EasyedaApi *m_api;
    ComponentData m_componentData;
    QString m_uuid;
    bool m_export3DModel;
};
```

##### 3.2 解耦任务与控制器

当前实现:
```cpp
class ComponentExportTask : public QObject, public QRunnable {
    MainController *m_controller;  // 直接依赖 MainController
};
```

改进方案:
```cpp
class ComponentExportTask : public QObject, public QRunnable {
    Q_OBJECT
    
public:
    explicit ComponentExportTask(
        const QString &componentId,
        const ExportOptions &options,
        QObject *parent = nullptr);

signals:
    void dataCollected(const QString &componentId, const ComponentData &data);
    void errorOccurred(const QString &componentId, const QString &error);

private:
    QString m_componentId;
    ExportOptions m_options;
    ComponentDataCollector *m_collector;
};
```

### 重构步骤

#### 阶段 1: 创建 Service 层 (1-2 周)

1. **创建 ComponentService**
   - 从 MainController 中提取元件相关的业务逻辑
   - 实现数据获取、验证和解析功能
   - 添加单元测试

2. **创建 ExportService**
   - 从 MainController 中提取导出相关的业务逻辑
   - 实现真正的异步导出流程
   - 添加单元测试

3. **创建 ConfigService**
   - 从 MainController 中提取配置管理逻辑
   - 实现配置的加载、保存和重置
   - 添加单元测试

#### 阶段 2: 创建 ViewModel 层 (1-2 周)

1. **创建 ComponentListViewModel**
   - 从 MainController 中提取元件列表相关的 UI 逻辑
   - 连接到 ComponentService
   - 添加 UI 测试

2. **创建 ExportSettingsViewModel**
   - 从 MainController 中提取导出设置相关的 UI 逻辑
   - 连接到 ConfigService
   - 添加 UI 测试

3. **创建 ExportProgressViewModel**
   - 从 MainController 中提取导出进度相关的 UI 逻辑
   - 连接到 ExportService
   - 添加 UI 测试

4. **创建 ThemeSettingsViewModel**
   - 从 MainController 中提取主题设置相关的 UI 逻辑
   - 连接到 ConfigService
   - 添加 UI 测试

#### 阶段 3: 重构异步操作 (1 周)

1. **创建 ComponentDataCollector**
   - 实现基于状态机的异步数据收集
   - 移除 QEventLoop 阻塞
   - 添加单元测试

2. **重构 ComponentExportTask**
   - 移除对 MainController 的直接依赖
   - 使用信号/槽机制传递结果
   - 添加单元测试

#### 阶段 4: 迁移 QML 界面 (1 周)

1. **更新 MainWindow.qml**
   - 将 MainController 替换为多个 ViewModel
   - 更新属性绑定和方法调用
   - 测试 UI 功能

2. **更新其他 QML 组件**
   - 更新组件间的数据传递
   - 测试组件功能

#### 阶段 5: 清理和优化 (1 周)

1. **删除 MainController**
   - 确保所有功能已迁移
   - 删除 MainController 类
   - 更新文档

2. **性能优化**
   - 优化异步流程
   - 优化资源管理
   - 添加性能测试

3. **代码审查**
   - 进行代码审查
   - 修复发现的问题
   - 更新文档

## 预期收益

### 1. 代码质量提升

- **职责清晰**: 每个类都有明确的职责
- **低耦合**: 各模块间通过接口通信,降低耦合度
- **高内聚**: 相关功能集中在同一个类中

### 2. 可测试性提升

- **单元测试**: 每个模块都可以独立测试
- **集成测试**: 各层接口清晰,易于集成
- **Mock 测试**: 可以轻松创建 Mock 对象

### 3. 性能提升

- **真正的异步**: 移除 QEventLoop 阻塞
- **并发优化**: 充分利用多核 CPU
- **资源优化**: 优化资源管理,减少内存开销

### 4. 可维护性提升

- **代码结构清晰**: 易于理解和维护
- **易于扩展**: 添加新功能更容易
- **易于调试**: 问题定位更快速

## 风险评估

### 1. 兼容性风险

- **风险**: 重构可能导致现有功能不兼容
- **缓解**: 逐步迁移,保持向后兼容
- **测试**: 充分的集成测试

### 2. 性能风险

- **风险**: 新架构可能引入性能问题
- **缓解**: 性能测试,优化关键路径
- **监控**: 添加性能监控

### 3. 时间风险

- **风险**: 重构可能比预期耗时更长
- **缓解**: 分阶段实施,及时调整计划
- **优先级**: 优先重构核心功能

## 总结

通过引入 ViewModel 和 Service 层,将 MainController 的职责分解,可以实现:

1. **职责分离**: 每个类都有明确的职责
2. **降低耦合**: 各模块间通过接口通信
3. **提高可测试性**: 每个模块都可以独立测试
4. **提升性能**: 移除阻塞操作,实现真正的异步处理
5. **易于维护**: 代码结构更清晰,易于理解和维护

这个重构方案将显著提升代码质量、可测试性和可维护性,为项目的长期发展奠定良好的基础。

---

**文档版本**: 1.0  
**创建日期**: 2026年1月17日  
**维护者**: EasyKiConverter Team