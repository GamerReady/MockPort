#!/usr/bin/env bash
# Builds the MockPort PS4 shell with the OpenOrbis toolchain and packs a .pkg.
# Requires: OO_PS4_TOOLCHAIN, clang, ld.lld, python3, dotnet runtime (PkgTool.Core).
set -euo pipefail

: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to the OpenOrbis toolchain root}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${OUT:-${ROOT}/build}"
APP=mockport
CONTENT_ID="${CONTENT_ID:-IV0000-MOCK00001_00-MOCKPORT00000000}"
TITLE_ID="${TITLE_ID:-MOCK00001}"
CXX="${CXX:-clang++}"
LD="${LD:-ld.lld}"

case "$(uname -s)" in
  Linux)  OO_BIN="${OO_PS4_TOOLCHAIN}/bin/linux" ;;
  Darwin) OO_BIN="${OO_PS4_TOOLCHAIN}/bin/macos" ;;
  *) echo "unsupported host OS" >&2; exit 1 ;;
esac

rm -rf "${OUT}"
mkdir -p "${OUT}/obj" "${OUT}/pkg"

echo "==> Generating assets"
python3 "${ROOT}/tools/gen_assets.py" "${OUT}/pkg" \
  --content-id "${CONTENT_ID}" --title-id "${TITLE_ID}"

echo "==> Compiling"
"${CXX}" --target=x86_64-pc-freebsd12-elf -std=c++17 -O2 -fPIC -funwind-tables \
  -fno-exceptions -c \
  -isysroot "${OO_PS4_TOOLCHAIN}" \
  -isystem "${OO_PS4_TOOLCHAIN}/include" \
  -isystem "${OO_PS4_TOOLCHAIN}/include/c++/v1" \
  "${ROOT}/app/main.cpp" -o "${OUT}/obj/main.o"

echo "==> Linking"
"${LD}" -m elf_x86_64 -pie --script "${OO_PS4_TOOLCHAIN}/link.x" --eh-frame-hdr \
  -L"${OO_PS4_TOOLCHAIN}/lib" "${OUT}/obj/main.o" -o "${OUT}/${APP}.elf" \
  -lc -lkernel -lc++ "${OO_PS4_TOOLCHAIN}/lib/crt1.o"

echo "==> Creating eboot.bin"
"${OO_BIN}/create-fself" -in="${OUT}/${APP}.elf" -out="${OUT}/${APP}.oelf" \
  --eboot "${OUT}/pkg/eboot.bin" --paid 0x3800000000000011

echo "==> Building .pkg"
cd "${OUT}/pkg"
"${OO_BIN}/create-gp4" -out pkg.gp4 --content-id="${CONTENT_ID}" \
  --files "eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/pic0.png"
"${OO_BIN}/PkgTool.Core" pkg_build pkg.gp4 .

echo "==> Done:"
ls -la "${OUT}/pkg"/*.pkg "${OUT}/pkg/eboot.bin"
