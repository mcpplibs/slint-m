#!/bin/sh
# Build the probe with mcpp, run it as mcpp programs run today (A), then again with an
# overlay generated from the live driver version on the search path (B).
# B runs the built binary directly: LD_LIBRARY_PATH holds only NVIDIA libraries, so it cannot
# pull in a second libc, and it does not depend on what `mcpp run` does with the environment.
cd "$(dirname "$0")" || exit 1
mcpp build || exit 1
bin=$(find target -type f -path '*/bin/glprobe' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d' ' -f2-)
[ -x "$bin" ] || { echo "glprobe was not built" >&2; exit 1; }
echo; echo "################ A: as an mcpp-built program runs today"
"$bin"; a=$?
echo; echo "################ B: plus an overlay of the live driver's NVIDIA libraries"
ov=$(./make-overlay.sh) || { echo "(no NVIDIA driver: B skipped)"; echo; echo "A exit=$a"; exit "$a"; }
echo "overlay: $ov ($(ls "$ov" | wc -l) links)"
LD_LIBRARY_PATH="$ov" "$bin"; b=$?
echo; echo "################ C: through mcpp run (mcpp's own environment, not your shell's)"
mcpp run; c=$?
echo; echo "A exit=$a   B exit=$b   C exit=$c   (0 fine; 2 EGL on X11 gave no configuration; 7 GLX gave none or an X error; 6 the host has NVIDIA but this program gets no NVIDIA EGL device; 3/4 could not get that far)"
