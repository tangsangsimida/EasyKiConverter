# 项目路线图

本文档展示项目未来的发展方向，计划在下几个版本中实现哪些主要功能。

## 当前状态

- **当前版本**: 3.1.10
- **开发状态**: 稳定优化阶段，跨平台支持完善
- **完成进度**: 约 99%（核心功能已实现，架构重构完成，测试框架已集成，跨平台打包完善）

## 版本规划

### v3.0.0 - 核心功能完善（当前版本）

**目标**: 完善核心功能，提升用户体验

**核心架构**:
- [x] 多阶段流水线并行架构实现
- [x] 线程安全的有界队列
- [x] 实时进度反馈

**导出功能增强**:
- [ ] 导出成功后快速打开目标文件夹
- [x] 网络请求重试机制（v3.0.2 已实现）
- [x] 弱网容错改进（超时重试、统一退避策略、NetworkWorker 超时保护）- v3.0.5 已完成
- [x] 3D模型路径导出选项（相对路径/绝对路径）— v3.1.8 已完成
  - [x] 相对路径：使用 KiCad 工程路径 + 3D 模型路径
  - [x] 绝对路径：使用导出路径 + 3D 模型路径
- [x] 导出进度条设计（v3.0.2 已实现）
- [x] 导出统计设计（v3.0.2 已实现）
- [ ] 导出预览功能（封装、符号、3D模型、手册PDF）

**导出选项优化**:
- [x] 覆盖导出和追加导出选项
- [x] 导出模式UI设计优化（更明显的模式选择）
- [x] 库描述导出选项（KiCad 库描述参数）— v3.1.7 已完成
- [ ] 封装属性增强（简介、双3D模型地址等）

**数据解析改进**:
- [x] 封装符号解析器和导出器完善
  - [x] 封装符号线宽解析
  - [ ] 其他属性解析优化
- [ ] BOM 表解析功能测试
- [ ] 特殊元器件导出问题修复（如 C7420375 位置偏移）

**用户界面改进**:
- [ ] 元器件列表搜索功能
- [~] 多语言支持（根据系统语言切换）— CLI 国际化已在 v3.1.9 实现，GUI 翻译文件已配置
- [ ] UI 性能优化和响应速度提升

**开发工具和流程**:
- [x] GitHub 工作流配置
  - [x] 代码法律安全自动化审查
  - [x] 自动化测试（QtTest 框架集成）- v3.0.5 已完成
  - [x] 自动化编译和打包
  - [x] 文档检查 - v3.0.5 已完成
  - [ ] 版本自动发布
- [x] 贡献指南和项目文档完善
- [x] 版本管理工具（manage_version.py）- v3.0.5 已完成

### v3.2.0 - 中间表示层架构重构（规划中）

**目标**: 引入通用中间表示层 (IR)，解耦数据源与导出器，为多数据源接入奠定基础

**决策文档**: [ADR 012: IR 架构重构](adr/012-intermediate-representation-refactor.md)

**阶段 1: IR 类型和几何解析**（1 周，低风险）
- [ ] 创建 `src/core/ir/` 目录和 `IRTypes.h`（PadShape/LayerType/PinElectricalType 等枚举）
- [ ] 创建 `SymbolIR.h`（通用符号数据，QList\<QPointF\> 等已解析几何）
- [ ] 创建 `FootprintIR.h`（通用封装数据，枚举化层 ID 和焊盘形状）
- [ ] 创建 `ComponentIR.h`、`Model3DIR.h`
- [ ] 创建 EasyEDA 层 ID/焊盘形状/引脚类型映射表

**阶段 2: Importer 迁移**（1 周，中风险）
- [ ] 迁移 `EasyedaSymbolImporter` 输出为 `SymbolComponentIR`
- [ ] 迁移 `EasyedaFootprintImporter` 输出为 `FootprintComponentIR`
- [ ] 几何字符串解析从导出器提取到 Importer
- [ ] 移除 `src/models/` 旧结构体（SymbolData/FootprintData 等）
- [ ] Golden file 全量回归验证

**阶段 3: 导出器适配**（1 周，中风险）
- [ ] 导出器接口改为消费 IR
- [ ] ViewModel/Service/CLI/BOM 路径适配
- [ ] 缓存序列化格式迁移
- [ ] 端到端测试验证

**代码质量改进**（见 [代码规模分析](CODE_SIZE_ANALYSIS.md)）:
- [ ] QML 组件拆分（7 个过长文件，最大 1,128 行）
- [ ] 服务层拆分（ComponentCacheService 1,509 行 / ComponentService 1,184 行）
- [ ] ViewModel 拆分（ComponentListViewModel 1,385 行）

## 如何参与

如果您想参与项目开发，请：

1. 查看[贡献指南](../developer/CONTRIBUTING.md)
2. 选择您感兴趣的任务
3. 创建 Issue 讨论您的想法
4. 提交 Pull Request

## 反馈和建议

我们欢迎您的反馈和建议：

- 在 [GitHub Issues](https://github.com/tangsangsimida/EasyKiConverter/issues) 提交问题
- 在 [GitHub Discussions](https://github.com/tangsangsimida/EasyKiConverter/discussions) 参与讨论
- 通过邮件联系项目维护者

## 更新日志

本项目路线图会定期更新，请关注最新动态。

## 免责声明

本路线图仅供参考，实际开发计划可能会根据实际情况进行调整。
