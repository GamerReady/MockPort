# MockPort — PS4 (jailbroken Orbis OS) port

This directory tree is a native port of [mocktail](https://github.com/komaruworld/mocktail)
(the Android Roblox client compatibility runtime) targeting the **PS4 (Orbis
OS, jailbroken)**. The goal is a native PS4 application that launches, runs
the first-time setup (download + signature verification of the Roblox APK),
and starts Roblox ready to play.

The PS4 is an AMD Jaguar x86_64 console running Orbis OS (a FreeBSD-derived
system). The port runs the official Android Roblox client inside mocktail's
Bionic/JNI runtime, with graphics forwarded to the console's system
OpenGL ES stack.

## What the port replaces

| Upstream (Linux)        | PS4 port                                            |
|-------------------------|-----------------------------------------------------|
| libcurl HTTP client     | PS4 socket + Mbed TLS HTTP/1.1 transport             |
| OpenSSL (crypto/TLS)    | Vendored Mbed TLS v3.6.2 + OpenSSL EVP shim         |
| ANGLE / desktop Vulkan  | System EGL/GLES forwarding stubs; Vulkan stub fails init |
| Desktop SDL runtime     | PS4 fullscreen SDL3 runtime (1920×1080)             |
| GTK/WebKit webview      | PS4 stubs (webview unavailable on console)          |
| GIO system proxy        | Env-based proxy only                               |
| Discord Unix socket RPC | Disabled (no Discord client on PS4)                |

## Layout

- `include/ps4/ps4_platform_runtime.h`, `src/platform/ps4/ps4_platform_runtime.cc`
  — PS4 fullscreen SDL3 platform runtime (window, events, DS4 gamepad).
- `include/ps4/ps4_http_transport.h`, `src/ps4/ps4_http_transport.cc`
  — Low-level HTTP/1.1 client: sockets + Mbed TLS, redirects, CONNECT proxy,
  chunked/CL/close bodies.
- `include/services/socket_http_client.h`, `src/services/socket_http_client.cc`
  — `HttpClient` implementation over the PS4 transport.
- `src/update/ps4_http_download.cc`
  — curl-free update downloader: resume, progress ("P" protocol on the
  progress fd, once per second), retries, SHA-256 digests.
- `include/ps4/ps4_openssl_compat.h`, `src/ps4/ps4_openssl_compat.cc`,
  `compat/openssl/*.h`
  — OpenSSL EVP shim on Mbed TLS (digests, RSA-PSS/ECDSA APK signature
  verification, SPKI/X.509 parsing) so `apk_signature.cc` compiles unchanged.
- `stubs/ps4_egl_forward_stub.cc`, `stubs/ps4_gles_forward_stub.cc`,
  `stubs/ps4_vulkan_stub.cc`
  — Guest-facing `libEGL.so` / `libGLESv2.so` forward to the system
  `libEGL.so.1` / `libGLESv2.so.2`; `libvulkan.so` fails init so the client
  selects the GLES renderer.
- `include/mocktail/audio/ps4_audio_sink.h`, `src/audio/ps4_audio_sink.cc`
  — SDL audio sink with a silent fallback.
- `src/runtime/ps4_*.cc`, `src/ui/ps4_failure_dialog.cc`,
  `src/webview/ps4_*.cc`, `src/graphics/ps4_angle_probe_stub.cc`
  — PS4 replacements for Linux-only components (Fleasion, Discord RPC,
  system proxy, webview launcher/policy/helper, failure dialog, ANGLE probe).
- `third_party/mbedtls/` — vendored Mbed TLS v3.6.2.
- `cmake/MocktailPs4Port.cmake`, `cmake/MocktailPs4ElfCompatibility.cmake`,
  `cmake/ps4/ps4-toolchain.cmake`
  — PS4 build: mirrors the upstream target structure with PS4 substitutions.
- `scripts/ps4/build_ps4.sh`, `scripts/ps4/run_ps4.sh`
  — Cross-build script and on-console launcher.

## Building

Prerequisites: a PS4 (Orbis OS) SDK providing an Orbis-targeting clang and
sysroot, plus SDL3, nlohmann_json, libyaml, minizip, capstone, libutf8proc,
fontconfig/freetype, and libelf cross-built for Orbis OS.

```sh
PS4_SDK_ROOT=/opt/ps4-sdk ./scripts/ps4/build_ps4.sh
```

or directly:

```sh
cmake -B build-ps4 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/ps4/ps4-toolchain.cmake \
  -DPSG4_SDK_ROOT=/opt/ps4-sdk \
  -DMOCKTAIL_PS4_PORT=ON
cmake --build build-ps4 -j
```

## Installing and running on a jailbroken PS4

Install the app directory (e.g. `/data/mocktail`) with the executable, the
`lib*.so` stubs, and `config/metadata/*.json`. Then run:

```sh
./run_ps4.sh
```

The launcher sets `MOCKTAIL_PS4_ROOT` (default: its own directory), the
`MOCKTAIL_*_ROOT` path variables, `LD_LIBRARY_PATH`, and the TLS CA bundle
(`cacert.pem` next to the executable, overridable with `MOCKTAIL_CA_BUNDLE`).

## Runtime notes

- **Graphics**: the guest `libEGL.so`/`libGLESv2.so` stubs `dlopen` the PS4
  system libraries (`MOCKTAIL_PS4_EGL_LIBRARY`, default `libEGL.so.1`;
  `MOCKTAIL_PS4_GLES_LIBRARY`, default `libGLESv2.so.2`) and forward calls,
  so GLES rendering runs on the console GPU. `libvulkan.so` fails
  `vkCreateInstance` with `VK_ERROR_INITIALIZATION_FAILED`, so the client
  falls back to the GLES renderer.
- **Audio**: SDL audio is tried first; if unavailable, a silent sink keeps
  the audio pipeline alive.
- **Updates**: downloads use the PS4 transport with TLS verification against
  the bundled CA bundle; the Roblox APK is verified with the Mbed TLS-backed
  OpenSSL shim (RSA-PSS/ECDSA, APK Signature Scheme v2/v3).
- **Fleasion** (TLS interception) and **Discord RPC** are unavailable on
  PS4 and report clean failures if explicitly enabled.
