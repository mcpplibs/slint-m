#!/usr/bin/env python3
"""Audit the module surface against Slint's SDK headers.

- src/*.cppm are what tools/gen_exports.py writes from the headers of the SDK
  compat.slint binds (upstream/documents.json pins the archive): every name
  declared directly in slint, slint::language, slint::interpreter and
  slint::platform is exported, and every slint::platform::key_codes constant
  (internal linkage) is defined in mcpplibs::mwrap::slint::platform::key_codes.
- Every SLINT_FEATURE_* the headers test is a constant of the macros module,
  and the features the SDK defines are exactly upstream CMake's defaults.

Reads clang's AST: pass --clang or set CLANGXX if clang++ is not on PATH or in
the toolchain mcpp installed.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from inventory import fetch, find_clang, surface  # noqa: E402

# api/cpp/cmake/SlintFeatures.cmake, desktop defaults, as the headers spell them
# (system-tray and the compiler state no macro).
DEFAULTS = {'SLINT_FEATURE_ACCESSIBILITY', 'SLINT_FEATURE_BACKEND_WINIT', 'SLINT_FEATURE_INTERPRETER',
            'SLINT_FEATURE_RENDERER_FEMTOVG', 'SLINT_FEATURE_RENDERER_SOFTWARE', 'SLINT_FEATURE_TESTING'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang')
    parser.add_argument('--sdk')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    clang = find_clang(args.clang)
    problems = []
    command = [sys.executable, str(root / 'tools/gen_exports.py'), '--check', '--clang', clang]
    if args.sdk:
        command += ['--sdk', args.sdk]
    generated = subprocess.run(command, capture_output=True, text=True)
    if generated.returncode != 0:
        problems.append(generated.stderr.strip() or generated.stdout.strip())
    sf = surface(fetch(root, args.sdk), clang)
    units = {ns: (root / f).read_text(encoding='utf-8') for ns, f in (
        ('slint', 'src/slint.cppm'), ('slint::language', 'src/slint.cppm'),
        ('slint::interpreter', 'src/interpreter.cppm'), ('slint::platform', 'src/platform.cppm'))}
    for ns, text in units.items():
        exported = set(re.findall(rf'^\s*using {re.escape(ns)}::(\w+(?:[=!<>]+)?);', text, flags=re.M))
        if set(sf.names[ns]) != exported:
            problems.append(f'{ns}: missing {sorted(set(sf.names[ns]) - exported)}, extra {sorted(exported - set(sf.names[ns]))}')
    keys = set(re.findall(r'^inline constexpr auto (\w+) = mwrap_slint_keys::', units['slint::platform'], flags=re.M))
    if keys != set(sf.names['slint::platform::key_codes']):
        problems.append('key_codes differ from the header')
    mapped = set(re.findall(r'^inline constexpr bool (SLINT_FEATURE_\w+) =',
                            (root / 'src/macros.cppm').read_text(encoding='utf-8'), flags=re.M))
    if mapped != set(sf.feature_universe):
        problems.append(f'features: missing {sorted(set(sf.feature_universe) - mapped)}, extra {sorted(mapped - set(sf.feature_universe))}')
    if set(sf.features) != DEFAULTS:
        problems.append(f'the SDK defines {sf.features}, not upstream CMake\'s defaults {sorted(DEFAULTS)}')
    if problems:
        sys.exit('\n'.join(problems))
    print(f"slint {len(sf.names['slint'])}, slint::language {len(sf.names['slint::language'])}, "
          f"slint::interpreter {len(sf.names['slint::interpreter'])}, slint::platform {len(sf.names['slint::platform'])} "
          f"names exported; {len(keys)} key codes defined; {len(mapped)} features mapped, "
          f"{len(sf.features)} defined (upstream CMake's defaults)")


if __name__ == '__main__':
    main()
