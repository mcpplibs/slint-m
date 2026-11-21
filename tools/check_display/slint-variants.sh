#!/bin/sh
# Run a Slint project's own binary in several ways and say what each does.
# Each run opens a window for up to 8 seconds, then is stopped: exit 124 means it was still
# running when stopped, i.e. the window came up; 134 means it aborted (SIGABRT).
#   usage: ./slint-variants.sh [project-dir] [binary-name]   (default: the current directory; the binary is named after the project directory)
here=$(cd "$(dirname "$0")" && pwd)
proj=${1:-.}
cd "$proj" || { echo "no such project: $proj" >&2; exit 1; }
name=${2:-$(basename "$(pwd)")}
mcpp build >/dev/null 2>&1 || mcpp build || exit 1
bin=$(find target -type f -path "*/bin/$name" -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)
[ -x "$bin" ] || { echo "no $name binary under $proj/target (pass its name as the second argument)" >&2; exit 1; }
out=$(mktemp)
run() {
    label=$1; shift
    env "$@" timeout 8 "$bin" >"$out" 2>&1; rc=$?
    case $rc in 124) verdict="window stayed up (stopped by the 8 s timeout)";; 134) verdict="ABORTED (SIGABRT)";; 0) verdict="exited normally";; *) verdict="exit $rc";; esac
    echo; echo "=== $label"; echo "    -> $verdict"
    grep -vE '^\s*$|Fontconfig|RUST_BACKTRACE|note: run with' "$out" | head -4 | cut -c1-220 | sed 's/^/    /'
}
ov=$("$here/make-overlay.sh" 2>/dev/null)
mesa=/usr/share/glvnd/egl_vendor.d/50_mesa.json
run "default"
run "software renderer: SLINT_BACKEND=winit-software" SLINT_BACKEND=winit-software
run "OpenGL renderer, explicit: SLINT_BACKEND=winit-femtovg" SLINT_BACKEND=winit-femtovg
run "OpenGL, GLX vendor forced to mesa" SLINT_BACKEND=winit-femtovg __GLX_VENDOR_LIBRARY_NAME=mesa
[ -f "$mesa" ] && run "OpenGL, EGL and GLX both forced to mesa" SLINT_BACKEND=winit-femtovg __GLX_VENDOR_LIBRARY_NAME=mesa __EGL_VENDOR_LIBRARY_FILENAMES=$mesa
[ -n "$ov" ] && run "default + overlay of the live NVIDIA driver (LD_LIBRARY_PATH=$ov)" LD_LIBRARY_PATH="$ov"
rm -f "$out"
