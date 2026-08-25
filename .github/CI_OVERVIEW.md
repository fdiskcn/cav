# 持续集成

| 工作流 | 说明 |
|--------|------|
| [Environments](https://github.com/fdiskcn/cav/actions/workflows/ci_environments.yml) | `macos-gl` / `linux-gl` / `linux-nvidia-rtx` / `windows-gl` / `windows-nvidia-rtx` / `usd-tests`，不要求 GPU |
| `ci_linux_*.yml`、`ci_macos*.yml`、`ci_windows.yml` | 上游 Mayo 发行矩阵。本仓库默认跳过（避免占满 runner）；`fougue/mayo` 或仓库变量 `MAYO_UPSTREAM_CI=true` 时运行 |
| `sonar.yml` | Sonar 扫描 |

nvidia-rtx 作业只验证编译与单测：无 GPU 时 `test-ovrtx-engine` 允许 initialize 失败，不允许崩溃。
