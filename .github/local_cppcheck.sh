#!/usr/bin/env bash
# local_cppcheck.sh — mide la deuda de cppcheck EN LOCAL con la MISMA version
# que la CI (2.21.0) y compara contra .github/cppcheck-baseline.txt.
#
#   bash .github/local_cppcheck.sh           # usa cppcheck 2.21.0 del PATH o $CPPCHECK
#   bash .github/local_cppcheck.sh --fetch   # si no lo tienes, extrae el MSI oficial (Windows)
#
# Sale 0 si no hay firmas NUEVAS (el gate de CI quedaria verde) y 1 si hay
# regresion o si la medicion no llega a hacerse.
#
# NO poda la baseline: podar es cosa de .github/prune_baselines.sh en la CI,
# porque aqui las rutas absolutas de Windows no son las que mide el runner.
set -u
cd "$(dirname "$0")/.." || exit 1
want=2.21.0
fetch=0
[ "${1:-}" = "--fetch" ] && fetch=1

buscar() {
  local c
  for c in "${CPPCHECK:-}" cppcheck; do
    [ -n "$c" ] || continue
    command -v "$c" >/dev/null 2>&1 || continue
    "$c" --version 2>&1 | grep -F "Cppcheck $want" >/dev/null && { echo "$c"; return 0; }
  done
  return 1
}

CP=$(buscar || true)
if [ -z "$CP" ]; then
  if [ "$fetch" -eq 1 ] && command -v msiexec >/dev/null 2>&1 && command -v cygpath >/dev/null 2>&1; then
    d="${TMPDIR:-/tmp}/cppcheck-$want"
    if [ ! -x "$d/PFiles/Cppcheck/cppcheck.exe" ]; then
      msi="${TMPDIR:-/tmp}/cppcheck-$want.msi"
      curl -sL -o "$msi" "https://github.com/cppcheck-opensource/cppcheck/releases/download/$want/cppcheck-$want-x64-Setup.msi" || exit 1
      msiexec //a "$(cygpath -w "$msi")" //qn TARGETDIR="$(cygpath -w "$d")" || exit 1
    fi
    CP="$d/PFiles/Cppcheck/cppcheck.exe"
  else
    echo "ERROR: hace falta cppcheck $want, la version con la que se midio la baseline."
    echo "  Windows: bash .github/local_cppcheck.sh --fetch   (extrae el MSI, no instala nada)"
    echo "  Linux:   git clone --depth 1 -b $want https://github.com/cppcheck-opensource/cppcheck.git"
    echo "           make -C cppcheck -j\$(nproc) FILESDIR=\$PWD/cppcheck/cfg"
    echo "           CPPCHECK=cppcheck/cppcheck bash .github/local_cppcheck.sh"
    exit 1
  fi
fi
"$CP" --version

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

sup=(--suppress=missingIncludeSystem)
while IFS= read -r l || [ -n "$l" ]; do
  l=${l%%#*}; l=${l#"${l%%[![:space:]]*}"}; l=${l%"${l##*[![:space:]]}"}
  [ -z "$l" ] && continue
  if [ "${l:0:1}" = "-" ]; then sup+=($l); else sup+=("--suppress=$l"); fi
done < .github/cppcheck-excludes.txt

set +e
"$CP" --enable=warning,performance,portability --std=c++20 "${sup[@]}" --template='{severity}|{file}|{line}|{message}' Source/ > "$tmp/raw.txt" 2>&1
code=$?
set -e
if [ ! -s "$tmp/raw.txt" ]; then
  echo "ERROR: cppcheck no ha producido ni una linea (codigo $code): la medicion no se hizo."
  exit 1
fi

PY=$(command -v python3 || command -v python || true)
[ -n "$PY" ] || { echo "ERROR: hace falta python3/python para extraer las firmas."; exit 1; }
set +e
"$PY" - "$tmp/raw.txt" .github/cppcheck-baseline.txt <<'PY'
import re, sys
BS = chr(92)
pat = re.compile(r"^(warning|performance|portability|error)[|]([^|]+)[|][^|]+?[|](.*)$")
def rel(p):
    s = p.replace(BS, '/')
    k = s.find('ABDJUNiO601/')
    return s[k + len('ABDJUNiO601/'):] if k >= 0 else s
meas = set()
for ln in open(sys.argv[1], encoding='utf-8', errors='replace'):
    m = pat.match(ln.rstrip('\r\n'))
    if m:
        meas.add(m.group(1) + '|' + rel(m.group(2)) + '|' + m.group(3))
base = set()
for ln in open(sys.argv[2], encoding='utf-8', errors='replace'):
    ln = ln.rstrip('\r\n')
    if ln and not ln.startswith('#') and '|' in ln:
        base.add(ln)
new = sorted(meas - base)
old = sorted(base - meas)
print('firmas_medidas=%d baseline=%d' % (len(meas), len(base)))
print('NUEVAS=%d' % len(new))
for x in new[:30]: print('  +', x)
print('OBSOLETAS=%d' % len(old))
for x in old[:20]: print('  -', x)
sys.exit(1 if new else 0)
PY
rc=$?
set -e
if [ "$rc" -eq 0 ]; then echo "GATE OK: sin firmas nuevas fuera de la baseline."; else echo "GATE ROJO: firmas nuevas o medicion fallida."; fi
exit "$rc"
