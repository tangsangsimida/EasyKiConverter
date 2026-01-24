# GitHub Actions 工作流依赖关系

## 工作流分类

### 持续集成 (CI)
- `build.yml` - 构建和测试 (所有平台)
- `clang-format.yml` - 代码格式检查
- `coverage.yml` - 代码覆盖率
- `performance.yml` - 性能测试
- `security.yml` - 安全检查

### 持续部署 (CD)
- `pack-linux.yml` - Linux 打包
- `pack-macos.yml` - macOS 打包
- `pack-windows.yml` - Windows 打包
- `deploy-docs.yml` - 文档部署

### 自动化与辅助
- `issue-triage.yaml` - Issue 自动分类
- `label.yml` - PR 自动标签
- `pr-review.yml` - PR 自动审查
- `.github/actions/setup-env` - 环境配置 Composite Action
- `.github/actions/get-version` - 版本号提取 Composite Action (新增)

## 全局策略

### 并发控制 (Concurrency)
所有 CI/CD 工作流均配置了并发控制组 (`group: ${{ github.workflow }}-${{ github.ref }}`)。当同一分支有新的 Push 时，旧的运行中的构建会自动取消，以节省资源。

### 权限控制 (Permissions)
遵循最小权限原则，每个工作流显式声明所需的 `GITHUB_TOKEN` 权限（例如 `contents: read` 或 `contents: write`）。

## 触发条件

| 工作流 | Push | PR | Tag | Schedule | Manual |
|--------|------|-----|-----|----------|--------|
| build.yml | ✓ | ✓ | ✗ | ✗ | ✓ |
| clang-format.yml | ✓ | ✓ | ✗ | ✗ | ✓ |
| coverage.yml | ✓ | ✓ | ✗ | ✗ | ✓ |
| performance.yml | ✓ | ✓ | ✗ | ✗ | ✓ |
| security.yml | ✗ | ✓ | ✗ | ✓ | ✗ |
| pack-linux.yml | ✗ | ✗ | ✓ | ✗ | ✓ |
| pack-macos.yml | ✗ | ✗ | ✓ | ✗ | ✓ |
| pack-windows.yml | ✗ | ✗ | ✓ | ✗ | ✓ |
| deploy-docs.yml | ✓ | ✗ | ✗ | ✗ | ✓ |

## 超时配置

| 工作流 | 超时时间 |
|--------|----------|
| build.yml (Linux) | 60 分钟 |
| build.yml (Windows) | 90 分钟 |
| build.yml (macOS) | 75 分钟 |
| pack-windows.yml | 120 分钟 |
| coverage.yml | 45 分钟 |
| performance.yml | 60 分钟 |