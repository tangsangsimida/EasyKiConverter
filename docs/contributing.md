# 🤝 贡献指南

[English Version](contributing_en.md)

欢迎为 EasyKiConverter 项目做贡献！以下是一些基本的贡献指南：

## 📋 贡献流程

1. **Fork 仓库**
   - 点击 GitHub 上的 Fork 按钮创建项目的副本。

2. **克隆仓库**
```bash
git clone <your-fork-url>
cd EasyKiConverter
```

3. **创建分支**
```bash
git checkout -b feature/your-feature-name
```

4. **构建和测试**
```bash
mkdir build
cd build
cmake ..
make
ctest
```

5. **提交更改**
```bash
git add .
git commit -m "描述您的更改"
git push origin feature/your-feature-name
```

6. **创建 Pull Request**
   - 在 GitHub 上打开您的 fork 页面。
   - 点击 "New Pull Request" 按钮。
   - 描述您的更改并提交 PR。

## 🧰 开发规范

- 遵循项目代码风格指南
- 使用 CMake 构建系统
- 确保所有新增功能都经过单元测试
- 提交前运行 `ctest` 确保所有测试通过

## 📚 文档贡献

- 更新 [README.md](../README.md) 和 [README_en.md](../README_en.md)
- 修改 [开发指南](development_guide.md) 和 [贡献指南](contributing.md)
- 确保文档内容清晰、准确且与代码一致

## 🛠️ 工具使用

- 使用 Visual Studio Code 或 CLion 进行开发
- 配置 CMake Tools 扩展以简化构建过程
- 使用 GDB 或 LLDB 进行调试

## 📂 目录结构
参照 [项目结构](project_structure.md) 文档