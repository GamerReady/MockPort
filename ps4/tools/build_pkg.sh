#!/usr/bin/env bash
# Builds the MockPort PS4 shell with the OpenOrbis toolchain and packs a .pkg.
# Requires: OO_PS4_TOOLCHAIN, clang, ld.lld, python3, libssl1.1 (PkgTool.Core).
# Optional env: OUT, APP_SRC, TITLE, VERSION, CONTENT_ID, TITLE_ID (use
# absolute paths for OUT and APP_SRC).
set -euo pipefail

: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to the OpenOrbis toolchain root}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${OUT:-${ROOT}/build}"
APP_SRC="${APP_SRC:-${ROOT}/app/main.cpp}"
APP=mockport
TITLE="${TITLE:-MockPort}"
VERSION="${VERSION:-01.00}"
CONTENT_ID="${CONTENT_ID:-IV0000-MOCK00001_00-MOCKPORT00000000}"
TITLE_ID="${TITLE_ID:-MOCK00001}"
CXX="${CXX:-clang++}"
LD="${LD:-ld.lld}"

case "$(uname -s)" in
  Linux)  OO_BIN="${OO_PS4_TOOLCHAIN}/bin/linux" ;;
  Darwin) OO_BIN="${OO_PS4_TOOLCHAIN}/bin/macos" ;;
  *) echo "unsupported host OS" >&2; exit 1 ;;
esac
PKGTOOL="${OO_BIN}/PkgTool.Core"

rm -rf "${OUT}"
mkdir -p "${OUT}/obj" "${OUT}/pkg"

echo "==> Generating images"
python3 "${ROOT}/tools/gen_assets.py" "${OUT}/pkg" \
  --content-id "${CONTENT_ID}" --title-id "${TITLE_ID}"

# Same param.sfo recipe as the OpenOrbis hello_world sample (PkgTool.Core).
echo "==> Creating param.sfo with PkgTool.Core"
rm -f "${OUT}/pkg/sce_sys/param.sfo"
SFO="${OUT}/pkg/sce_sys/param.sfo"
"${PKGTOOL}" sfo_new "${SFO}"
"${PKGTOOL}" sfo_setentry "${SFO}" APP_TYPE --type Integer --maxsize 4 --value 1
"${PKGTOOL}" sfo_setentry "${SFO}" APP_VER --type Utf8 --maxsize 8 --value "${VERSION}"
"${PKGTOOL}" sfo_setentry "${SFO}" ATTRIBUTE --type Integer --maxsize 4 --value 0
"${PKGTOOL}" sfo_setentry "${SFO}" CATEGORY --type Utf8 --maxsize 4 --value 'gd'
"${PKGTOOL}" sfo_setentry "${SFO}" CONTENT_ID --type Utf8 --maxsize 48 --value "${CONTENT_ID}"
"${PKGTOOL}" sfo_setentry "${SFO}" DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
"${PKGTOOL}" sfo_setentry "${SFO}" SYSTEM_VER --type Integer --maxsize 4 --value 0
"${PKGTOOL}" sfo_setentry "${SFO}" TITLE --type Utf8 --maxsize 128 --value "${TITLE}"
"${PKGTOOL}" sfo_setentry "${SFO}" TITLE_ID --type Utf8 --maxsize 12 --value "${TITLE_ID}"
"${PKGTOOL}" sfo_setentry "${SFO}" VERSION --type Utf8 --maxsize 8 --value "${VERSION}"

echo "==> Compiling ${APP_SRC}"
"${CXX}" --target=x86_64-pc-freebsd12-elf -std=c++17 -O2 -fPIC -funwind-tables \
  -fno-exceptions -c \
  -isysroot "${OO_PS4_TOOLCHAIN}" \
  -isystem "${OO_PS4_TOOLCHAIN}/include" \
  -isystem "${OO_PS4_TOOLCHAIN}/include/c++/v1" \
  "${APP_SRC}" -o "${OUT}/obj/main.o"

echo "==> Linking"
"${LD}" -m elf_x86_64 -pie --script "${OO_PS4_TOOLCHAIN}/link.x" --eh-frame-hdr \
  -L"${OO_PS4_TOOLCHAIN}/lib" "${OUT}/obj/main.o" -o "${OUT}/${APP}.elf" \
  -lc -lkernel -lc++ "${OO_PS4_TOOLCHAIN}/lib/crt1.o"

echo "==> Creating eboot.bin"
"${OO_BIN}/create-fself" -in="${OUT}/${APP}.elf" -out="${OUT}/${APP}.oelf" \
  --eboot "${OUT}/pkg/eboot.bin" --paid 0x3800000000000011

echo "==> Building .pkg"
cd "${OUT}/pkg"
FILES="eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/pic0.png"
# Optional extras, if the toolchain ships them.
if [ -f "${OO_PS4_TOOLCHAIN}/samples/hello_world/sce_sys/about/right.sprx" ]; then
  mkdir -p sce_sys/about
  cp "${OO_PS4_TOOLCHAIN}/samples/hello_world/sce_sys/about/right.sprx" sce_sys/about/
  FILES="${FILES} sce_sys/about/right.sprx"
fi
"${OO_BIN}/create-gp4" -out pkg.gp4 --content-id="${CONTENT_ID}" --files "${FILES}"
"${PKGTOOL}" pkg_build pkg.gp4 .

echo "==> Done:"
ls -la "${OUT}/pkg"/*.pkg "${OUT}/pkg/eboot.bin"
