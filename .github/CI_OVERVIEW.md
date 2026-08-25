# 持续集成

| 工作流 | 说明 |
|--------|------|
| [Environments](https://github.com/fdiskcn/cav/actions/workflows/ci_environments.yml) | `macos-gl` / `linux-gl` / `linux-nvidia-rtx` / `windows-gl` / `windows-nvidia-rtx` / `usd-tests`，不要求 GPU |
| `ci_linux_*.yml`、`ci_macos*.yml`、`ci_windows.yml` | 上游 Mayo 发行矩阵，固定 `Mayo_GpuBackend=opengl` |
| `sonar.yml` | Sonar 扫描 |

nvidia-rtx 作业只验证编译与单测：无 GPU 时 `test-ovrtx-engine` 允许 initialize 失败，不允许崩溃。
