# 更新日志

本文档记录了 EasyKiConverter 每个版本的新增、修复和更改内容。

## [3.0.0] - 2026-01-17

### 新增

#### 核心功能
- 完整的符号转换功能（EasyEDA 到 KiCad）
- 完整的封装生成功能（EasyEDA 到 KiCad）
- 完整的 3D 模型支持（WRL、STEP、OBJ 格式）
- 批量处理功能（支持多个元件同时转换）
- 智能提取功能（从剪贴板文本中提取元件编号）
- BOM 导入功能（CSV、Excel 格式）

#### 性能优化
- 并行转换支持（多线程并行处理）
- 两阶段导出策略（并行数据收集，串行数据导出）
- 状态机模式（异步数据收集）
- 网络请求优化（自动重试机制）
- GZIP 解压缩支持
- 内存优化（智能指针管理）

#### 用户界面
- 现代化 Qt Quick 界面
- 深色/浅色主题切换
- 卡片式布局系统
- 流畅动画效果
- 实时进度显示
- 响应式设计

#### 高级功能
- 完整的图层映射系统（50+ 图层）
- 多边形焊盘支持
- 椭圆弧计算（精确圆弧计算）
- 文本层处理（类型 "N" 和镜像文本）
- 覆盖文件功能
- 调试模式支持

#### 架构
- MVVM 架构实现
- Service 层（ComponentService、ExportService、ConfigService）
- ViewModel 层（ComponentListViewModel、ExportSettingsViewModel、ExportProgressViewModel、ThemeSettingsViewModel）
- 状态机模式（ComponentDataCollector）
- MainController 移除
- **多阶段流水线并行架构（ExportServicePipeline）**
  - **Fetch Stage**：I/O 密集型，32 线程并发下载
  - **Process Stage**：CPU 密集型，CPU 核心数线程并发处理
  - **Write Stage**：磁盘 I/O 密集型，8 线程并发写入
  - **线程安全的有界队列（BoundedThreadSafeQueue）**用于阶段间通信
  - **实时进度反馈**（三阶段进度条：抓取 30%、处理 50%、写入 20%）
  - **详细失败诊断**（精确识别失败阶段和原因）
  - **零拷贝解析优化**（QByteArrayView）
  - **HTTP/2 支持**（网络请求多路复用）

#### 测试
- 完整的测试框架
- 单元测试（8 个测试程序）
- 集成测试框架
- 性能测试框架
- **流水线架构测试**
  - BoundedThreadSafeQueue 并发测试
  - ExportServicePipeline 集成测试
  - 多阶段并发测试

#### 文档
- 完整的文档体系（14 个技术文档）
- 用户手册
- 开发者文档
- 架构文档
- 构建指南
- 贡献指南
- **ADR-002：流水线并行架构决策记录**

### 修复
- 修复封装解析 Type 判断错误
- 修复 3D Model UUID 遗漏问题
- 修复 Footprint BBox 不完整问题
- 修复图层映射错误
- 修复多边形焊盘处理缺失
- 修复椭圆弧计算不完整
- 修复文本层处理逻辑缺失
- 修复 3D 模型偏移参数计算错误
- 修复 3D 模型不显示问题
- 修复圆弧和实体区域导出不完整
- 修复非 ASCII 文本处理问题
- 修复元件 ID 验证规则过于严格
- 修复与 Python 版本 V6 一致性问题

### 更改
- 从 MVC 架构重构为 MVVM 架构
- 移除 MainController
- 移除丝印层复制逻辑
- 移除特殊层圆形处理
- 优化网络请求流程
- 优化错误处理机制
- 优化配置管理

### 移除
- 移除 MainController
- 移除丝印层复制逻辑
- 移除特殊层圆形处理（层 100 和 101）

## [2.0.0] - 2025-12-15

### 新增
- 基础符号转换功能
- 基础封装生成功能
- 基础 3D 模型支持
- 网络请求功能
- 基本用户界面
- 配置管理功能

### 修复
- 修复基本网络请求错误
- 修复基本文件读写问题

## [1.0.0] - 2025-10-01

### 新增
- 项目初始化
- 基础架构搭建
- CMake 构建系统
- Qt Quick 应用框架

## 版本说明

本项目遵循语义化版本规范：MAJOR.MINOR.PATCH

- **MAJOR**：不兼容的 API 更改
- **MINOR**：新功能（向后兼容）
- **PATCH**：Bug 修复（向后兼容）

## 更新类型说明

- **新增**：新功能
- **修复**：Bug 修复
- **更改**：现有功能的更改
- **移除**：移除的功能
- **安全**：安全相关的修复
- **弃用**：即将移除的功能

## 如何贡献

如果您想参与项目开发，请参考[贡献指南](docs/CONTRIBUTING.md)。

## 反馈

如果您有任何问题或建议，请在 [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter_QT/issues) 上提交。