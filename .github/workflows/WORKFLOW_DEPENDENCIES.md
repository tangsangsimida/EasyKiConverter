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

### 自动化
- `issue-triage.yaml` - Issue 自动分类
- `label.yml` - PR 自动标签
- `pr-review.yml` - PR 自动审查

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
