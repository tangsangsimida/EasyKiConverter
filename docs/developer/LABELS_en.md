# GitHub Labels Management Guide

This document describes the project's GitHub Labels system and management approach.

## Label System

The project uses a bilingual (Chinese/English) label system to facilitate collaboration between Chinese and English-speaking developers.

### Type Labels

Used to identify the type of Issue or PR.

| Label | Color | Description |
|-------|-------|-------------|
| `bug / 缺陷` | 🔴 #d73a4a | Something isn't working |
| `feature / 新功能` | 🟢 #0e8a16 | New feature request |
| `enhancement / 功能增强` | 🔵 #1d76db | Improve existing functionality |
| `docs / 文档` | 📘 #0075ca | Documentation improvements |
| `refactor / 重构` | 🟡 #e4e669 | Code refactoring |
| `test / 测试` | 🟣 #d876e3 | Testing related |
| `ci / 持续集成` | 🟠 #fda67a | CI/CD workflows |
| `build / 构建` | 🟢 #a6c28c | Build configuration |
| `deps / 依赖` | 🟢 #2ae51d | Dependency updates |
| `performance / 性能` | 🟡 #fbca04 | Performance optimization |

### Module Labels

Used to identify the code module involved.

| Label | Color | Description |
|-------|-------|-------------|
| `core / 核心` | 🔴 #b60205 | Core conversion engine |
| `easyeda / 嘉立创` | 🟢 #0b641b | EasyEDA module |
| `kicad / KiCad` | 🟠 #e3621f | KiCad export module |
| `ui / 界面` | 💗 #f2347e | User interface |
| `network / 网络` | 🔵 #1d76db | Network client |
| `services / 服务` | 🟤 #76611c | Service layer |
| `workers / 工作线程` | 🟣 #b5a4fa | Background workers |
| `cli / 命令行` | ⚪ #ededed | Command line interface |
| `models / 数据模型` | 🔵 #0e28c6 | Data models |
| `resources / 资源` | 💗 #fe81f4 | Icons, images, QML |

### Status Labels

Used to identify the status of an Issue or PR.

| Label | Color | Description |
|-------|-------|-------------|
| `good first issue / 新手友好` | 💜 #7057ff | Good for newcomers |
| `help wanted / 寻求帮助` | 🟢 #008672 | Extra attention is needed |
| `wontfix / 不修复` | ⚪ #ffffff | Will not be worked on |
| `duplicate / 重复` | 🔘 #cfd3d7 | Already exists |
| `invalid / 无效` | 🟡 #e4e669 | Not valid |
| `question / 提问` | 🟣 #d876e3 | Further information needed |

### Priority Labels

Used to identify the priority of an Issue.

| Label | Color | Description |
|-------|-------|-------------|
| `priority: high / 高优先级` | 🔴 #b60205 | High priority issue |
| `priority: medium / 中优先级` | 🟡 #fbca04 | Medium priority |
| `priority: low / 低优先级` | 🟢 #0e8a16 | Low priority |

### Progress Labels

Used to identify the progress of an Issue or PR.

| Label | Color | Description |
|-------|-------|-------------|
| `status: in progress / 进行中` | 🔵 #1d76db | Currently being worked on |
| `status: blocked / 阻塞` | 🔴 #d73a4a | Blocked by something |
| `status: ready for review / 待审核` | 💜 #7057ff | Ready for code review |

## Label Usage Guidelines

### Issue Labels

When creating an Issue, add the following labels:

1. **Type Label** (required): Identify the Issue type
   - Bug report: `bug / 缺陷`
   - Feature request: `feature / 新功能` or `enhancement / 功能增强`
   - Documentation: `docs / 文档`

2. **Module Label** (optional): Identify the code module
   - Such as `core / 核心`, `ui / 界面`, etc.

3. **Priority Label** (optional): Identify priority
   - Such as `priority: high / 高优先级`

4. **Status Label** (optional): Identify progress
   - Such as `status: in progress / 进行中`

### PR Labels

When creating a PR, add the following labels:

1. **Type Label** (required): Identify the PR type
   - Bug fix: `bug / 缺陷`
   - New feature: `feature / 新功能`
   - Refactor: `refactor / 重构`
   - Documentation: `docs / 文档`

2. **Module Label** (optional): Identify the code module

3. **Progress Label** (optional): Identify review status
   - Such as `status: ready for review / 待审核`

## Label Management

### Configuration Files

Label configurations are stored in the following files:

- `.github/labels.yml` - Label definition file
- `.github/sync-labels.sh` - Label sync script

### Adding New Labels

1. Edit `.github/labels.yml` and add the new label definition:

```yaml
- name: "english-name / 中文名"
  color: "hex-color"
  description: "English description / 中文描述"
```

2. Run the sync script:

```bash
bash .github/sync-labels.sh
```

### Label Naming Convention

Label name format: `english-name / 中文名称`

- English name uses lowercase, words separated by spaces
- Chinese name uses concise Chinese description
- Chinese and English separated by ` / ` (space + slash + space)

### Color Selection Guidelines

- Red shades (`#d73a4a`, `#b60205`): For bugs, high priority, blocked status
- Green shades (`#0e8a16`, `#0b641b`, `#2ae51d`): For new features, module labels
- Blue shades (`#1d76db`, `#0075ca`): For enhancement, documentation, in-progress status
- Yellow shades (`#fbca04`, `#e4e669`): For medium priority, refactor, invalid
- Purple shades (`#d876e3`, `#7057ff`): For testing, good first issue, ready for review
- Orange shades (`#fda67a`, `#e3621f`): For CI, KiCad module
- Gray shades (`#ededed`, `#cfd3d7`, `#ffffff`): For CLI, duplicate, wontfix

## Related Files

- [Label Configuration](../../.github/labels.yml)
- [Sync Script](../../.github/sync-labels.sh)
