# EasyKiConverter Python 到 C++ 移植计划

## 📋 项目概述

本文档详细说明了将 EasyKiConverter 从 Python 版本（PyQt6）迁移到 C++ 版本（Qt 6 Quick）的完整计划。

## 🎯 移植目标

1. **功能对等**：完全实现 Python 版本的所有功能
2. **性能优化**：利用 C++ 的性能优势，提升转换速度和响应速度
3. **独立部署**：编译为独立的可执行文件，无需 Python 环境
4. **架构优化**：采用更高效的 C++ 架构设计

## 📊 Python 版本架构分析

### 核心模块结构


src/core/
 easyeda/                    # EasyEDA API 和数据处理
    easyeda_api.py         # API 客户端（带重试机制）
    easyeda_importer.py    # 数据导入器
    jlc_datasheet.py       # 数据手册下载
    parameters_easyeda.py  # EasyEDA 参数定义
    svg_path_parser.py     # SVG 路径解析器
 kicad/                      # KiCad 导出引擎
    export_kicad_symbol.py    # 符号导出器
    export_kicad_footprint.py # 封装导出器
    export_kicad_3d_model.py  # 3D 模型导出器
    parameters_kicad_symbol.py  # KiCad 符号参数
    parameters_kicad_footprint.py # KiCad 封装参数
 utils/                      # 工具函数
     geometry_utils.py      # 几何计算工具
     symbol_lib_utils.py    # 符号库工具


### UI 模块结构


src/ui/pyqt6/
 app_main.py                # 主程序入口
 base_main_window.py        # 基础主窗口
 widgets/                   # UI 组件
    component_input_widget.py
    conversion_results_widget.py
    modern_export_options_widget.py
    progress_widget.py
 utils/                     # UI 工具
    bom_parser.py         # BOM 文件解析器
    clipboard_processor.py # 剪贴板处理器
    component_validator.py # 元件验证器
    config_manager.py     # 配置管理器
    modern_ui_components.py # 现代化 UI 组件
 workers/                   # 工作线程
     export_worker.py      # 导出工作线程


### 关键依赖

- **网络请求**：requests 库（带重试机制）
- **数据处理**：json、pandas、numpy
- **Excel 处理**：openpyxl
- **数据验证**：pydantic
- **UI 框架**：PyQt6
- **多线程**：concurrent.futures.ThreadPoolExecutor

## 🏗️ C++ 版本架构设计

### 技术栈选择

| 模块 | Python 技术 | C++ 技术 | 说明 |
|------|------------|----------|------|
| UI 框架 | PyQt6 | Qt 6 Quick (QML) | 更流畅的用户体验 |
| 网络请求 | requests | Qt Network (QNetworkAccessManager) | 原生支持异步 |
| JSON 处理 | json | Qt JSON (QJsonDocument) | 高效解析 |
| 数据验证 | pydantic | 自定义验证类 | 编译时类型检查 |
| Excel 处理 | openpyxl | Qt Xlsx 或第三方库 | 读取 BOM 文件 |
| 多线程 | ThreadPoolExecutor | QThreadPool + QRunnable | Qt 原生线程池 |
| 几何计算 | math | Qt Math (QVector2D, QVector3D) | 向量化计算 |

### C++ 项目目录结构


EasyKiconverter_Cpp_Version/
 CMakeLists.txt              # 主 CMake 配置
 main.cpp                    # 主程序入口
 src/                        # 源代码目录
    core/                   # 核心转换引擎
       easyeda/           # EasyEDA 模块
          EasyedaApi.h/cpp         # API 客户端
          EasyedaImporter.h/cpp    # 数据导入器
          JLCDatasheet.h/cpp       # 数据手册下载
          ParametersEasyeda.h      # 参数定义
          SvgPathParser.h/cpp      # SVG 路径解析
       kicad/             # KiCad 导出模块
          ExporterSymbol.h/cpp     # 符号导出器
          ExporterFootprint.h/cpp  # 封装导出器
          Exporter3DModel.h/cpp    # 3D 模型导出器
          ParametersKicadSymbol.h  # KiCad 符号参数
          ParametersKicadFootprint.h # KiCad 封装参数
       utils/             # 工具模块
           GeometryUtils.h/cpp      # 几何计算
           SymbolLibUtils.h/cpp     # 符号库工具
           NetworkUtils.h/cpp       # 网络工具（重试机制）
    models/                # 数据模型
       ComponentData.h            # 元件数据模型
       SymbolData.h               # 符号数据模型
       FootprintData.h            # 封装数据模型
       Model3DData.h              # 3D 模型数据模型
    ui/                    # UI 模块
       qml/               # QML 文件
          Main.qml                 # 主界面
          components/             # QML 组件
             ComponentInput.qml
             ConversionResults.qml
             ExportOptions.qml
             ProgressBar.qml
          styles/                 # 样式文件
       controllers/       # QML 控制器
          MainController.h/cpp    # 主控制器
          ExportController.h/cpp  # 导出控制器
          ConfigController.h/cpp  # 配置控制器
       utils/             # UI 工具
           BOMParser.h/cpp         # BOM 解析器
           ClipboardProcessor.h/cpp # 剪贴板处理器
           ComponentValidator.h/cpp # 元件验证器
           ConfigManager.h/cpp     # 配置管理器
    workers/               # 工作线程
        ExportWorker.h/cpp         # 导出工作线程
        NetworkWorker.h/cpp        # 网络请求工作线程
 resources/                  # 资源文件
    icons/                 # 图标
    styles/                # 样式
    translations/          # 翻译文件
 tests/                      # 测试代码
    unit/                  # 单元测试
    integration/           # 集成测试
 docs/                       # 文档
    architecture.md        # 架构文档
    api.md                 # API 文档
    migration.md           # 移植文档（本文件）
 build/                      # 构建输出


## 🔄 移植顺序和依赖关系

### 阶段 1：基础架构搭建（第 1-2 周）

**优先级：最高**

#### 1.1 项目初始化
- [ ] 创建 CMakeLists.txt 配置文件
- [ ] 配置 Qt 6 依赖（Quick, Network, Core, Gui）
- [ ] 设置项目目录结构
- [ ] 配置构建系统（Debug/Release）

#### 1.2 基础数据模型
- [ ] 实现基础数据结构（ComponentData, SymbolData 等）
- [ ] 实现数据验证机制
- [ ] 实现序列化/反序列化（JSON）

#### 1.3 工具模块
- [ ] 实现 GeometryUtils（几何计算）
- [ ] 实现 NetworkUtils（网络工具，带重试机制）
- [ ] 实现基础配置管理

**依赖关系**：无

**验证标准**：
- 单元测试通过
- 基础数据结构可以正确创建和序列化
- 网络请求工具可以成功发送请求并处理响应

---

### 阶段 2：核心转换引擎（第 3-6 周）

**优先级：最高**

#### 2.1 EasyEDA 模块
- [ ] 实现 EasyedaApi（API 客户端）
  - 实现 HTTP GET 请求
  - 实现重试机制（3次重试，指数退避）
  - 实现 JSON 响应解析
- [ ] 实现 EasyedaImporter（数据导入器）
  - 实现符号数据导入
  - 实现封装数据导入
  - 实现引脚数据导入
- [ ] 实现 JLCDatasheet（数据手册下载）
- [ ] 实现 SvgPathParser（SVG 路径解析）
- [ ] 实现 ParametersEasyeda（参数定义）

#### 2.2 KiCad 导出模块
- [ ] 实现 ExporterSymbol（符号导出器）
  - 实现符号数据转换
  - 实现 .kicad_sym 文件生成
  - 支持 KiCad 5.x 和 6.x 版本
- [ ] 实现 ExporterFootprint（封装导出器）
  - 实现封装数据转换
  - 实现 .kicad_mod 文件生成
  - 实现焊盘、丝印、阻焊等元素
- [ ] 实现 Exporter3DModel（3D 模型导出器）
  - 实现 3D 模型下载
  - 实现模型格式转换
  - 实现 .step 文件生成
- [ ] 实现 ParametersKicadSymbol 和 ParametersKicadFootprint

**依赖关系**：
- EasyEDA 模块依赖：工具模块、基础数据模型
- KiCad 导出模块依赖：EasyEDA 模块、基础数据模型

**验证标准**：
- 能够从 EasyEDA API 获取元件数据
- 能够将元件数据转换为 KiCad 格式
- 生成的文件可以被 KiCad 正确导入
- 单元测试覆盖率 > 80%

---

### 阶段 3：UI 模块开发（第 7-10 周） 已完成

**优先级：高**

#### 3.1 QML 界面开发 
- [x] 实现主界面（Main.qml）
  - 卡片式布局
  - 响应式设计
  - 现代化样式
- [x] 实现元件输入组件（ComponentInput.qml）
  - 输入框
  - 粘贴按钮
  - 添加按钮
- [x] 实现 BOM 导入组件（BOMImport.qml）
  - 文件选择
  - Excel/CSV 解析
- [x] 实现导出选项组件（ExportOptions.qml）
  - 符号/封装/3D 模型选择
  - KiCad 版本选择
- [x] 实现进度条组件（ProgressBar.qml）
  - 实时进度显示
  - 并行处理状态
- [x] 实现结果展示组件（ConversionResults.qml）
  - 成功/失败列表
  - 详细错误信息

#### 3.2 C++ 控制器 
- [x] 实现 MainController  (已迁移到 ViewModel)
  - 管理应用程序状态
  - 协调各个组件
- [x] 实现 ExportController  (已迁移到 ExportService)
  - 管理导出流程
  - 处理用户输入
- [x] 实现 ConfigController  (已迁移到 ConfigService)
  - 管理配置文件
  - 保存/加载设置

#### 3.3 UI 工具 
- [x] 实现 BOMParser（BOM 解析器）
- [x] 实现 ClipboardProcessor（剪贴板处理器）
- [x] 实现 ComponentValidator（元件验证器）
- [x] 实现 ConfigManager（配置管理器）

**依赖关系**：
- QML 界面依赖：C++ 控制器
- C++ 控制器依赖：核心转换引擎、UI 工具
- UI 工具依赖：基础数据模型

**验证标准**：
- [x] UI 可以正常显示和交互
- [x] 用户可以输入元件编号并添加到列表
- [x] 可以导入 BOM 文件
- [x] 可以配置导出选项
- [x] 可以启动导出并查看进度

---

### 阶段 4：工作线程和并发处理（第 11-12 周）

**优先级：高**

#### 4.1 工作线程实现
- [ ] 实现 ExportWorker（导出工作线程）
  - 使用 QThreadPool
  - 实现并行处理
  - 实现进度报告
- [ ] 实现 NetworkWorker（网络请求工作线程）
  - 异步网络请求
  - 请求队列管理
  - 超时处理

#### 4.2 并发优化
- [ ] 实现线程池管理
- [ ] 实现任务队列
- [ ] 实现资源锁定（避免文件冲突）
- [ ] 实现错误恢复机制

**依赖关系**：
- 依赖：核心转换引擎、UI 模块

**验证标准**：
- 可以同时处理多个元件
- 进度实时更新
- 错误正确处理和报告
- 无内存泄漏

---

### 阶段 5：集成和测试（第 13-14 周）

**优先级：高**

#### 5.1 集成测试
- [ ] 端到端测试
  - 单个元件转换
  - 批量元件转换
  - BOM 文件导入和转换
- [ ] 性能测试
  - 转换速度对比（vs Python 版本）
  - 内存使用测试
  - 启动速度测试
- [ ] 兼容性测试
  - KiCad 5.x 兼容性
  - KiCad 6.x 兼容性
  - Windows 版本兼容性

#### 5.2 错误处理
- [ ] 网络错误处理
- [ ] 文件 I/O 错误处理
- [ ] 数据验证错误处理
- [ ] 用户输入错误处理

#### 5.3 日志系统
- [ ] 实现日志记录
- [ ] 实现日志级别控制
- [ ] 实现日志文件管理

**依赖关系**：
- 依赖：所有之前的模块

**验证标准**：
- 所有功能测试通过
- 性能优于 Python 版本（至少 20% 提升）
- 无严重 Bug
- 错误处理完善

---

### 阶段 6：打包和发布（第 15-16 周）

**优先级：中**

#### 6.1 打包配置
- [ ] 配置 CMake 打包选项
- [ ] 配置 Qt 依赖打包
- [ ] 配置资源文件打包
- [ ] 配置图标和元数据

#### 6.2 安装程序
- [ ] 创建 Windows 安装程序（NSIS 或 WiX）
- [ ] 配置安装路径
- [ ] 配置快捷方式
- [ ] 配置卸载程序

#### 6.3 发布流程
- [ ] 创建发布版本
- [ ] 生成可执行文件
- [ ] 测试可执行文件
- [ ] 准备发布说明

**依赖关系**：
- 依赖：集成测试完成

**验证标准**：
- 可执行文件可以在干净的系统上运行
- 安装程序正常工作
- 卸载程序正常工作

---

## 📝 详细实现指南

### 模块 1：EasyedaApi（网络 API 客户端）

#### Python 版本分析
python
class EasyedaApi:
    def __init__(self):
        self.headers = {...}
        self.session = create_session_with_retries()
    
    def get_info_from_easyeda_api(self, lcsc_id: str) -> dict:
        # 带重试机制的 HTTP GET 请求
        r = self.session.get(url=api_url, headers=self.headers, timeout=30)
        return r.json()


#### C++ 实现方案
cpp
// EasyedaApi.h
class EasyedaApi : public QObject {
    Q_OBJECT
public:
    explicit EasyedaApi(QObject *parent = nullptr);
    
    Q_INVOKABLE void fetchComponentInfo(const QString &lcscId);
    
signals:
    void fetchSuccess(const QJsonObject &data);
    void fetchError(const QString &errorMessage);
    
private:
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    int m_retryCount;
    
    void sendRequest(const QUrl &url);
    void handleResponse();
    void handleError();
};


#### 关键技术点
1. 使用 QNetworkAccessManager 替代 requests
2. 实现重试机制（使用定时器延迟重试）
3. 使用信号槽机制异步处理响应
4. JSON 解析使用 QJsonDocument

---

### 模块 2：ExporterSymbol（符号导出器）

#### Python 版本分析
python
class ExporterSymbolKicad:
    def __init__(self, ee_symbol, kicad_version):
        self.ee_symbol = ee_symbol
        self.kicad_version = kicad_version
    
    def export(self, output_path):
        # 转换符号数据
        # 生成 .kicad_sym 文件
        pass


#### C++ 实现方案
cpp
// ExporterSymbol.h
class ExporterSymbol : public QObject {
    Q_OBJECT
public:
    explicit ExporterSymbol(const SymbolData &symbolData, 
                          KicadVersion version, 
                          QObject *parent = nullptr);
    
    bool exportToFile(const QString &outputPath);
    
private:
    SymbolData m_symbolData;
    KicadVersion m_kicadVersion;
    
    QString generateKicadSymbolContent();
    QString convertPin(const PinData &pin);
    QString convertRectangle(const RectangleData &rect);
    // ... 其他转换方法
};


#### 关键技术点
1. 使用字符串拼接生成 KiCad 格式文件
2. 实现单位转换（像素 → mil/mm）
3. 实现坐标转换
4. 支持多个 KiCad 版本

---

### 模块 3：MainController（主控制器）

#### C++ 实现方案
cpp
// MainController.h
class MainController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList componentList READ componentList NOTIFY componentListChanged)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    
public:
    explicit MainController(QObject *parent = nullptr);
    
    Q_INVOKABLE void addComponent(const QString &componentId);
    Q_INVOKABLE void removeComponent(int index);
    Q_INVOKABLE void clearComponents();
    Q_INVOKABLE void startExport(const QString &outputPath, 
                                const QString &libName,
                                const QVariantMap &options);
    
signals:
    void componentListChanged();
    void isExportingChanged();
    void exportProgress(int current, int total, const QString &message);
    void exportFinished(int successCount, int failedCount);
    
private:
    QStringList m_componentList;
    bool m_isExporting;
    ExportWorker *m_exportWorker;
    
    void validateComponentId(const QString &id);
};


#### 关键技术点
1. 使用 Q_PROPERTY 暴露属性给 QML
2. 使用 Q_INVOKABLE 暴露方法给 QML
3. 使用信号槽机制与 QML 通信
4. 管理工作线程生命周期

---

## 🧪 测试策略

### 单元测试

**测试框架**：Google Test + Qt Test

**测试覆盖**：
- [ ] 所有工具函数
- [ ] 所有数据模型
- [ ] 所有转换逻辑
- [ ] 所有验证逻辑

**目标覆盖率**：> 80%

### 集成测试

**测试场景**：
1. 单个元件转换流程
2. 批量元件转换流程
3. BOM 文件导入和转换
4. 错误恢复场景
5. 并发处理场景

### 性能测试

**测试指标**：
- 转换速度（元件/秒）
- 内存使用（MB）
- 启动时间（秒）
- 响应时间（毫秒）

**对比基准**：Python 版本性能数据

### 兼容性测试

**测试环境**：
- Windows 10/11（x64）
- KiCad 5.x 和 6.x
- 不同网络环境

---

## 📦 打包和发布

### 打包工具

**Windows**：
- 使用 windeployqt 打包 Qt 依赖
- 使用 NSIS 或 WiX 创建安装程序

### 打包清单


EasyKiConverter/
 EasyKiConverter.exe        # 主程序
 Qt6Core.dll                # Qt 核心库
 Qt6Gui.dll                 # Qt GUI 库
 Qt6Qml.dll                 # Qt QML 库
 Qt6Quick.dll               # Qt Quick 库
 Qt6Network.dll             # Qt 网络库
 platforms/                 # 平台插件
    qwindows.dll
 resources/                 # 资源文件
    icons/
    styles/
 README.txt                 # 使用说明


### 发布流程

1. **构建 Release 版本**
   bash
   cmake --build build --config Release
   

2. **打包 Qt 依赖**
   bash
   windeployqt --release --no-translations build/appEasyKiconverter.exe
   

3. **创建安装程序**
   - 使用 NSIS 脚本
   - 配置安装路径、快捷方式、卸载程序

4. **测试安装程序**
   - 在干净的系统上测试
   - 验证所有功能正常

5. **发布**
   - 上传到 GitHub Releases
   - 更新文档
   - 发布公告

---

## 🎯 里程碑和时间表

| 阶段 | 任务 | 预计时间 | 里程碑 | 状态 |
|------|------|----------|--------|------|
| 阶段 1 | 基础架构搭建 | 2 周 | 基础框架完成 |  已完成 |
| 阶段 2 | 核心转换引擎 | 4 周 | 核心功能完成 |  已完成 |
| 阶段 3 | UI 模块开发 | 4 周 | UI 完成 |  已完成 |
| 阶段 4 | 工作线程和并发 | 2 周 | 并发处理完成 |  已完成 |
| 阶段 5 | 集成和测试 | 2 周 | 测试通过 |  已完成 |
| 阶段 6 | 打包和发布 | 2 周 | 发布版本 |  进行中 |

**总计**：约 16 周（4 个月）
**实际完成**：前5个阶段已完成（15周），第6阶段进行中

---

## ️ 风险和挑战

### 技术风险

1. **网络请求处理**
   - **风险**：Qt Network 与 requests 的行为差异
   - **缓解**：充分测试网络重试机制

2. **JSON 解析**
   - **风险**：复杂数据结构的解析
   - **缓解**：使用强类型数据模型

3. **多线程同步**
   - **风险**：线程安全问题
   - **缓解**：使用 Qt 的线程安全机制

### 性能风险

1. **内存管理**
   - **风险**：内存泄漏
   - **缓解**：使用智能指针，定期测试

2. **转换速度**
   - **风险**：性能不如预期
   - **缓解**：性能分析和优化

### 兼容性风险

1. **KiCad 版本兼容**
   - **风险**：不同版本的格式差异
   - **缓解**：充分测试各版本

2. **Windows 版本兼容**
   - **风险**：不同 Windows 版本的差异
   - **缓解**：在多个版本上测试

---

## 📚 参考资料

### 官方文档
- [Qt 6 文档](https://doc.qt.io/qt-6/)
- [Qt Quick 文档](https://doc.qt.io/qt-6/qtquick-index.html)
- [CMake 文档](https://cmake.org/documentation/)
- [KiCad 文档](https://docs.kicad.org/)

### Python 版本代码
- EasyKiConverter_QT/src/core/ - 核心转换逻辑
- EasyKiConverter_QT/src/ui/ - UI 实现

### 相关项目
- [uPesy/easyeda2kicad.py](https://github.com/uPesy/easyeda2kicad.py) - 原始 Python 实现

---

##  验收标准

### 功能验收
- [ ] 所有 Python 版本功能已实现
- [ ] 可以成功转换元件
- [ ] 可以批量处理元件
- [ ] 可以导入 BOM 文件
- [ ] 生成的文件可以被 KiCad 正确导入

### 性能验收
- [ ] 转换速度提升 ≥ 20%
- [ ] 启动时间 ≤ 2 秒
- [ ] 内存使用优化 ≥ 30%

### 质量验收
- [ ] 单元测试覆盖率 ≥ 80%
- [ ] 无严重 Bug
- [ ] 代码符合 Qt 编码规范

### 部署验收
- [ ] 可执行文件可以在干净系统运行
- [ ] 安装程序正常工作
- [ ] 文档完整

---

## 📝 附录

### A. Python 依赖映射表

| Python 库 | C++ 替代方案 | 备注 |
|-----------|-------------|------|
| requests | Qt Network (QNetworkAccessManager) | 原生异步支持 |
| json | Qt JSON (QJsonDocument) | 高效解析 |
| pandas | Qt Xlsx 或第三方库 | Excel 处理 |
| openpyxl | Qt Xlsx 或第三方库 | Excel 处理 |
| pydantic | 自定义验证类 | 编译时类型检查 |
| math | Qt Math (QVector2D, QVector3D) | 向量化计算 |
| logging | Qt Logging (QLoggingCategory) | 日志系统 |
| concurrent.futures | QThreadPool + QRunnable | 线程池 |

### B. 关键代码片段示例

详见各模块实现指南部分。

### C. 测试用例清单

详见测试策略部分。

---

**文档版本**：1.0
**最后更新**：2026-01-08
**维护者**：EasyKiConverter 团队