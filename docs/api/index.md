# API 参考

本节包含 EasyKiConverter 的自动生成 API 参考文档，由 [Doxygen](https://www.doxygen.nl/) 从源代码注释生成。

## 访问 API 文档

API 文档由 CI 工作流自动生成并部署到 GitHub Pages：

- **[C++ API 参考](https://tangsangsimida.github.io/EasyKiConverter_QT/api/html/index.html)** - 完整的类、函数、信号槽参考

## 本地生成

如需在本地生成 API 文档：

```bash
# 安装 Doxygen 和 Graphviz
# Windows: choco install doxygen graphviz
# macOS: brew install doxygen graphviz
# Linux: sudo apt install doxygen graphviz

# 生成文档
doxygen Doxyfile

# 查看文档
# 输出目录: docs/api/html/index.html
```

## 文档结构

生成的 API 文档包含：

- **类索引** - 所有公开类的列表
- **命名空间成员** - 按命名空间组织的函数和类
- **类层次结构** - 继承关系图
- **协作图** - 类之间的依赖关系