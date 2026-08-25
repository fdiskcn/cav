# Mayo + NVIDIA ovrtx

[Mayo](https://github.com/fougue/mayo) CAD 查看器。可选把视口从 OCCT OpenGL 换成 [NVIDIA ovrtx](https://github.com/nvidia-omniverse/ovrtx)。导入、文档树、测量和相机仍由 Mayo / OpenCascade 负责；**NVIDIA RTX** 环境下屏幕像素来自 ovrtx 的 `LdrColor` AOV。

## 环境

先选环境，再配置、编译、测试、打包。环境 id 同时表示操作系统和视口后端。

| 环境 | 平台 | 视口 |
|------|------|------|
| `macos-gl` | macOS (arm64 / x64) | OCCT OpenGL（ovrtx 无 macOS 包） |
| `linux-gl` | Linux (x64 / arm64) | OCCT OpenGL |
| `linux-nvidia-rtx` | Linux (x64 / arm64) | NVIDIA ovrtx |
| `windows-gl` | Windows x64 | OCCT OpenGL |
| `windows-nvidia-rtx` | Windows x64 | NVIDIA ovrtx |
| `usd-tests` | 任意 | 无（仅 USDA 管线单测，不打包） |

NVIDIA RTX 环境在没有 GPU / CUDA 时仍可配置、编译、跑单测和打包。运行时视口显示错误文案，进程不崩溃。出图需要 NVIDIA 驱动。QEMU 与 Apple GPU 不能代替 RTX 像素。

```bash
python3 scripts/mayo-env.py list
python3 scripts/mayo-env.py suggest
python3 scripts/mayo-env.py all macos-gl
```

Windows：

```bat
python scripts\mayo-env.py all windows-gl
python scripts\mayo-env.py all windows-nvidia-rtx
```

Linux 环境可在 macOS / Windows 上通过 Docker 构建测试（`docker/linux-env.Dockerfile`）。

产物：

- 安装目录：`dist/<环境>/`
- 归档：`dist/mayo-<版本>-<os>-<arch>-<opengl|nvidia-rtx>.tar.gz`（Windows 为 `.zip`）
- 包内 `share/mayo/MAYO_ENVIRONMENT.txt` 记录当时的平台与显卡选型

CMake Preset（CMake ≥ 3.21）：

```bash
cmake --list-presets
cmake --preset macos-gl
cmake --build --preset macos-gl
```

手动开关：

```bash
cmake -S . -B build -DMayo_GpuBackend=opengl
cmake -S . -B build -DMayo_GpuBackend=nvidia-rtx   # Windows / Linux x64 或 arm64
```

`Mayo_UseOvrtx` 仍可用；与 `Mayo_GpuBackend` 同时给出时以 GpuBackend 为准。

依赖：CMake ≥ 3.16（preset 需要 ≥ 3.21）、C++17、Qt 5.12+ 或 Qt 6、OpenCascade 7.x。nvidia-rtx 会从 GitHub Releases 拉取 ovrtx / ovstage（`cmake/_deps/`，已 gitignore）。

## 测试

```bash
python3 scripts/mayo-env.py test usd-tests
python3 scripts/mayo-env.py platform
python3 scripts/mayo-env.py platform --only linux-nvidia-rtx
```

`platform` 构建并测试本机能跑的环境。六个环境的 CI 见 [ci_environments.yml](.github/workflows/ci_environments.yml)，不要求 GPU。无 GPU 的 nvidia-rtx 作业验证编译、`test-mayo`、`test-ovrtx-usd` 和 `test-ovrtx-engine`（initialize 失败但不崩溃）。Docker 默认设置 `MAYO_SKIP_GL_TESTS=1`，跳过依赖硬件 OpenGL 的回归。

RTX 出图：在装有 NVIDIA 驱动的 Windows / Linux 上打开 `tests/inputs/cube.step`。

## 架构

```
STEP/IGES/BREP/… ──► Mayo IO + OCCT 网格
                         │
                         ▼
              USDA（Y-up，毫米） ──► ovstage ──► ovrtx step   ← 仅 nvidia-rtx
                         │                         │
              V3d_View 相机矩阵 ──► ovrtx_set_xform_mat
                                                   ▼
                                        QImage 显示在视口
```

- `Mayo_GpuBackend=nvidia-rtx` 时，`initGui()` 使用 `WidgetOvrtxView`；`opengl` 时使用原版 OCCT OpenGL 视口。
- RTX 路径下 OCCT 仍创建离屏 `V3d_View` 做相机和选择，不把 OpenGL 画到屏幕上。
- 坐标：OCCT Z-up `(x, y, z)` → USD Y-up `(x, z, -y)`。

## 已知限制

相对原版 OpenGL 视口，RTX 路径目前不把测量标注、网格、裁剪平面手柄、ViewCube 编进 USD。选择高亮与装配爆炸会同步到网格颜色 / 变换。

## 许可

- Mayo 源码：BSD-2-Clause（见 `LICENSE.txt`）
- `src/ovrtx/*`、`src/app/widget_ovrtx_view.*`：同样按 BSD-2-Clause 贡献
- `cmake/ovrtx.cmake`、`cmake/ovstage.cmake`：复制自 ovrtx C 示例，属 NVIDIA 专有许可，仅用于拉取 SDK
- 运行时 ovrtx / ovstage 二进制包：NVIDIA 专有，使用前请遵守 NVIDIA 许可

## 上游

- Mayo: https://github.com/fougue/mayo
- ovrtx: https://github.com/nvidia-omniverse/ovrtx
