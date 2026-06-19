# GitHub Actions 工作流依赖关系

## 工作流分类

### 持续集成 (CI)
- `build.yml` - 构建和测试 (所有平台)
- `actionlint.yml` - GitHub Actions 工作流检查
- `clang-format.yml` - 代码格式检查
- `security.yml` - 安全检查

### 持续部署 (CD)
- `pack-linux.yml` - Linux 打包
- `pack-macos.yml` - macOS 打包
- `pack-windows.yml` - Windows 打包
- `release.yml` - 汇总各平台打包产物并发布 GitHub Release
- `deploy-docs.yml` - 文档部署

### 自动化与辅助
- `issue-triage.yaml` - Issue 自动分类
- `label.yml` - PR 自动标签
- `pr-review.yml` - PR 自动审查
- `.github/actions/setup-env` - 环境配置 Composite Action
- `.github/actions/get-version` - 版本号提取 Composite Action (新增)

## 全局策略

### 并发控制 (Concurrency)
主要 CI/CD 工作流配置了并发控制组 (`group: ${{ github.workflow }}-${{ github.ref }}`)。构建和打包工作流会在同一分支或标签有新运行时取消旧运行以节省资源；`release.yml` 不自动取消，避免发布过程被后续手动重跑中断。

### 权限控制 (Permissions)
遵循最小权限原则，每个工作流显式声明所需的 `GITHUB_TOKEN` 权限（例如 `contents: read` 或 `contents: write`）。

## 触发条件

| 工作流 | Push | PR | Tag | Schedule | Manual |
|--------|------|-----|-----|----------|--------|
| build.yml | ✓ | ✓ | ✗ | ✗ | ✓ |
| actionlint.yml | ✓ | ✓ | ✗ | ✗ | ✓ |
| clang-format.yml | ✓ | ✓ | ✗ | ✗ | ✓ |
| security.yml | ✗ | ✓ | ✗ | ✓ | ✗ |
| pack-linux.yml | ✗ | ✗ | ✓ | ✗ | ✓ |
| pack-macos.yml | ✗ | ✗ | ✓ | ✗ | ✓ |
| pack-windows.yml | ✗ | ✗ | ✓ | ✗ | ✓ |
| release.yml | ✗ | ✗ | ✓ | ✗ | ✓ |
| deploy-docs.yml | ✓ | ✗ | ✗ | ✗ | ✓ |

## 超时配置

| 工作流 | 超时时间 |
|--------|----------|
| build.yml (Linux) | 60 分钟 |
| build.yml (Windows) | 90 分钟 |
| build.yml (macOS) | 75 分钟 |
| release.yml | 180 分钟 |
| pack-windows.yml | 120 分钟 |
