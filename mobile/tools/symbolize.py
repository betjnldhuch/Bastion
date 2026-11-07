#!/usr/bin/env python3
"""Symbolize a Bastion game crash report (the "Стек:" lines from the launcher).

Usage: symbolize.py <report.txt> <libs-dir> [llvm-symbolizer]

<libs-dir> contains the native libraries of the exact same build:
unstripped libsamp.so (with DWARF) and the other .so files from the APK.
Frames look like:  #00 pc 001234ab  libsamp.so (...)
"""
import re
import subprocess
import sys
from pathlib import Path

if len(sys.argv) < 3:
    raise SystemExit(__doc__)
report = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
libs = Path(sys.argv[2])
symbolizer = sys.argv[3] if len(sys.argv) > 3 else "llvm-symbolizer"

frame_re = re.compile(r"^#(\d+)\s+pc\s+([0-9a-fA-F]+)\s+(\S+)")
out = []
for line in report.splitlines():
    m = frame_re.match(line.strip())
    if not m:
        continue
    index, pc, lib = m.group(1), m.group(2), m.group(3)
    lib_name = lib.split("!")[-1].split("/")[-1]
    path = libs / lib_name
    if not path.is_file():
        out.append(f"#{index} {lib_name}+0x{pc}: (нет файла библиотеки в сборке — системная)")
        continue
    try:
        res = subprocess.run([symbolizer, "--demangle", "--inlines", "--functions=linkage",
                              f"--obj={path}", f"0x{pc}"], capture_output=True, text=True, timeout=60)
        lines = [l for l in res.stdout.splitlines() if l.strip()]
        pretty = []
        for i in range(0, len(lines) - 1, 2):
            pretty.append(f"{lines[i]}  at {lines[i + 1]}")
        out.append(f"#{index} {lib_name}+0x{pc}:\n      " + "\n      ".join(pretty or ["??"]))
    except Exception as exc:  # noqa: BLE001
        out.append(f"#{index} {lib_name}+0x{pc}: ошибка символизации: {exc}")

if not out:
    raise SystemExit("В отчёте не найдено строк стека вида '#00 pc 0000abcd  libX.so'")
print("\n".join(out))
