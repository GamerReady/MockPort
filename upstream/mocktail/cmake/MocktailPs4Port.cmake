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

# PS4 (Orbis OS, jailbroken) build of Mocktail.
#
# This module replaces the Linux build entirely when MOCKTAIL_PS4_PORT=ON.
# It mirrors the upstream target structure so the main executable and all
# internal dependencies line up, with PS4 substitutions for:
#   - HTTP:     libcurl          -> PS4 socket/mbedTLS transport
#   - TLS/crypto: OpenSSL       -> vendored Mbed TLS (via the OpenSSL shim)
#   - Graphics: ANGLE/Vulkan    -> system EGL/GLES (forwarding stubs)
#   - Platform: SDL desktop runtime -> PS4 fullscreen SDL3 runtime
#   - Linux-only helpers (GTK/WebKit/GIO/Discord sockets) -> PS4 stubs
#
# The `compat/` include directory is placed BEFORE the system include paths so
# that <openssl/...> and <jsc/jsc.h> resolve to the PS4 shim headers.

include_guard(GLOBAL)

set(MOCKTAIL_PS4_PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(MOCKTAIL_PS4_PORT_ROOT "${MOCKTAIL_PS4_PORT_ROOT}" ABSOLUTE)

if(NOT DEFINED MOCKTAIL_PS4_PORT)
  message(FATAL_ERROR "MocktailPs4Port.cmake requires MOCKTAIL_PS4_PORT=ON")
endif()

message(STATUS "Mocktail: building the PS4 (Orbis OS) port")

# ---------------------------------------------------------------------------
# Compiler setup
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mocktail_compile_options INTERFACE)
target_compile_options(mocktail_compile_options INTERFACE
  "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Wno-unused-parameter;-fno-exceptions;-g>"
)

function(mocktail_apply_compile_options target_name)
  target_link_libraries(${target_name} PRIVATE mocktail_compile_options)
endfunction()

# Common PS4 include directories. `compat` must precede the system paths so
# the OpenSSL/JSC shim headers win.
set(MOCKTAIL_PS4_COMMON_INCLUDES
  ${MOCKTAIL_PS4_PORT_ROOT}/compat
  ${MOCKTAIL_PS4_PORT_ROOT}/include
  ${MOCKTAIL_PS4_PORT_ROOT}/src
)

function(mocktail_ps4_apply_includes target_name)
  target_include_directories(${target_name} BEFORE PUBLIC ${MOCKTAIL_PS4_COMMON_INCLUDES})
endfunction()

# Shared helper for guest-facing stub shared libraries.
function(configure_stub_helper TARGET SONAME)
  string(REGEX REPLACE "^lib" "" _base "${SONAME}")
  string(REGEX REPLACE "\\.so$" "" _base "${_base}")
  set_target_properties(${TARGET} PROPERTIES
    OUTPUT_NAME              "${_base}"
    PREFIX                   "lib"
    SUFFIX                   ".so"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
  target_link_options(${TARGET} PRIVATE "-Wl,-soname,${SONAME}")
  target_compile_options(${TARGET} PRIVATE -Wno-unused-parameter)
endfunction()

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)

# Mbed TLS supplies TLS, X.509, and the crypto primitives behind the OpenSSL
# compatibility shim (APK signature verification, payload digests).
set(MBEDTLS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MBEDTLS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(MBEDTLS_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(MBEDTLS_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
add_subdirectory(${MOCKTAIL_PS4_PORT_ROOT}/third_party/mbedtls
  ${CMAKE_CURRENT_BINARY_DIR}/mbedtls EXCLUDE_FROM_ALL)

# SDL3 (cross-compiled for Orbis OS by the toolchain).
find_package(SDL3 3.4 REQUIRED CONFIG)

find_package(nlohmann_json CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBYAML REQUIRED IMPORTED_TARGET yaml-0.1)
pkg_check_modules(MINIZIP REQUIRED IMPORTED_TARGET minizip)
pkg_check_modules(CAPSTONE REQUIRED IMPORTED_TARGET capstone)
pkg_check_modules(UTF8PROC REQUIRED IMPORTED_TARGET libutf8proc)
pkg_check_modules(FONTCONFIG QUIET IMPORTED_TARGET fontconfig)
pkg_check_modules(LIBSECRET QUIET IMPORTED_TARGET libsecret-1)
find_package(SDL3_ttf REQUIRED CONFIG)

# Vulkan headers are vendored; the PS4 libvulkan.so stub fails init cleanly
# so the client selects the GLES renderer, but the WSI sources still compile.
set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${MOCKTAIL_PS4_PORT_ROOT}/third_party/Vulkan-Headers
  EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Bionic/ELF compatibility (PS4: Mbed TLS instead of OpenSSL)
# ---------------------------------------------------------------------------

include(${MOCKTAIL_PS4_PORT_ROOT}/cmake/MocktailPs4ElfCompatibility.cmake)

# ---------------------------------------------------------------------------
# Vendored subprojects
# ---------------------------------------------------------------------------

option(MOCKTAIL_ENABLE_UPSTREAM_JNIVM
  "Build upstream libjnivm for the incremental JNI runtime migration" ON)
if(MOCKTAIL_ENABLE_UPSTREAM_JNIVM)
  set(JNIVM_ENABLE_TRACE OFF CACHE BOOL "")
  set(JNIVM_ENABLE_GC ON CACHE BOOL "")
  set(JNIVM_ENABLE_DEBUG OFF CACHE BOOL "")
  set(JNIVM_ENABLE_RETURN_NON_ZERO OFF CACHE BOOL "")
  set(JNIVM_FAKE_JNI_MINECRAFT_LINUX_COMPAT OFF CACHE BOOL "")
  set(JNIVM_ENABLE_TESTS OFF CACHE BOOL "")
  set(JNIVM_BUILD_EXAMPLES OFF CACHE BOOL "")
  add_subdirectory(${MOCKTAIL_PS4_PORT_ROOT}/third_party/libjnivm
    EXCLUDE_FROM_ALL)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(jnivm PRIVATE -include string)
  endif()
  target_link_libraries(jnivm PUBLIC Threads::Threads)
  add_library(Mocktail::UpstreamJni ALIAS jnivm)
endif()

add_subdirectory(${MOCKTAIL_PS4_PORT_ROOT}/third_party/mcpelauncher-linker)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(linker PRIVATE -w)
endif()

set(MOCKTAIL_JNI_INCLUDE_DIRS ${MOCKTAIL_PS4_PORT_ROOT}/third_party/jni/include)

# ---------------------------------------------------------------------------
# Graphics foundation (PS4: system EGL/GLES, no ANGLE)
# ---------------------------------------------------------------------------

# The PS4 system ships EGL/GLES headers; ANGLE headers are not used.
add_library(mocktail_platform_sdl STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/platform/ps4/ps4_platform_runtime.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/platform/sdl_application_metadata.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/platform/sdl_display_refresh_capabilities.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/platform/sdl_event_converter.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/platform/sdl_gamepad_manager.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/platform/sdl_text_clipboard.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/platform/sdl_window_icon.cc
)
add_library(Mocktail::PlatformSdl ALIAS mocktail_platform_sdl)
mocktail_ps4_apply_includes(mocktail_platform_sdl)
target_link_libraries(mocktail_platform_sdl PUBLIC SDL3::SDL3)
target_compile_features(mocktail_platform_sdl PUBLIC cxx_std_17)
mocktail_apply_compile_options(mocktail_platform_sdl)

add_library(mocktail_graphics_foundation STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/graphics_backend.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/ps4_angle_probe_stub.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/bionic_egl_bridge.cc
)
add_library(Mocktail::GraphicsFoundation ALIAS mocktail_graphics_foundation)
mocktail_ps4_apply_includes(mocktail_graphics_foundation)
target_compile_features(mocktail_graphics_foundation PUBLIC cxx_std_17)
mocktail_apply_compile_options(mocktail_graphics_foundation)

add_library(mocktail_gles_text_overlay STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/gles_text_overlay_compositor.cc
)
add_library(Mocktail::GlesTextOverlay ALIAS mocktail_gles_text_overlay)
mocktail_ps4_apply_includes(mocktail_gles_text_overlay)
target_link_libraries(mocktail_gles_text_overlay PUBLIC SDL3::SDL3)
target_compile_features(mocktail_gles_text_overlay PUBLIC cxx_std_17)
mocktail_apply_compile_options(mocktail_gles_text_overlay)

# Vulkan WSI sources compile unchanged; the PS4 libvulkan.so stub makes the
# client fall back to the GLES renderer at runtime.
add_library(mocktail_sdl_vulkan_wsi STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/sdl_vulkan_wsi.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/android_vulkan_wsi_adapter.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/present_mode_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/graphics/vulkan_swapchain_policy.cc
)
add_library(Mocktail::SdlVulkanWsi ALIAS mocktail_sdl_vulkan_wsi)
mocktail_ps4_apply_includes(mocktail_sdl_vulkan_wsi)
target_link_libraries(mocktail_sdl_vulkan_wsi PUBLIC
  SDL3::SDL3
  Vulkan::Vulkan
)
target_compile_features(mocktail_sdl_vulkan_wsi PUBLIC cxx_std_17)
mocktail_apply_compile_options(mocktail_sdl_vulkan_wsi)

# ---------------------------------------------------------------------------
# Audio
# ---------------------------------------------------------------------------

add_library(mocktail_audio_core STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/audio_sink.cc
)
add_library(Mocktail::AudioCore ALIAS mocktail_audio_core)
mocktail_ps4_apply_includes(mocktail_audio_core)
target_compile_features(mocktail_audio_core PUBLIC cxx_std_17)
set_target_properties(mocktail_audio_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
mocktail_apply_compile_options(mocktail_audio_core)

add_library(mocktail_audio_sdl SHARED
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/sdl_audio_capture.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/sdl_audio_sink.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/ps4_audio_sink.cc
)
add_library(Mocktail::AudioSdl ALIAS mocktail_audio_sdl)
mocktail_ps4_apply_includes(mocktail_audio_sdl)
target_link_libraries(mocktail_audio_sdl PUBLIC
  Mocktail::AudioCore
  SDL3::SDL3
)
target_compile_features(mocktail_audio_sdl PUBLIC cxx_std_17)
set_target_properties(mocktail_audio_sdl PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  BUILD_RPATH_USE_ORIGIN TRUE
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
)
mocktail_apply_compile_options(mocktail_audio_sdl)

add_library(mocktail_audio_fmod_java_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/fmod_java_audio_runtime.cc
)
add_library(Mocktail::FmodJavaAudioRuntime ALIAS mocktail_audio_fmod_java_runtime)
mocktail_ps4_apply_includes(mocktail_audio_fmod_java_runtime)
target_link_libraries(mocktail_audio_fmod_java_runtime PUBLIC
  Mocktail::AudioSdl
  Threads::Threads
)
target_compile_features(mocktail_audio_fmod_java_runtime PUBLIC cxx_std_17)
set_target_properties(mocktail_audio_fmod_java_runtime PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)
mocktail_apply_compile_options(mocktail_audio_fmod_java_runtime)

add_library(mocktail_fmod_jni_audio_bridge STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/fmod_jni_audio_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/webrtc_jni_audio_bridge.cc
)
add_library(Mocktail::FmodJniAudioBridge ALIAS mocktail_fmod_jni_audio_bridge)
mocktail_ps4_apply_includes(mocktail_fmod_jni_audio_bridge)
target_link_libraries(mocktail_fmod_jni_audio_bridge PUBLIC
  Mocktail::FmodJavaAudioRuntime
  Mocktail::LegacyJni
)
target_compile_features(mocktail_fmod_jni_audio_bridge PUBLIC cxx_std_17)
set_target_properties(mocktail_fmod_jni_audio_bridge PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)
mocktail_apply_compile_options(mocktail_fmod_jni_audio_bridge)

# Guest OpenSL ES audio stub.
add_library(mocktail_opensles SHARED
  ${MOCKTAIL_PS4_PORT_ROOT}/stubs/libopensl_stub.cc
)
configure_stub_helper(mocktail_opensles "libOpenSLES.so")
mocktail_ps4_apply_includes(mocktail_opensles)
mocktail_apply_compile_options(mocktail_opensles)

# ---------------------------------------------------------------------------
# Bionic / linker / compatibility
# ---------------------------------------------------------------------------

add_library(mocktail_bionic_stdio_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/compat/bionic_stdio_runtime.cc
)
add_library(Mocktail::BionicStdio ALIAS mocktail_bionic_stdio_runtime)
mocktail_ps4_apply_includes(mocktail_bionic_stdio_runtime)
mocktail_apply_compile_options(mocktail_bionic_stdio_runtime)

add_library(mocktail_linker STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/linker/linker.cc
)
mocktail_ps4_apply_includes(mocktail_linker)
target_link_libraries(mocktail_linker PUBLIC
  dl
  linker
  mocktail_compat
  mocktail_bionic_stdio_runtime
  mocktail_bionic_abi_exports
)
mocktail_apply_compile_options(mocktail_linker)
target_compile_definitions(mocktail_linker PUBLIC MOCKTAIL_USE_BIONIC_LINKER=1)
target_link_options(mocktail_linker INTERFACE
  -Wl,--defsym=__rela_iplt_start=0
  -Wl,--defsym=__rela_iplt_end=0
)

add_library(libc_shim STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/libc_shim/libc_shim.cc
)
mocktail_ps4_apply_includes(libc_shim)
mocktail_apply_compile_options(libc_shim)
target_link_libraries(mocktail_linker PUBLIC libc_shim)

# ---------------------------------------------------------------------------
# Update (APK download/signature/integrity)
# ---------------------------------------------------------------------------

add_library(mocktail_update STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/android_manifest.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/apk_bundle.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/apk_provider.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/apk_signature.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/apkpure_provider.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/candidate_approval.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/compatibility_catalog.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/ps4_http_download.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/host_abi_deriver.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/payload_integrity.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/payload_store.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/pinned_apk_provider.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/readiness_canary.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/state_file.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/unsafe_latest_runner.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/update_config.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/update_coordinator.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/update/zip_archive.cc
)
add_library(Mocktail::Update ALIAS mocktail_update)
mocktail_ps4_apply_includes(mocktail_update)
target_link_libraries(mocktail_update PUBLIC
  Mocktail::Compat
  mbedtls
  PkgConfig::LIBYAML
  PkgConfig::MINIZIP
  PkgConfig::CAPSTONE
  nlohmann_json::nlohmann_json
  Threads::Threads
)
mocktail_apply_compile_options(mocktail_update)

add_executable(mocktail_updater ${MOCKTAIL_PS4_PORT_ROOT}/src/update/main.cc)
mocktail_ps4_apply_includes(mocktail_updater)
target_link_libraries(mocktail_updater PRIVATE Mocktail::Update)
target_compile_definitions(mocktail_updater PRIVATE
  "MOCKTAIL_DEFAULT_COMPATIBILITY_MANIFEST=\"${MOCKTAIL_DEFAULT_COMPATIBILITY_MANIFEST}\""
  "MOCKTAIL_DEFAULT_SIGNING_TRUST_MANIFEST=\"${MOCKTAIL_DEFAULT_SIGNING_TRUST_MANIFEST}\""
  "MOCKTAIL_DEFAULT_DOWNLOAD_SOURCES_MANIFEST=\"${MOCKTAIL_DEFAULT_DOWNLOAD_SOURCES_MANIFEST}\""
)
mocktail_apply_compile_options(mocktail_updater)

# ---------------------------------------------------------------------------
# Runtime
# ---------------------------------------------------------------------------

add_library(mocktail_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/command_line.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/crash_report_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/ps4_discord_rpc.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/device_memory_profile.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/device_profile.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/environment.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/external_launch_broker.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/frame_rate_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/ps4_fleasion_stub.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/game_mode.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/game_session_coordinator.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/graphics_launch_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/http_client_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/memory_limit.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/owned_pthread.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/performance_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/platform_cache_migration.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_desktop_app_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_app_lifecycle.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_browser_service_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_capability_resolver.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_experience_launch_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_launch_uri.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_presence_resolver.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_cookie_store.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_game_session_native_adapter.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_web_view_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_permissions_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_message_bus_request_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_call_protocol_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/payload_update_preflight.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/process_launch_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/process_diagnostics.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/runtime_config.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/runtime_config_bootstrap.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/runtime_config_file.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/runtime_paths.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/session_log.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/single_instance_lock.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/startup_state_machine.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/supported_launch_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/support_bundle.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/ps4_system_proxy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/texture_memory_policy.cc
)
add_library(Mocktail::Runtime ALIAS mocktail_runtime)
mocktail_ps4_apply_includes(mocktail_runtime)
target_include_directories(mocktail_runtime PUBLIC ${MOCKTAIL_JNI_INCLUDE_DIRS})
target_compile_definitions(mocktail_runtime PRIVATE
  "MOCKTAIL_DISCORD_APPLICATION_ID=\"${MOCKTAIL_DISCORD_APPLICATION_ID}\""
  "MOCKTAIL_INSTALL_LIBDIR=\"${CMAKE_INSTALL_LIBDIR}\""
  "MOCKTAIL_PS4_PORT=1"
)
set_property(SOURCE ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/session_log.cc APPEND PROPERTY
  COMPILE_DEFINITIONS
  "MOCKTAIL_PROJECT_VERSION=\"${PROJECT_VERSION}\""
  "MOCKTAIL_BUILD_GIT_COMMIT=\"${MOCKTAIL_BUILD_GIT_COMMIT}\""
  "MOCKTAIL_BUILD_GIT_DIRTY=\"${MOCKTAIL_BUILD_GIT_DIRTY}\""
  "MOCKTAIL_BUILD_TYPE=\"$<CONFIG>\""
  "MOCKTAIL_BUILD_COMPILER_ID=\"${CMAKE_CXX_COMPILER_ID}\""
  "MOCKTAIL_BUILD_COMPILER_VERSION=\"${CMAKE_CXX_COMPILER_VERSION}\""
  "MOCKTAIL_BUILD_TARGET_SYSTEM=\"${CMAKE_SYSTEM_NAME}\""
  "MOCKTAIL_BUILD_TARGET_PROCESSOR=\"${CMAKE_SYSTEM_PROCESSOR}\""
)
mocktail_apply_compile_options(mocktail_runtime)
target_link_libraries(mocktail_runtime PUBLIC Threads::Threads)
target_link_libraries(mocktail_runtime PRIVATE
  mbedtls
  nlohmann_json::nlohmann_json
  PkgConfig::LIBYAML
  ${CMAKE_DL_LIBS}
)

# Failure dialog (PS4: console output).
add_library(mocktail_failure_dialog_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/ui/ps4_failure_dialog.cc
)
add_library(Mocktail::FailureDialog ALIAS mocktail_failure_dialog_runtime)
mocktail_ps4_apply_includes(mocktail_failure_dialog_runtime)
target_compile_definitions(mocktail_failure_dialog_runtime PRIVATE
  "MOCKTAIL_INSTALL_LIBDIR=\"${CMAKE_INSTALL_LIBDIR}\""
)
mocktail_apply_compile_options(mocktail_failure_dialog_runtime)

# Cookie helper (PS4: no libsecret; private plaintext cookie files).
add_executable(mocktail_cookie_helper
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/mocktail_cookie_helper.cc
)
mocktail_ps4_apply_includes(mocktail_cookie_helper)
if(LIBSECRET_FOUND)
  target_link_libraries(mocktail_cookie_helper PRIVATE PkgConfig::LIBSECRET)
  target_compile_definitions(mocktail_cookie_helper PRIVATE MOCKTAIL_HAVE_LIBSECRET=1)
else()
  message(WARNING "libsecret-1 headers missing: Roblox cookies will use private plaintext files")
endif()
mocktail_apply_compile_options(mocktail_cookie_helper)

# ---------------------------------------------------------------------------
# Services (PS4 socket HTTP client)
# ---------------------------------------------------------------------------

add_library(mocktail_services STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/services/socket_http_client.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/services/auth_service.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/services/browser_tracker_service.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/services/client_settings_service.cc
)
add_library(Mocktail::Services ALIAS mocktail_services)
mocktail_ps4_apply_includes(mocktail_services)
target_link_libraries(mocktail_services PUBLIC
  mocktail_runtime
  mbedtls
  nlohmann_json::nlohmann_json
)
mocktail_apply_compile_options(mocktail_services)

# WebView launcher (PS4: codecs preserved, launcher reports unavailable).
add_library(mocktail_webview_launcher STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/ps4_webview_helper_launcher.cc
)
add_library(Mocktail::WebViewLauncher ALIAS mocktail_webview_launcher)
mocktail_ps4_apply_includes(mocktail_webview_launcher)
target_link_libraries(mocktail_webview_launcher PUBLIC
  mocktail_runtime
  mbedtls
)
mocktail_apply_compile_options(mocktail_webview_launcher)

# WebView helper policy (PS4: no GLib/JSC).
add_library(mocktail_webview_helper_policy STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/webview/ps4_webview_helper_policy.cc
)
add_library(Mocktail::WebViewHelperPolicy ALIAS mocktail_webview_helper_policy)
mocktail_ps4_apply_includes(mocktail_webview_helper_policy)
target_link_libraries(mocktail_webview_helper_policy PUBLIC
  mbedtls
  nlohmann_json::nlohmann_json
)
mocktail_apply_compile_options(mocktail_webview_helper_policy)

# The webview helper binary itself (PS4 stub).
add_executable(mocktail_webview_helper
  ${MOCKTAIL_PS4_PORT_ROOT}/src/webview/ps4_mocktail_webview_helper.cc
)
mocktail_ps4_apply_includes(mocktail_webview_helper)
mocktail_apply_compile_options(mocktail_webview_helper)

# ---------------------------------------------------------------------------
# Auth / experience / legacy composition
# ---------------------------------------------------------------------------

add_library(mocktail_auth_runtime_composition STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/auth_runtime_composition.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/webview_roblox_cookie.cc
)
add_library(Mocktail::AuthRuntimeComposition ALIAS mocktail_auth_runtime_composition)
mocktail_ps4_apply_includes(mocktail_auth_runtime_composition)
target_link_libraries(mocktail_auth_runtime_composition PUBLIC
  mocktail_legacy_jnivm
  mocktail_runtime
  mocktail_services
)
mocktail_apply_compile_options(mocktail_auth_runtime_composition)

# Input runtime (SDL3 gamepad + text).
add_library(mocktail_input_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_gamepad_input.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_input_native_adapter.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_input_router.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_native_text_box_info_reader.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_text_display_state.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_text_editor.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_text_font_resolver.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_text_surface_overlay.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_window_input_runtime.cc
)
add_library(Mocktail::InputRuntime ALIAS mocktail_input_runtime)
mocktail_ps4_apply_includes(mocktail_input_runtime)
target_link_libraries(mocktail_input_runtime PUBLIC
  Mocktail::PlatformSdl
  Mocktail::Runtime
  mocktail_window
  PkgConfig::UTF8PROC
  PRIVATE
    PkgConfig::FONTCONFIG
    SDL3_ttf::SDL3_ttf
    nlohmann_json::nlohmann_json
)
target_compile_features(mocktail_input_runtime PUBLIC cxx_std_17)
mocktail_apply_compile_options(mocktail_input_runtime)

# Window.
add_library(mocktail_window STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/roblox_fullscreen_menu_request_gate.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/video_driver_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_creation_policy.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_fullscreen_request_gate.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_game_surface_bridge.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_pointer_capture_owner.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_resize_readiness_gate.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_state_store.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_surface_lifecycle.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/window/window_text_input_owner.cc
)
mocktail_ps4_apply_includes(mocktail_window)
mocktail_apply_compile_options(mocktail_window)
target_link_libraries(mocktail_window PUBLIC
  Mocktail::PlatformSdl
  Mocktail::SdlVulkanWsi
  SDL3::SDL3
  ${CMAKE_DL_LIBS}
  PRIVATE Mocktail::Runtime Mocktail::GlesTextOverlay Mocktail::GraphicsFoundation
)
target_compile_definitions(mocktail_window PRIVATE MOCKTAIL_PS4_PORT=1)

# Bionic ABI exports.
add_library(mocktail_bionic_abi_exports STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/compat/bionic_abi_exports.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/compat/http_client_spin_guard.cc
)
mocktail_ps4_apply_includes(mocktail_bionic_abi_exports)
mocktail_apply_compile_options(mocktail_bionic_abi_exports)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_property(SOURCE ${MOCKTAIL_PS4_PORT_ROOT}/src/compat/bionic_abi_exports.cc APPEND PROPERTY
    COMPILE_OPTIONS
      -fno-lto
      -fno-builtin
  )
endif()
target_link_libraries(mocktail_bionic_abi_exports PRIVATE
  mocktail_bionic_stdio_runtime
  mocktail_window
  Threads::Threads
)

add_library(mocktail_legacy_jnivm STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/jnivm/jnivm.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/jnivm/jni_references.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/jnivm/jni_fields.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/jnivm/jni_strings.cc
)
add_library(Mocktail::LegacyJni ALIAS mocktail_legacy_jnivm)
mocktail_ps4_apply_includes(mocktail_legacy_jnivm)
target_include_directories(mocktail_legacy_jnivm PUBLIC ${MOCKTAIL_JNI_INCLUDE_DIRS})
mocktail_apply_compile_options(mocktail_legacy_jnivm)

# Experience runtime.
add_library(mocktail_experience_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/jnivm_platform_web_callbacks.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_experience_composition.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_fresh_game_launch_controller.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_platform_web_symbols.cc
)
add_library(Mocktail::ExperienceRuntime ALIAS mocktail_experience_runtime)
mocktail_ps4_apply_includes(mocktail_experience_runtime)
target_link_libraries(mocktail_experience_runtime PUBLIC
  Mocktail::Runtime
  Mocktail::WebViewLauncher
  PkgConfig::UTF8PROC
  PRIVATE
    Mocktail::AuthRuntimeComposition
    Mocktail::LegacyJni
    mocktail_linker
    mocktail_window
)
mocktail_apply_compile_options(mocktail_experience_runtime)

# Legacy runtime (Android client load path).
add_library(mocktail_legacy_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/legacy/legacy_runtime.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_text_input_jni_bridge.cc
)
mocktail_ps4_apply_includes(mocktail_legacy_runtime)
mocktail_apply_compile_options(mocktail_legacy_runtime)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_property(SOURCE ${MOCKTAIL_PS4_PORT_ROOT}/src/legacy/legacy_runtime.cc APPEND PROPERTY
    COMPILE_OPTIONS -Wno-unused-result
  )
endif()
target_link_libraries(mocktail_legacy_runtime PRIVATE
  mocktail_legacy_jnivm
  mocktail_compat
  mocktail_runtime
  mocktail_input_runtime
  mocktail_experience_runtime
  mocktail_services
  mocktail_linker
  mocktail_bionic_abi_exports
  libc_shim
  mocktail_window
  mocktail_graphics_foundation
)
target_compile_definitions(mocktail_legacy_runtime PRIVATE
  "MOCKTAIL_DEFAULT_COMPATIBILITY_MANIFEST=\"${MOCKTAIL_DEFAULT_COMPATIBILITY_MANIFEST}\""
  "MOCKTAIL_PS4_PORT=1"
)

# ---------------------------------------------------------------------------
# Stub shared libraries (guest-facing)
# ---------------------------------------------------------------------------

foreach(_pair
  "libm.so|m"
  "libz.so|z"
  "libdl.so|dl"
)
  string(REPLACE "|" ";" _parts "${_pair}")
  list(GET _parts 0 _soname)
  list(GET _parts 1 _link)
  string(REGEX REPLACE "[^a-z0-9]" "_" _target "stub_sys_${_link}")
  add_library(${_target} SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/empty.cc)
  configure_stub_helper(${_target} ${_soname})
  target_link_options(${_target} PRIVATE "LINKER:--no-as-needed")
  target_link_libraries(${_target} PRIVATE -l${_link})
endforeach()

add_library(stub_sys_c SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/empty.cc)
configure_stub_helper(stub_sys_c "libc.so")
target_link_options(stub_sys_c PRIVATE "LINKER:--no-as-needed")

add_library(stub_android SHARED
  ${MOCKTAIL_PS4_PORT_ROOT}/stubs/libandroid_stub.cc
  ${MOCKTAIL_PS4_PORT_ROOT}/src/libc_shim/vulkan_etc1_sky_transcoder.cc
)
configure_stub_helper(stub_android "libandroid.so")
mocktail_ps4_apply_includes(stub_android)
target_link_libraries(stub_android PRIVATE
  PkgConfig::MINIZIP
  Threads::Threads
  ${CMAKE_DL_LIBS}
)
mocktail_apply_compile_options(stub_android)

add_library(stub_log SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/liblog_stub.cc)
configure_stub_helper(stub_log "liblog.so")
mocktail_ps4_apply_includes(stub_log)
mocktail_apply_compile_options(stub_log)

add_library(stub_mediandk SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/libmediandk_stub.cc)
configure_stub_helper(stub_mediandk "libmediandk.so")
mocktail_apply_compile_options(stub_mediandk)

add_library(stub_openmax SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/libopensl_stub.cc)
configure_stub_helper(stub_openmax "libOpenMAXAL.so")
mocktail_apply_compile_options(stub_openmax)

# PS4 graphics stubs: forward to the system EGL/GLES libraries, and make
# Vulkan init fail cleanly so the client selects the GLES renderer.
add_library(stub_egl SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/ps4_egl_forward_stub.cc)
configure_stub_helper(stub_egl "libEGL.so")
mocktail_ps4_apply_includes(stub_egl)
target_link_libraries(stub_egl PRIVATE dl)
mocktail_apply_compile_options(stub_egl)

add_library(stub_glesv2 SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/ps4_gles_forward_stub.cc)
configure_stub_helper(stub_glesv2 "libGLESv2.so")
mocktail_ps4_apply_includes(stub_glesv2)
target_link_libraries(stub_glesv2 PRIVATE dl)
mocktail_apply_compile_options(stub_glesv2)

add_library(stub_vulkan SHARED ${MOCKTAIL_PS4_PORT_ROOT}/stubs/ps4_vulkan_stub.cc)
configure_stub_helper(stub_vulkan "libvulkan.so")
mocktail_ps4_apply_includes(stub_vulkan)
mocktail_apply_compile_options(stub_vulkan)

add_custom_target(all_stubs DEPENDS
  stub_sys_m stub_sys_z stub_sys_dl stub_sys_c
  stub_android stub_log stub_mediandk mocktail_opensles stub_openmax
  stub_egl stub_glesv2 stub_vulkan
)

# ---------------------------------------------------------------------------
# Roblox output device / fullscreen bridges
# ---------------------------------------------------------------------------

add_library(mocktail_roblox_output_device_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/audio/roblox_output_device_bridge.cc
)
mocktail_ps4_apply_includes(mocktail_roblox_output_device_runtime)
target_link_libraries(mocktail_roblox_output_device_runtime PUBLIC
  mocktail_compat
  Mocktail::AudioSdl
  PRIVATE mocktail_linker
)
mocktail_apply_compile_options(mocktail_roblox_output_device_runtime)

add_library(mocktail_roblox_fullscreen_runtime STATIC
  ${MOCKTAIL_PS4_PORT_ROOT}/src/runtime/roblox_fullscreen_runtime_bridge.cc
)
mocktail_ps4_apply_includes(mocktail_roblox_fullscreen_runtime)
target_link_libraries(mocktail_roblox_fullscreen_runtime PUBLIC
  mocktail_compat
  PRIVATE
    mocktail_linker
    mocktail_window
    stub_log
)
mocktail_apply_compile_options(mocktail_roblox_fullscreen_runtime)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------

add_executable(mocktail ${MOCKTAIL_PS4_PORT_ROOT}/src/main.cc)
mocktail_ps4_apply_includes(mocktail)
mocktail_apply_compile_options(mocktail)
target_compile_definitions(mocktail PRIVATE
  "MOCKTAIL_DEFAULT_COMPATIBILITY_MANIFEST=\"${MOCKTAIL_DEFAULT_COMPATIBILITY_MANIFEST}\""
  "MOCKTAIL_PS4_PORT=1"
)
target_link_libraries(mocktail PRIVATE
  mocktail_legacy_runtime
  mocktail_auth_runtime_composition
  mocktail_failure_dialog_runtime
  mocktail_fmod_jni_audio_bridge
  mocktail_roblox_output_device_runtime
  mocktail_roblox_fullscreen_runtime
  mocktail_window
  libc_shim
)
target_link_options(mocktail PRIVATE -Wl,--export-dynamic)

set_target_properties(mocktail PROPERTIES
  BUILD_RPATH   "$ORIGIN"
  INSTALL_RPATH "$ORIGIN;$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/mocktail"
)

# ---------------------------------------------------------------------------
# Install (PS4 application layout)
# ---------------------------------------------------------------------------

set(MOCKTAIL_INSTALL_RUNTIME_DIR "${CMAKE_INSTALL_LIBDIR}/mocktail")
set(MOCKTAIL_INSTALL_DATA_DIR "${CMAKE_INSTALL_DATADIR}/mocktail")

install(TARGETS mocktail RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS
    mocktail
    mocktail_updater
    mocktail_webview_helper
    mocktail_cookie_helper
    mocktail_audio_sdl
    mocktail_opensles
    stub_sys_m
    stub_sys_z
    stub_sys_dl
    stub_sys_c
    stub_android
    stub_log
    stub_mediandk
    stub_openmax
    stub_egl
    stub_glesv2
    stub_vulkan
  RUNTIME DESTINATION "${MOCKTAIL_INSTALL_RUNTIME_DIR}"
  LIBRARY DESTINATION "${MOCKTAIL_INSTALL_RUNTIME_DIR}"
)
install(FILES
    config/roblox_compatibility.json
    config/roblox_bootstrap_sources.json
    config/roblox_download_sources.json
    config/roblox_host_abi_reference.json
    config/roblox_signing_certificates.json
  DESTINATION "${MOCKTAIL_INSTALL_DATA_DIR}/metadata"
)

add_dependencies(mocktail all_stubs)
add_dependencies(mocktail mocktail_failure_dialog_runtime)
add_dependencies(mocktail mocktail_webview_helper mocktail_cookie_helper)
