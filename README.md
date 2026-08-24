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

- 接入点是 `IWidgetOccView`。`Mayo_UseOvrtx=ON`（默认）时，`initGui()` 把视口工厂设为 `WidgetOvrtxView::create`，不再创建 `QOpenGLWidgetOccView`。
- OCCT 仍会创建 `Graphic3d_GraphicDriver` 和 `V3d_View`，用于相机、选择和测量；**不会把 OCCT OpenGL 画到屏幕上**。
- 几何变化时重写 USDA 并 `ovstage_population_open_usd_from_string`；仅相机运动时走 `ovrtx_set_xform_mat`。
- 坐标：OCCT Z-up `(x, y, z)` → USD Y-up `(x, z, -y)`。
- 没有 RTX GPU 或 SDK 运行时失败时，视口显示错误文案，进程不崩溃。

## 构建应用

依赖：

- CMake ≥ 3.16、C++17 编译器
- Qt 5.12+ 或 Qt 6（Widgets）
- OpenCascade 7.x
- **NVIDIA RTX GPU**（运行视口）以及可由 CMake 下载的 ovrtx / ovstage 二进制包

```bash
cmake -S . -B build -DMayo_UseOvrtx=ON
cmake --build build --target mayo
```

首次配置会从 GitHub Releases 拉取 ovrtx / ovstage SDK（体积较大，NVIDIA 专有许可）。包被放在 `cmake/_deps/`（已 gitignore）。

关闭 RTX 视口、回到原版 OCCT OpenGL：

```bash
cmake -S . -B build -DMayo_UseOvrtx=OFF
```

## 不依赖 GPU 的 USD 测试

本环境没有 NVIDIA GPU / Qt / OCCT 时，仍可验证网格→USDA、look-at 矩阵和 Z-up 转换：

```bash
cmake -S tests/ovrtx -B build-ovrtx-usd -DCMAKE_CXX_COMPILER=g++
cmake --build build-ovrtx-usd
ctest --test-dir build-ovrtx-usd --output-on-failure
```

若默认 `c++` 是 clang 且链接时报找不到 `-lstdc++`，请像上面一样显式指定 `g++`。

完整 Mayo 工程在配置成功后也会生成 `test-ovrtx-usd` 目标。

## 许可

- Mayo 源码：BSD-2-Clause（见 `LICENSE.txt`）
- `src/ovrtx/*`、`src/app/widget_ovrtx_view.*`：同样按 BSD-2-Clause 贡献
- `cmake/ovrtx.cmake`、`cmake/ovstage.cmake`：复制自 ovrtx C 示例，属 NVIDIA 专有许可，仅用于拉取 SDK
- 运行时 ovrtx / ovstage 二进制包：NVIDIA 专有，使用前请遵守 NVIDIA 许可

## 上游

- Mayo: https://github.com/fougue/mayo
- ovrtx: https://github.com/nvidia-omniverse/ovrtx
