# Slint

> Slint, the declarative GUI toolkit, through C++23 modules: its runtime API, the interpreter and `.slint` files compiled into modules.

**English** · [简体中文](README.zh.md) · [Upstream](https://github.com/slint-ui/slint) · [Releases](https://github.com/mcpplibs/slint-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/slint-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/slint-m/actions/workflows/ci.yml)

The upstream API is unchanged: `slint::ComponentHandle`, `slint::SharedString`,
`slint::VectorModel`, `slint::interpreter::ComponentCompiler` are the names of
`<slint.h>`. The SDK is `compat.slint` 1.19.0 (xim:slint: `libslint_cpp`, its
headers and `slint-compiler`, built from the release tag with upstream CMake's
default features). A `.slint` file is compiled by `slint.mcpp.plugins` (package
`mcpplibs.slintgen`, in `rules/` of this repository), which the project's `build.mcpp`
imports; with `module` set, each file becomes a module the program imports.

## Modules

- `import mcpplibs.mwrap.slint;`: `slint::` -- windows and components, `SharedString`, `SharedVector`, models, `Color`, `Brush`, `Image`, timers, the event loop, `slint::language`
- `import mcpplibs.mwrap.slint.interpreter;`: `slint::interpreter` -- `.slint` sources loaded at run time
- `import mcpplibs.mwrap.slint.platform;`: `slint::platform` -- a custom platform, the software renderer (Clang; with GCC include `<slint-platform.h>`)
- `import mcpplibs.mwrap.slint.macros;`: `SLINT_FEATURE_*` as `constexpr bool` in `mcpplibs::mwrap::slint::macros`
- `import app.ui;`: a component of yours, from `ui/app.slint` (`slint.mcpp.plugins`, `options::module`)

> More Modules API -> [Usage and API](docs/usage.md)

## Getting started

```sh
mcpp new counter --template mcpplibs.mwrap.slint@1.19.0
cd counter
mcpp run
```

The default template opens a window with a counter; without a display (or with
`SLINT_HEADLESS=1`) it runs on Slint's software renderer and prints the counter.

```toml
[dependencies]
mcpplibs.mwrap.slint = "1.19.0.1"

[build-dependencies]
mcpplibs.slintgen = { version = "1.19.0.1", features = ["slint"] }   # claims `.slint`

[build]
sources = ["src/*.cpp", "ui/app.slint"]
```

```cpp
// build.mcpp
import mcpp;
import slint.mcpp.plugins;
int main() {
    mcpp::plugins::slint::options o;
    o.module = "app.ui";
    return mcpp::plugins::slint::compile(o) ? 0 : 1;
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

## Features of the SDK

`compat.slint` 1.19.0 is upstream CMake's default desktop set: `interpreter`,
`backend-winit` (X11, Wayland, Windows, macOS), `renderer-femtovg` (OpenGL),
`renderer-software`, `accessibility`, `system-tray`, `testing`, and the compiler.
Skia, LinuxKMS, Qt and gettext are not in it. On Linux the library needs
fontconfig (xim:fontconfig) and the C runtime only; X11, Wayland and GL are loaded
when used. `mcpplibs::mwrap::slint::macros::SLINT_FEATURE_RENDERER_SKIA` and the
others say which features are present.

## Build and testing

Linux, macOS and Windows CI build the modules with GCC and Clang and run the tests:
the runtime types through the module, and a component compiled at run time by
`slint::interpreter` and driven headless through its properties and callbacks. The
export audit ([tools/check_exports.py](tools/check_exports.py)) reads clang's AST of
the SDK's headers: every public name exported, the key codes and features mapped,
the SDK's features upstream CMake's defaults. A separate job scaffolds the default
template as an installed package, builds it with `slint.mcpp.plugins` and runs it
headless.

## License

[Apache-2.0](LICENSE) for the wrapper, by mcpp-community ([NOTICE](NOTICE)). Slint is
available under the GNU GPLv3, the Slint Royalty-free License 2.0 (desktop, mobile
and web; not embedded) or the Slint Commercial License, at your choice
([upstream/slint/LICENSE.md](upstream/slint/LICENSE.md), [LICENSES](upstream/slint/LICENSES)).
The SDK's own notices, including the Rust crates it links, are deployed with programs.
