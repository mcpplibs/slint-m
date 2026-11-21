# Slint

> 通过 C++23 模块使用声明式 GUI 工具包 Slint：运行时 API、解释器，以及把 `.slint` 文件编译为模块。

[English](README.md) · **简体中文** · [上游](https://github.com/slint-ui/slint) · [发布](https://github.com/mcpplibs/slint-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [索引](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/slint-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/slint-m/actions/workflows/ci.yml)

上游 API 保持不变：`slint::ComponentHandle`、`slint::SharedString`、`slint::VectorModel`、
`slint::interpreter::ComponentCompiler` 就是 `<slint.h>` 里的名字。SDK 是 `compat.slint`
1.19.0（xim:slint：`libslint_cpp`、头文件和 `slint-compiler`，由发布 tag 按上游 CMake
默认功能构建）。`.slint` 文件由 `mcpplibs.mplugin.slint`（包 `mcpplibs.mplugin.slint`，在本仓库的 `rules/`）编译，
由工程的 `build.mcpp` 导入；设置 `module` 后，每个文件都成为程序可以 import 的模块。

## 模块

- `import mcpplibs.mwrap.slint;`：`slint::`，即窗口与组件、`SharedString`、`SharedVector`、模型、`Color`、`Brush`、`Image`、定时器、事件循环、`slint::language`
- `import mcpplibs.mwrap.slint.interpreter;`：`slint::interpreter`，运行时加载 `.slint` 源码
- `import mcpplibs.mwrap.slint.platform;`：`slint::platform`，自定义平台与软件渲染器（Clang；GCC 下请 include `<slint-platform.h>`）
- `import mcpplibs.mwrap.slint.macros;`：`SLINT_FEATURE_*` 映射为 `constexpr bool`，位于 `mcpplibs::mwrap::slint::macros`
- `import app.ui;`：你自己的组件，来自 `ui/app.slint`（`mcpplibs.mplugin.slint` 的 `options::module`）

> 更多模块 API -> [用法和 API](docs/usage.md)

## 快速开始

```sh
mcpp new counter --template mcpplibs.mwrap.slint@1.19.0
cd counter
mcpp run
```

默认模板打开一个带计数器的窗口；没有显示环境（或设置 `SLINT_HEADLESS=1`）时，用 Slint
的软件渲染器运行并打印计数。

```toml
[dependencies]
mcpplibs.mwrap.slint = "1.19.0.3"
```

```cpp
// build.mcpp
import mcpp;
import mcpplibs.mplugin.slint;
int main() {
    mcpplibs::mplugin::slint::options o;
    o.sources = {"ui/app.slint"};
    o.module = "app.ui";
    return mcpplibs::mplugin::slint::compile(o) ? 0 : 1;
}
```

```cpp
// src/main.cpp
import mcpplibs.mwrap.slint;
import app.ui;

int main() {
    auto ui = AppWindow::create();
    ui->on_increase([&] { ui->set_counter(ui->get_counter() + 1); });
    ui->run();
}
```

## SDK 的功能

`compat.slint` 1.19.0 即上游 CMake 的桌面默认功能集：`interpreter`、`backend-winit`
（X11、Wayland、Windows、macOS）、`renderer-femtovg`（OpenGL）、`renderer-software`、
`accessibility`、`system-tray`、`testing`，以及编译器。不含 Skia、LinuxKMS、Qt 与 gettext。
Linux 上该库只依赖 fontconfig（xim:fontconfig）和 C 运行时；X11、Wayland 与 GL 在用到时
动态加载。`mcpplibs::mwrap::slint::macros::SLINT_FEATURE_RENDERER_SKIA` 等常量说明哪些功能存在。

### Linux 上的窗口需要 GL 厂商库

libglvnd 只是分发器：默认的 OpenGL 渲染器开窗口还需要厂商库（Mesa；NVIDIA 的 X 服务器上是
NVIDIA 的）。`compat.slint` 依赖 `compat.glx-runtime`（`compat.glfw` 用的同一个包）来提供它，
项目不需要额外写什么；更早的描述符则需要在 `[dependencies]` 里加
`compat.glx-runtime = "2026.09.11"`。窗口仍以 `argument does not name a valid config` 中止时，
[`tools/check_display/`](tools/check_display/README.md) 能指出缺的是哪一环；
期间可用 `SLINT_BACKEND=winit-software` 不经 GL 运行。

## 构建与测试

Linux、macOS 与 Windows 的 CI 用 GCC 和 Clang 构建模块并运行测试：经模块使用运行时类型，
以及由 `slint::interpreter` 在运行时编译组件、在无头平台上通过属性和回调驱动它。导出审计
（[tools/check_exports.py](tools/check_exports.py)）读取 SDK 头文件的 clang AST：每个公开名字都已导出，
按键码与功能都已映射，SDK 的功能即上游 CMake 默认值。另有一个任务把默认模板当作已安装包生成项目，
经 `mcpplibs.mplugin.slint` 构建并无头运行。

## 许可

封装层为 [Apache-2.0](LICENSE)，作者 mcpp-community（[NOTICE](NOTICE)）。Slint 可在 GNU GPLv3、
Slint 免版税许可 2.0（桌面、移动与 Web，不含嵌入式）或 Slint 商业许可中任选其一
（[upstream/slint/LICENSE.md](upstream/slint/LICENSE.md)、[LICENSES](upstream/slint/LICENSES)）。
SDK 自带的通知（含其链接的 Rust crate）随程序部署。
