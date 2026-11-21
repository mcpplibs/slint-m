# check_display: does a window's GPU path work on this machine?

A Slint window needs an X11 or Wayland session and, for the default OpenGL renderer, a GL vendor
library (Mesa, or NVIDIA's). The headless build and `mcpp test` never touch any of that, so a machine
where the window dies shows no sign of it until a person runs the program. This directory is how to
find out which part is missing. Run it from a **desktop terminal** (`DISPLAY` or `WAYLAND_DISPLAY` set).

```
cd tools/check_display
./check-homes.sh              # read-only: which xlings homes hold nvidia-gl-host-link, and is it current
./run.sh                      # build and run the probe: as an mcpp program runs, with an NVIDIA overlay, through `mcpp run`
./slint-variants.sh <project> # run a Slint project's binary six ways, 8 s each
```

`slint-variants.sh` takes the project directory (default: the current one) and, if it differs from
the directory name, the binary name. Each variant opens a window for up to 8 seconds: exit **124** =
the window came up, **134** = it aborted.

## The probe (`src/main.cpp`)

It uses only `dlopen`/`dlsym`, so it sees exactly what a toolkit's own `dlopen()` sees under mcpp's
loader.

- `0.` the environment (glvnd and Mesa variables choose the vendor).
- `1.` which of the libraries a toolkit loads at run time resolve, including the NVIDIA vendor libraries.
- `2.` EGL devices (no X needed).
- `3.` EGL on X11 and how many configurations each template gets.
- `4.` GLX on X11: the server's GLX vendor and the configuration count. glutin tries GLX and then
  EGL, and reports GLXBadFBConfig as `argument does not name a valid config`. On an NVIDIA X server
  the GLX vendor is NVIDIA's, so `libGLX_nvidia.so.0` must load.
- `SUMMARY` and the exit code: 0 fine, 2 EGL on X11 gave nothing, 7 GLX gave nothing or an X error,
  6 the host has NVIDIA but the program gets no NVIDIA EGL device, 3/4 could not get that far.

## Reading the result

| what you see | meaning | what to do |
|---|---|---|
| Slint default aborts with `glutin: argument does not name a valid config`; probe part 4 says `client vendor: (null)` or `libGLX_nvidia.so.0` is not found | libglvnd is only a dispatcher and nothing wired a vendor into it: the GL provider (`xim:graphics`, through `compat.glx-runtime`) is not in the project's dependency closure | `compat.slint` carries that dependency (index PR #871); with an older descriptor, add `compat.glx-runtime = "2026.09.11"` to `[dependencies]` |
| the same, with `compat.glx-runtime` present | the `nvidia-gl-host-link` copy is stale (a driver upgrade leaves its version-named links dangling), `check-homes.sh` shows it: "links named for the live driver: 0 of N", "dangling" > 0 | reinstall `nvidia-gl-host-link`, or run `xlings-gl-doctor` |
| `SLINT_BACKEND=winit-software` works and the default does not | the software renderer needs no GL; the failure is in the GL path above | use it as a stop-gap |
| no window and a different error | an X11 or Wayland library is missing (`libX11.so.6`, `libxkbcommon-x11.so.0` ...): probe part 1 | add the library's xim package |

Wayland is not covered: the index has no package that provides `opengl.egl.driver`, and the probe
only exercises X11.
