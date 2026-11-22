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

# Cross-compile toolchain for the PS4 (Orbis OS) port.
#
# Usage:
#   cmake -B build-ps4 \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/ps4/ps4-toolchain.cmake \
#     -DPSG4_SDK_ROOT=/path/to/ps4/sdk \
#     -DMOCKTAIL_PS4_PORT=ON
#
# The PS4 SDK (Orbis OS SDK, e.g. the PS4-Orbis-SDK or a PSDev SDK
# distribution) provides a clang that targets Orbis OS together with the
# Orbis OS sysroot (system headers and libraries). Point PS4_SDK_ROOT at the
# SDK root; the toolchain derives the compiler and sysroot from it.

set(CMAKE_SYSTEM_NAME Orbis)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# The PS4 SDK ships its own clang/clang++ driver. Allow an explicit override;
# otherwise derive it from the SDK root.
if(NOT PS4_C_COMPILER)
  if(PS4_SDK_ROOT AND EXISTS "${PS4_SDK_ROOT}/bin/clang")
    set(PS4_C_COMPILER "${PS4_SDK_ROOT}/bin/clang")
  else()
    find_program(PS4_C_COMPILER NAMES clang clang-19 clang-18 clang-17)
  endif()
endif()
if(NOT PS4_CXX_COMPILER)
  if(PS4_SDK_ROOT AND EXISTS "${PS4_SDK_ROOT}/bin/clang++")
    set(PS4_CXX_COMPILER "${PS4_SDK_ROOT}/bin/clang++")
  else()
    find_program(PS4_CXX_COMPILER NAMES clang++ clang++-19 clang++-18 clang++-17)
  endif()
endif()

if(NOT PS4_C_COMPILER OR NOT PS4_CXX_COMPILER)
  message(FATAL_ERROR
    "No PS4 cross-compiler found. Set PS4_SDK_ROOT to the PS4 SDK root or "
    "provide PS4_C_COMPILER/PS4_CXX_COMPILER.")
endif()

set(CMAKE_C_COMPILER ${PS4_C_COMPILER})
set(CMAKE_CXX_COMPILER ${PS4_CXX_COMPILER})

# Orbis OS sysroot. The SDK's target triple is x86_64-unknown-orbis (or
# x86_64-pc-orbis); pass it through so clang selects the right builtins.
if(PS4_SDK_ROOT)
  get_filename_component(PS4_SDK_ROOT "${PS4_SDK_ROOT}" ABSOLUTE)
  if(NOT PS4_SYSROOT)
    if(EXISTS "${PS4_SDK_ROOT}/target/x86_64-unknown-orbis/sysroot")
      set(PS4_SYSROOT "${PS4_SDK_ROOT}/target/x86_64-unknown-orbis/sysroot")
    elseif(EXISTS "${PS4_SDK_ROOT}/sysroot")
      set(PS4_SYSROOT "${PS4_SDK_ROOT}/sysroot")
    endif()
  endif()
  set(CMAKE_SYSROOT "${PS4_SYSROOT}" CACHE PATH "Orbis OS sysroot")
  set(CMAKE_FIND_ROOT_PATH "${PS4_SYSROOT}" CACHE PATH "PS4 sysroot")
endif()

# Never search the host for programs; only the sysroot for libraries and
# headers.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Orbis OS is a FreeBSD-derived system.
set(CMAKE_OSX_ARCHITECTURES "")

# The PS4 executable format is the same ELF x86_64 as Linux; no special
# linker flags are required beyond the SDK driver defaults.
