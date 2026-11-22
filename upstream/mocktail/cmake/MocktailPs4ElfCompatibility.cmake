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

# PS4 variant of MocktailElfCompatibility.cmake: same bionic/ELF compatibility
# sources, but crypto comes from the vendored Mbed TLS (through the OpenSSL
# shim) instead of OpenSSL.

include_guard(GLOBAL)

get_filename_component(MOCKTAIL_PS4_ELF_COMPAT_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBELF REQUIRED IMPORTED_TARGET libelf)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(mocktail_compat STATIC
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_atfork_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_dns_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_host_libc_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_large_file_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_prctl_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_rwlock_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_signal_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_socket_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_semaphore_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_pthread_create_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_pthread_key_runtime.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/bionic_sysconf.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/elf_build_id.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/build_profile.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/payload_compatibility.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/host_allocator_bridge.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/host_abi_experiment.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/host_abi_profile.cc
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/src/compat/host_abi_profile_loader.cc
)
add_library(Mocktail::Compat ALIAS mocktail_compat)
target_include_directories(mocktail_compat BEFORE PUBLIC
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/compat
  ${MOCKTAIL_PS4_ELF_COMPAT_ROOT}/include
)
target_compile_definitions(mocktail_compat PRIVATE
  "MOCKTAIL_INSTALL_LIBDIR=\"${CMAKE_INSTALL_LIBDIR}\""
)
target_link_libraries(mocktail_compat PUBLIC
  PkgConfig::LIBELF
  nlohmann_json::nlohmann_json
  Threads::Threads
)
target_link_libraries(mocktail_compat PRIVATE mbedtls)
target_compile_features(mocktail_compat PUBLIC cxx_std_17)

if(COMMAND mocktail_apply_compile_options)
  mocktail_apply_compile_options(mocktail_compat)
endif()
