#!/bin/sh
# Which copy of nvidia-gl-host-link is stale? No build needed; read-only.
live=$(cat /sys/module/nvidia/version 2>/dev/null)
[ -n "$live" ] || live=$(grep -o '[0-9][0-9][0-9]\.[0-9]*\.[0-9]*' /proc/driver/nvidia/version 2>/dev/null | head -1)
echo "host driver now: ${live:-none loaded}"
for home in "$HOME/.xlings" "$HOME/.mcpp/registry"; do
    echo
    echo "== $home"
    found=0
    for p in "$home"/data/xpkgs/xim-x-nvidia-gl-host-link/*/; do
        [ -d "$p" ] || continue
        found=1
        stamp=$(tr -d '\n' < "$p/.host-driver-version" 2>/dev/null)
        total=$(ls "$p/lib" 2>/dev/null | grep -c '\.so\.[0-9][0-9][0-9]\.[0-9]*\.[0-9]*')
        cur=$(ls "$p/lib" 2>/dev/null | grep -c "$live")
        dang=$(find "$p/lib" -xtype l 2>/dev/null | wc -l)
        echo "  $(basename "$p")  stamp=${stamp:-none}  links named for the live driver: $cur of $total  dangling: $dang"
    done
    [ "$found" = 0 ] && echo "  (no nvidia-gl-host-link here)"
    for g in "$home"/data/xpkgs/xim-x-libglvnd/*/lib/glx-vendor; do
        [ -d "$g" ] || continue
        echo "  glvnd GLX vendors ($(basename "$(dirname "$(dirname "$g")")")): $(ls "$g" | grep -v '^\.' | tr '\n' ' ')"
    done
    for lib in "$home"/subos/*/lib; do
        [ -d "$lib" ] || continue
        echo "  subos $(basename "$(dirname "$lib")"): nvidia links dangling: $(find "$lib" -maxdepth 1 -xtype l -name '*nvidia*' 2>/dev/null | wc -l)"
    done
done
echo
echo "Reading: a copy whose 'links named for the live driver' is 0 and whose 'dangling' is > 0 is stale."
echo "mcpp uses ~/.mcpp/registry; the global ~/.xlings is a different copy."
