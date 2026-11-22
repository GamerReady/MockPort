#!/usr/bin/env bash
# Informational probe: syntax-checks the PS4-specific sources of the Mocktail
# port (and Mbed TLS) with the OpenOrbis clang flags. It never fails the job;
# it writes a report so you can see what still blocks a real Orbis build.
set -uo pipefail

: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC="${ROOT}/upstream/mocktail"
OUT="${OUT:-${ROOT}/ps4/build}"
REPORT="${OUT}/probe-report.md"
CXX="${CXX:-clang++}"
CC="${CC:-clang}"
mkdir -p "${OUT}/probe_inc"
[ -e "${OUT}/probe_inc/nlohmann" ] || ln -s /usr/include/nlohmann "${OUT}/probe_inc/nlohmann" 2>/dev/null || true

COMMON=(--target=x86_64-pc-freebsd12-elf -fsyntax-only -fPIC -funwind-tables
  -isysroot "${OO_PS4_TOOLCHAIN}" -isystem "${OO_PS4_TOOLCHAIN}/include")
INCS=(-I"${SRC}/compat" -I"${SRC}/include" -I"${SRC}/src"
  -I"${SRC}/third_party/mbedtls/include" -I"${SRC}/third_party/jni/include"
  -isystem "${OUT}/probe_inc" -DMOCKTAIL_PS4_PORT=1)

ok=0; fail=0
{
  echo "# PS4 port probe (OpenOrbis, syntax-only)"
  echo
  echo "| Source | Result |"
  echo "|---|---|"
} > "${REPORT}"

probe() {  # $1=compiler $2=file; remaining args = flags
  local compiler="$1" file="$2"; shift 2
  local log="${OUT}/probe_inc/$(echo "${file#${SRC}/}" | tr '/' '_').log"
  if "${compiler}" "${COMMON[@]}" "$@" "${file}" >"${log}" 2>&1; then
    echo "| \`${file#${SRC}/}\` | ok |" >> "${REPORT}"; ok=$((ok+1))
  else
    local first
    first="$(grep -m1 -E 'error|fatal' "${log}" | sed 's/|/\\|/g' | cut -c1-200)"
    echo "| \`${file#${SRC}/}\` | FAIL: ${first} |" >> "${REPORT}"; fail=$((fail+1))
  fi
}

while IFS= read -r f; do
  probe "${CXX}" "$f" -std=c++17 "${INCS[@]}" -isystem "${OO_PS4_TOOLCHAIN}/include/c++/v1"
done < <(find "${SRC}/src" "${SRC}/stubs" -name '*ps4*.cc' | sort)

while IFS= read -r f; do
  probe "${CC}" "$f" -std=c99 -I"${SRC}/third_party/mbedtls/include" -I"${SRC}/third_party/mbedtls/library"
done < <(find "${SRC}/third_party/mbedtls/library" -maxdepth 1 -name '*.c' 2>/dev/null | sort)

{ echo; echo "**ok: ${ok}, failed: ${fail}**"; } >> "${REPORT}"
cat "${REPORT}"
[ -n "${GITHUB_STEP_SUMMARY:-}" ] && cat "${REPORT}" >> "${GITHUB_STEP_SUMMARY}"
exit 0
