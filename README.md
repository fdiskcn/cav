# Mayo + NVIDIA ovrtx

本仓库在 [Mayo](https://github.com/fougue/mayo) 之上，把视口里的 **OCCT OpenGL 渲染器**换成 [NVIDIA ovrtx](https://github.com/nvidia-omniverse/ovrtx)（Omniverse RTX 嵌入式 SDK）。CAD 导入、文档树、测量、以及基于 `V3d_View` 的相机导航仍由 Mayo / OpenCascade 负责；屏幕上的像素来自 ovrtx 的 `LdrColor` AOV。

## 架构

```
STEP/IGES/BREP/… ──► Mayo IO + OCCT 网格
                         │
                         ▼
              USDA（Y-up，毫米） ──► ovstage ──► ovrtx step
                         │                         │
              V3d_View 相机矩阵 ──► ovrtx_set_xform_mat
                                                   ▼
                                        QImage 显示在视口
```

- 接入点是 `IWidgetOccView`。`Mayo_UseOvrtx=ON` 时，`initGui()` 把视口工厂设为 `WidgetOvrtxView::create`，不再创建 `QOpenGLWidgetOccView`。
- OCCT 仍会创建 `Graphic3d_GraphicDriver` 和 `V3d_View`（挂在离屏 `Aspect_NeutralWindow` 上），用于相机、选择和测量；**不会把 OCCT OpenGL 画到屏幕上**。
- 几何 / 显隐 / 选择 / 正交缩放变化时重写 USDA 并 `ovstage_population_open_usd_from_string`；仅相机运动时走 `ovrtx_set_xform_mat`。
- 坐标：OCCT Z-up `(x, y, z)` → USD Y-up `(x, z, -y)`。
- 没有 RTX GPU 或 SDK 运行时失败时，视口显示错误文案，进程不崩溃。
- NVIDIA ovrtx 目前只提供 **Windows / Linux x86_64 / Linux aarch64** 包。macOS 等平台会自动回退到 OCCT OpenGL，应用仍可运行。

## 构建应用

依赖：

- CMake ≥ 3.16、C++17 编译器
- Qt 5.12+ 或 Qt 6（Widgets）
- OpenCascade 7.x
- **启用 RTX 视口时**：NVIDIA RTX GPU，以及可由 CMake 下载的 ovrtx / ovstage 二进制包

```bash
cmake -S . -B build -DMayo_UseOvrtx=ON
cmake --build build --target mayo
```

首次配置会从 GitHub Releases 拉取 ovrtx / ovstage SDK（体积较大，NVIDIA 专有许可）。包被放在 `cmake/_deps/`（已 gitignore）。

关闭 RTX 视口、回到原版 OCCT OpenGL：

```bash
cmake -S . -B build -DMayo_UseOvrtx=OFF
cmake --build build --target mayo
```

安装（可交付目录）：

```bash
cmake --install build --prefix /path/to/mayo-ovrtx
```

`mayo` / `mayo-conv` 会装到 `bin/`。在支持 ovrtx 的平台上，还会把 SDK 运行时装到 `bin/ovrtx/` 与 `bin/ovstage/`，与可执行文件并排，符合静态 loader 的 `${executable_dir}/ovrtx` 约定。

## 测试

### 不依赖 GPU 的 USD 管线

本环境没有 NVIDIA GPU / Qt / OCCT 时，仍可验证网格→USDA、look-at 矩阵、Z-up 转换和 RenderProduct 结构：

```bash
cmake -S tests/ovrtx -B build-ovrtx-usd -DCMAKE_CXX_COMPILER=g++
cmake --build build-ovrtx-usd
ctest --test-dir build-ovrtx-usd --output-on-failure
```

若默认 `c++` 是 clang 且链接时报找不到 `-lstdc++`，请像上面一样显式指定 `g++`。

完整 Mayo 工程在配置成功后也会生成 `test-ovrtx-usd` 目标。CI 工作流 `.github/workflows/ci_ovrtx_usd.yml` 只跑这一套测试。

### Mayo 原有单元测试

```bash
cmake -S . -B build -DMayo_BuildTests=ON -DMayo_UseOvrtx=OFF
cmake --build build --target test-mayo
ctest --test-dir build -R test-mayo --output-on-failure
```

上游 Linux / Windows / macOS CI 继续用 `Mayo_UseOvrtx=OFF` 构建，避免在无 GPU 的 runner 上下载专有 SDK。

### RTX 视口（需要 NVIDIA GPU）

在 Windows 或 Linux 工作站上：

1. 用 `Mayo_UseOvrtx=ON` 构建 `mayo`
2. 运行 `mayo`，打开 `tests/inputs/cube.step`（或任意 STEP/IGES/BREP）
3. 旋转 / 平移 / 缩放视口，确认画面来自 RTX 而不是 OCCT OpenGL
4. 无 GPU 时应看到错误文案而不是崩溃

## 已知限制

相对原版 Mayo OpenGL 视口，RTX 路径目前不把下列 AIS 叠加层编进 USD：测量标注、网格、裁剪平面手柄、ViewCube。选择高亮与装配爆炸会同步到网格颜色 / 变换。

## 许可

- Mayo 源码：BSD-2-Clause（见 `LICENSE.txt`）
- `src/ovrtx/*`、`src/app/widget_ovrtx_view.*`：同样按 BSD-2-Clause 贡献
- `cmake/ovrtx.cmake`、`cmake/ovstage.cmake`：复制自 ovrtx C 示例，属 NVIDIA 专有许可，仅用于拉取 SDK
- 运行时 ovrtx / ovstage 二进制包：NVIDIA 专有，使用前请遵守 NVIDIA 许可

## 上游

- Mayo: https://github.com/fougue/mayo
- ovrtx: https://github.com/nvidia-omniverse/ovrtx
