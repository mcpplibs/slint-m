#!/bin/sh
# Prints the path of a directory of symlinks to the host's NVIDIA user-space libraries, built by
# globbing the host for the driver that is loaded NOW. No list of file names; nothing is copied.
live=$(cat /sys/module/nvidia/version 2>/dev/null)
[ -n "$live" ] || live=$(grep -o '[0-9][0-9][0-9]\.[0-9]*\.[0-9]*' /proc/driver/nvidia/version 2>/dev/null | head -1)
[ -n "$live" ] || { echo "no NVIDIA driver is loaded" >&2; exit 1; }
libdir=${HOST_LIBDIR:-/usr/lib/x86_64-linux-gnu}
dir=${TMPDIR:-/tmp}/gl-overlay-$live
rm -rf "$dir"; mkdir -p "$dir"
for f in "$libdir"/libnvidia-*.so* "$libdir"/libEGL_nvidia.so* "$libdir"/libGLX_nvidia.so* "$libdir"/libGLESv*_nvidia.so*; do
    [ -e "$f" ] && ln -s "$f" "$dir/"
done
echo "$dir"
