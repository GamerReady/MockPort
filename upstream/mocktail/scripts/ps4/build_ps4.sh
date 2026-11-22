#!/usr/bin/env bash
# Copyright 2026 MockPort Project Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Builds the PS4 (Orbis OS) port of Mocktail.
#
# Prerequisites:
#   - A PS4 SDK (Orbis OS SDK) providing a clang that targets Orbis OS and the
#     Orbis OS sysroot. Point PS4_SDK_ROOT at the SDK root.
#   - SDL3, nlohmann_json, libyaml, minizip, capstone, libutf8proc,
#     fontconfig/freetype, libelf, and (optionally) libsecret cross-built for
#     Orbis OS and installed into the SDK sysroot (or findable through
#     pkg-config with the PS4 prefix).
#   - CMake >= 3.20.
#
# Usage:
#   PS4_SDK_ROOT=/opt/ps4-sdk ./scripts/ps4/build_ps4.sh
#
# The build output lands in build-ps4/.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-ps4}"

: "${PS4_SDK_ROOT:?Set PS4_SDK_ROOT to the PS4 (Orbis OS) SDK root}"

if ! command -v cmake >/dev/null 2>&1; then
  echo "error: cmake is required" >&2
  exit 1
fi

echo "==> Configuring PS4 port (SDK: ${PS4_SDK_ROOT})"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE="${REPO_ROOT}/cmake/ps4/ps4-toolchain.cmake" \
  -DPSG4_SDK_ROOT="${PS4_SDK_ROOT}" \
  -DMOCKTAIL_PS4_PORT=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo

echo "==> Building"
cmake --build "${BUILD_DIR}" -j "$(nproc 2>/dev/null || echo 4)"

echo
echo "==> Done. Artifacts:"
echo "    executable : ${BUILD_DIR}/mocktail"
echo "    stubs      : ${BUILD_DIR}/lib*.so"
echo
echo "Package with: cmake --install ${BUILD_DIR} --prefix <ps4-app-dir>"
