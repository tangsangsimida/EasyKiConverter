# GitHub Labels 标签管理指南

本文档介绍项目的 GitHub Labels 标签体系及管理方式。

## 标签体系

项目采用中英文双语标签体系，方便中英文开发者共同协作。

### 类型标签 (Type Labels)

用于标识 Issue 或 PR 的类型。

| 标签 | 颜色 | 描述 |
|------|------|------|
| `bug / 缺陷` | 🔴 #d73a4a | Something isn't working / 功能异常 |
| `feature / 新功能` | 🟢 #0e8a16 | New feature request / 新功能请求 |
| `enhancement / 功能增强` | 🔵 #1d76db | Improve existing functionality / 改进现有功能 |
| `docs / 文档` | 📘 #0075ca | Documentation improvements / 文档改进 |
| `refactor / 重构` | 🟡 #e4e669 | Code refactoring / 代码重构 |
| `test / 测试` | 🟣 #d876e3 | Testing related / 测试相关 |
| `ci / 持续集成` | 🟠 #fda67a | CI/CD workflows / 持续集成工作流 |
| `build / 构建` | 🟢 #a6c28c | Build configuration / 构建配置 |
| `deps / 依赖` | 🟢 #2ae51d | Dependency updates / 依赖更新 |
| `performance / 性能` | 🟡 #fbca04 | Performance optimization / 性能优化 |

### 模块标签 (Module Labels)

用于标识代码涉及的模块。

| 标签 | 颜色 | 描述 |
|------|------|------|
| `core / 核心` | 🔴 #b60205 | Core conversion engine / 核心转换引擎 |
| `easyeda / 嘉立创` | 🟢 #0b641b | EasyEDA module / EasyEDA 模块 |
| `kicad / KiCad` | 🟠 #e3621f | KiCad export module / KiCad 导出模块 |
| `ui / 界面` | 💗 #f2347e | User interface / 用户界面 |
| `network / 网络` | 🔵 #1d76db | Network client / 网络客户端 |
| `services / 服务` | 🟤 #76611c | Service layer / 服务层 |
| `workers / 工作线程` | 🟣 #b5a4fa | Background workers / 后台工作线程 |
| `cli / 命令行` | ⚪ #ededed | Command line interface / 命令行工具 |
| `models / 数据模型` | 🔵 #0e28c6 | Data models / 数据模型 |
| `resources / 资源` | 💗 #fe81f4 | Icons, images, QML / 图标、图片、QML |

### 状态标签 (Status Labels)

用于标识 Issue 或 PR 的状态。

| 标签 | 颜色 | 描述 |
|------|------|------|
| `good first issue / 新手友好` | 💜 #7057ff | Good for newcomers / 适合新手参与 |
| `help wanted / 寻求帮助` | 🟢 #008672 | Extra attention is needed / 需要社区帮助 |
| `wontfix / 不修复` | ⚪ #ffffff | Will not be worked on / 不予修复 |
| `duplicate / 重复` | 🔘 #cfd3d7 | Already exists / 重复问题 |
| `invalid / 无效` | 🟡 #e4e669 | Not valid / 无效问题 |
| `question / 提问` | 🟣 #d876e3 | Further information needed / 需要更多信息 |

### 优先级标签 (Priority Labels)

用于标识 Issue 的优先级。

| 标签 | 颜色 | 描述 |
|------|------|------|
| `priority: high / 高优先级` | 🔴 #b60205 | High priority issue / 高优先级问题 |
| `priority: medium / 中优先级` | 🟡 #fbca04 | Medium priority / 中优先级 |
| `priority: low / 低优先级` | 🟢 #0e8a16 | Low priority / 低优先级 |

### 进度标签 (Progress Labels)

用于标识 Issue 或 PR 的处理进度。

| 标签 | 颜色 | 描述 |
|------|------|------|
| `status: in progress / 进行中` | 🔵 #1d76db | Currently being worked on / 正在处理 |
| `status: blocked / 阻塞` | 🔴 #d73a4a | Blocked by something / 被阻塞 |
| `status: ready for review / 待审核` | 💜 #7057ff | Ready for code review / 等待代码审核 |

## 标签使用规范

### Issue 标签

创建 Issue 时，应添加以下标签：

1. **类型标签**（必选）：标识 Issue 类型
   - Bug 报告：`bug / 缺陷`
   - 功能请求：`feature / 新功能` 或 `enhancement / 功能增强`
   - 文档问题：`docs / 文档`

2. **模块标签**（可选）：标识涉及的代码模块
   - 如 `core / 核心`、`ui / 界面` 等

3. **优先级标签**（可选）：标识优先级
   - 如 `priority: high / 高优先级`

4. **状态标签**（可选）：标识处理状态
   - 如 `status: in progress / 进行中`

### PR 标签

创建 PR 时，应添加以下标签：

1. **类型标签**（必选）：标识 PR 类型
   - Bug 修复：`bug / 缺陷`
   - 新功能：`feature / 新功能`
   - 重构：`refactor / 重构`
   - 文档：`docs / 文档`

2. **模块标签**（可选）：标识涉及的代码模块

3. **进度标签**（可选）：标识审核状态
   - 如 `status: ready for review / 待审核`

## 标签管理

### 配置文件

标签配置存储在以下文件中：

- `.github/labels.yml` - 标签定义文件
- `.github/sync-labels.sh` - 标签同步脚本

### 添加新标签

1. 编辑 `.github/labels.yml`，添加新标签定义：

```yaml
- name: "标签名 / 中文名"
  color: "hex颜色"
  description: "English description / 中文描述"
```

2. 运行同步脚本：

```bash
bash .github/sync-labels.sh
```

### 标签命名规范

标签名称格式：`english-name / 中文名称`

- 英文名称使用小写，单词用空格分隔
- 中文名称使用简洁的中文描述
- 中英文之间用 ` / ` 分隔（空格 + 斜杠 + 空格）

### 颜色选择规范

- 红色系 (`#d73a4a`, `#b60205`)：用于 bug、高优先级、阻塞状态
- 绿色系 (`#0e8a16`, `#0b641b`, `#2ae51d`)：用于新功能、模块标签
- 蓝色系 (`#1d76db`, `#0075ca`)：用于增强、文档、进行中状态
- 黄色系 (`#fbca04`, `#e4e669`)：用于中优先级、重构、无效
- 紫色系 (`#d876e3`, `#7057ff`)：用于测试、新手友好、待审核
- 橙色系 (`#fda67a`, `#e3621f`)：用于 CI、KiCad 模块
- 灰色系 (`#ededed`, `#cfd3d7`, `#ffffff`)：用于 CLI、重复、不修复

## 相关文件

- [标签配置](../../.github/labels.yml)
- [同步脚本](../../.github/sync-labels.sh)
