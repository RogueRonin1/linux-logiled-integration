#!/usr/bin/env bash
# Enable or disable the LogiLED shim for Total War: Warhammer 3 by editing the
# Proton prefix registry directly.
#
# libled.dll opens SOFTWARE\Classes\CLSID\{A6519E67-...}\ServerBinary as a
# *subkey* and reads its default value — it is not a value under the CLSID key.
#
# The game must not be running: the prefix registry is only re-read when
# wineserver starts.
#
#   ./tools/wh3-shim.sh on      # register the signed shim
#   ./tools/wh3-shim.sh off     # restore (removes the key)
#   ./tools/wh3-shim.sh status
set -euo pipefail

PFX="${WH3_PFX:-/var/mnt/games/SteamLibrary/steamapps/compatdata/1142710/pfx}"
REG="$PFX/system.reg"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DLL="$REPO/build/LogitechLedEnginesWrapper-signed.dll"
CLSID="A6519E67-7632-4375-AFDF-CAA889744403"

[[ -f "$REG" ]] || { echo "no prefix registry at $REG"; exit 1; }

if pgrep -x Warhammer3.exe >/dev/null 2>&1; then
    echo "Warhammer3.exe is running — quit it first"; exit 1
fi

case "${1:-status}" in
on)
    [[ -f "$DLL" ]] || { echo "signed shim missing: $DLL"; echo "build it first (see README)"; exit 1; }
    cp -a "$REG" "$REG.g915-backup.$(date +%s)"
    python3 - "$REG" "$DLL" "$CLSID" <<'PY'
import sys, time
reg, dll, clsid = sys.argv[1], sys.argv[2], sys.argv[3]
txt = open(reg, encoding="utf-8", errors="surrogateescape").read()
if clsid in txt:
    print("already registered"); raise SystemExit
win = ("Z:" + dll.replace("/", "\\")).replace("\\", "\\\\")
key = r"[Software\\Classes\\CLSID\\{%s}\\ServerBinary]" % clsid
with open(reg, "a", encoding="utf-8", errors="surrogateescape") as f:
    f.write(f'\n{key} {int(time.time())}\n#time=1dd2a8caf73702c\n@="{win}"\n')
print("registered ->", win.replace("\\\\", "\\"))
PY
    echo "trace will be written beside the DLL: $REPO/build/logiled-shim-trace.jsonl"
    ;;
off)
    python3 - "$REG" "$CLSID" <<'PY'
import sys
reg, clsid = sys.argv[1], sys.argv[2]
lines = open(reg, encoding="utf-8", errors="surrogateescape").read().split("\n")
out, skip, removed = [], False, 0
for ln in lines:
    if ln.startswith("["):
        skip = clsid in ln
        if skip: removed += 1
    if not skip:
        out.append(ln)
open(reg, "w", encoding="utf-8", errors="surrogateescape").write("\n".join(out))
print(f"removed {removed} key block(s)")
PY
    ;;
status)
    if grep -q "$CLSID" "$REG"; then
        echo "ENABLED"; grep -A2 "$CLSID" "$REG" | head -3
    else
        echo "disabled (prefix is in its stock state)"
    fi
    ;;
*)
    echo "usage: $0 {on|off|status}"; exit 1
    ;;
esac
