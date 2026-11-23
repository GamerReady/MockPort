#!/usr/bin/env bash
# Builds the MockPort .pkg by driving the OpenOrbis hello_world sample Makefile,
# which is known to produce a package that launches on the console. Only the
# listed inputs are swapped in; everything else is the sample's own recipe.
# (A hand-written packaging script produced "corrupted data" on the console;
# this Makefile-driven one launches.)
# Requires: OO_PS4_TOOLCHAIN, clang, ld.lld, make, python3, libssl1.1.
# Optional env (use absolute paths):
#   OUT          output dir (the .pkg ends up in $OUT/pkg)
#   APP_SRC      main.cpp to build (default: ps4/app/main.cpp)
#   TITLE, TITLE_ID, CONTENT_ID   package identity
#   CUSTOM_ICON  1 (default) = generated icon0.png, 0 = the sample's icon
set -euo pipefail

: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to the OpenOrbis toolchain root}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${OUT:-${ROOT}/build}"
APP_SRC="${APP_SRC:-${ROOT}/app/main.cpp}"
TITLE="${TITLE:-MockPort}"
TITLE_ID="${TITLE_ID:-MOCK00001}"
CONTENT_ID="${CONTENT_ID:-IV0000-MOCK00001_00-MOCKPORT00000000}"
CUSTOM_ICON="${CUSTOM_ICON:-1}"

rm -rf "${OUT}"
mkdir -p "${OUT}/pkg"

# The sample Makefile derives the project name from the directory name, so the
# working copy must be called hello_world.
WORK="${OUT}/hello_world"
cp -r "${OO_PS4_TOOLCHAIN}/samples/hello_world" "${WORK}"
# Drop any build output copied along with the sample.
rm -rf "${WORK}/hello_world/x64" "${WORK}/pkg.gp4" "${WORK}/eboot.bin" \
       "${WORK}/sce_sys/param.sfo" "${WORK}"/*.pkg

cp "${APP_SRC}" "${WORK}/hello_world/main.cpp"

if [ "${CUSTOM_ICON}" = "1" ]; then
  echo "==> Using generated icon0.png"
  python3 "${ROOT}/tools/gen_assets.py" "${OUT}/assets"
  cp "${OUT}/assets/sce_sys/icon0.png" "${WORK}/sce_sys/icon0.png"
fi

echo "==> make (title='${TITLE}', title id=${TITLE_ID}, content id=${CONTENT_ID})"
( cd "${WORK}" && make TITLE="${TITLE}" TITLE_ID="${TITLE_ID}" CONTENT_ID="${CONTENT_ID}" )

cp "${WORK}"/*.pkg "${OUT}/pkg/"
echo "==> Done:"
ls -la "${OUT}/pkg"
