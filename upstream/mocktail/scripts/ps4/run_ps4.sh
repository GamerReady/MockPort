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

# Launcher for the PS4 (jailbroken Orbis OS) port of Mocktail.
#
# Run this from the installed app directory on the PS4. It sets up the
# runtime environment (paths, library search path, TLS CA bundle) and then
# executes the Mocktail binary, forwarding all arguments.
#
# Environment overrides:
#   MOCKTAIL_PS4_ROOT   app root (default: the directory containing this script)
#   MOCKTAIL_CA_BUNDLE  PEM CA bundle for Roblox downloads (default:
#                       cacert.pem next to the executable)
#   MOCKTAIL_PS4_EGL_LIBRARY / MOCKTAIL_PS4_GLES_LIBRARY
#                       system graphics library names (defaults:
#                       libEGL.so.1 / libGLESv2.so.2)

set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXECUTABLE="${APP_ROOT}/mocktail"

if [[ ! -x "${EXECUTABLE}" ]]; then
  echo "error: ${EXECUTABLE} not found or not executable" >&2
  exit 1
fi

# Default the app root to the launcher's directory unless overridden.
export MOCKTAIL_PS4_ROOT="${MOCKTAIL_PS4_ROOT:-${APP_ROOT}}"

# Point the runtime at the app's data/cache/state/config roots.
export MOCKTAIL_DATA_ROOT="${MOCKTAIL_DATA_ROOT:-${MOCKTAIL_PS4_ROOT}/data}"
export MOCKTAIL_CACHE_ROOT="${MOCKTAIL_CACHE_ROOT:-${MOCKTAIL_PS4_ROOT}/cache}"
export MOCKTAIL_STATE_ROOT="${MOCKTAIL_STATE_ROOT:-${MOCKTAIL_PS4_ROOT}/state}"
export MOCKTAIL_CONFIG_ROOT="${MOCKTAIL_CONFIG_ROOT:-${MOCKTAIL_PS4_ROOT}/config}"
export MOCKTAIL_AUTH_ROOT="${MOCKTAIL_AUTH_ROOT:-${MOCKTAIL_DATA_ROOT}/auth}"

# The guest-facing stub shared libraries live next to the executable.
export LD_LIBRARY_PATH="${APP_ROOT}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# Default the TLS CA bundle to cacert.pem next to the executable.
if [[ -z "${MOCKTAIL_CA_BUNDLE:-}" && -f "${APP_ROOT}/cacert.pem" ]]; then
  export MOCKTAIL_CA_BUNDLE="${APP_ROOT}/cacert.pem"
fi

# The PS4 system graphics libraries are resolved by the EGL/GLES forwarding
# stubs through dlopen; no extra library path is required.

exec "${EXECUTABLE}" "$@"
