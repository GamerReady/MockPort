// Copyright 2026 MockPort Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// PS4 build of the guest-facing libEGL.so stub.
//
// The Android Roblox client links libEGL.so and drives its GLES renderer
// through it. The jailbroken PS4 has no Vulkan but ships a real system EGL
// (libEGL.so.1) backed by the console GPU. This stub is a forwarding shim:
// it dlopens the PS4 system EGL and forwards every entry point the guest may
// reference, so guest EGL calls land on the console's real EGL implementation.
//
// The PS4 system library path is resolved through MOCKTAIL_PS4_EGL_LIBRARY
// (default "libEGL.so.1"). If the system library cannot be loaded, every
// entry point reports EGL_FALSE / EGL_NO_DISPLAY so the client degrades
// cleanly instead of crashing.

#include <dlfcn.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using EGLBoolean = unsigned int;
using EGLint = int32_t;
using EGLenum = uint32_t;
using EGLDisplay = void*;
using EGLConfig = void*;
using EGLContext = void*;
using EGLSurface = void*;
using EGLNativeDisplayType = void*;
using EGLNativeWindowType = void*;
using EGLNativePbufferType = void*;
using __eglMustCastFP = void (*)();

namespace {

constexpr EGLBoolean kEglFalse = 0;
constexpr EGLDisplay kEglNoDisplay = nullptr;
constexpr EGLContext kEglNoContext = nullptr;
constexpr EGLSurface kEglNoSurface = nullptr;
constexpr EGLint kEglNotInitialized = 0x3001;

void* g_system_egl = nullptr;

const char* SystemEglLibrary() {
  const char* override = std::getenv("MOCKTAIL_PS4_EGL_LIBRARY");
  if (override != nullptr && override[0] != '\0') {
    return override;
  }
  return "libEGL.so.1";
}

// Resolves one entry point from the PS4 system EGL. When the system library
// is missing or does not export the symbol, the provided fallback stub is
// used so the guest's EGL initialization fails cleanly instead of crashing.
template <typename Fn>
Fn ResolveEntry(const char* name, Fn fallback_stub) {
  if (g_system_egl == nullptr) {
    return fallback_stub;
  }
  auto* symbol = reinterpret_cast<void*>(dlsym(g_system_egl, name));
  if (symbol == nullptr) {
    return fallback_stub;
  }
  return reinterpret_cast<Fn>(symbol);
}

// Fallback stubs used when the PS4 system EGL is unavailable.
EGLBoolean FallbackInitialize(EGLDisplay, EGLint*, EGLint*) {
  return kEglFalse;
}
EGLDisplay FallbackGetDisplay(EGLNativeDisplayType) {
  return kEglNoDisplay;
}
EGLBoolean FallbackChooseConfig(EGLDisplay, const EGLint*, EGLConfig*) {
  return kEglFalse;
}
EGLContext FallbackCreateContext(EGLDisplay, EGLConfig, EGLContext,
                                 const EGLint*) {
  return kEglNoContext;
}
EGLSurface FallbackCreateWindowSurface(EGLDisplay, EGLConfig,
                                       EGLNativeWindowType,
                                       const EGLint*) {
  return kEglNoSurface;
}
EGLSurface FallbackCreatePbufferSurface(EGLDisplay, EGLConfig,
                                        EGLNativePbufferType,
                                        const EGLint*) {
  return kEglNoSurface;
}
EGLBoolean FallbackMakeCurrent(EGLDisplay, EGLSurface, EGLSurface,
                               EGLContext) {
  return kEglFalse;
}
EGLBoolean FallbackSwapBuffers(EGLDisplay, EGLSurface) {
  return kEglFalse;
}
EGLint FallbackGetError() { return kEglNotInitialized; }

// X-macro table of forwarded EGL entry points. Every row defines the
// exported symbol as a thin forwarder to the resolved system function.
#define PS4_EGL_ENTRY(sym, ret, params, args, fallback)                    \
  ret sym params {                                                         \
    return ((ret (*)params)ResolveEntry(#sym, (ret (*)params)fallback))  \
        args;                                                             \
  }

PS4_EGL_ENTRY(eglBindAPI, EGLBoolean, (EGLenum api), (api),
              FallbackMakeCurrent)
PS4_EGL_ENTRY(eglChooseConfig, EGLBoolean,
              (EGLDisplay d, const EGLint* attrib_list, EGLConfig* configs),
              (d, attrib_list, configs), FallbackChooseConfig)
PS4_EGL_ENTRY(eglCopyBuffers, EGLBoolean,
              (EGLDisplay d, EGLSurface s, const void* target), (d, s, target),
              FallbackSwapBuffers)
PS4_EGL_ENTRY(eglCreateContext, EGLContext,
              (EGLDisplay d, EGLConfig c, EGLContext share,
               const EGLint* attrib_list),
              (d, c, share, attrib_list), FallbackCreateContext)
PS4_EGL_ENTRY(eglCreatePbufferSurface, EGLSurface,
              (EGLDisplay d, EGLConfig c, EGLNativePbufferType b,
               const EGLint* attrib_list),
              (d, c, b, attrib_list), FallbackCreatePbufferSurface)
PS4_EGL_ENTRY(eglCreateWindowSurface, EGLSurface,
              (EGLDisplay d, EGLConfig c, EGLNativeWindowType w,
               const EGLint* attrib_list),
              (d, c, w, attrib_list), FallbackCreateWindowSurface)
PS4_EGL_ENTRY(eglDestroyContext, EGLBoolean, (EGLDisplay d, EGLContext c),
              (d, c), FallbackMakeCurrent)
PS4_EGL_ENTRY(eglDestroySurface, EGLBoolean, (EGLDisplay d, EGLSurface s),
              (d, s), FallbackMakeCurrent)
PS4_EGL_ENTRY(eglGetConfigAttrib, EGLBoolean,
              (EGLDisplay d, EGLConfig c, EGLint a, EGLint* value),
              (d, c, a, value), FallbackChooseConfig)
PS4_EGL_ENTRY(eglGetConfigs, EGLBoolean,
              (EGLDisplay d, EGLConfig* configs, EGLint* num),
              (d, configs, num), FallbackChooseConfig)
PS4_EGL_ENTRY(eglGetCurrentContext, EGLContext, (), (), FallbackCreateContext)
PS4_EGL_ENTRY(eglGetCurrentDisplay, EGLDisplay, (), (), FallbackGetDisplay)
PS4_EGL_ENTRY(eglGetCurrentSurface, EGLSurface, (EGLint read_draw),
              (read_draw), FallbackCreateWindowSurface)
PS4_EGL_ENTRY(eglGetDisplay, EGLDisplay, (EGLNativeDisplayType display),
              (display), FallbackGetDisplay)
PS4_EGL_ENTRY(eglGetError, EGLint, (), (), FallbackGetError)
PS4_EGL_ENTRY(eglGetProcAddress, __eglMustCastFP, (const char* name), (name),
              ((__eglMustCastFP)nullptr))
PS4_EGL_ENTRY(eglInitialize, EGLBoolean, (EGLDisplay d, EGLint* a, EGLint* b),
              (d, a, b), FallbackInitialize)
PS4_EGL_ENTRY(eglMakeCurrent, EGLBoolean,
              (EGLDisplay d, EGLSurface d_s, EGLSurface r_s, EGLContext c),
              (d, d_s, r_s, c), FallbackMakeCurrent)
PS4_EGL_ENTRY(eglQueryContext, EGLBoolean,
              (EGLDisplay d, EGLContext c, EGLint a, EGLint* value),
              (d, c, a, value), FallbackCreateContext)
PS4_EGL_ENTRY(eglQueryString, const char*, (EGLDisplay d, EGLint name),
              (d, name), ((const char*)nullptr))
PS4_EGL_ENTRY(eglReleaseThread, EGLBoolean, (), (), FallbackMakeCurrent)
PS4_EGL_ENTRY(eglSwapBuffers, EGLBoolean, (EGLDisplay d, EGLSurface s),
              (d, s), FallbackSwapBuffers)
PS4_EGL_ENTRY(eglSwapInterval, EGLBoolean, (EGLDisplay d, EGLint interval),
              (d, interval), FallbackSwapBuffers)
PS4_EGL_ENTRY(eglTerminate, EGLBoolean, (EGLDisplay d), (d),
              FallbackInitialize)
PS4_EGL_ENTRY(eglWaitGL, EGLBoolean, (), (), FallbackSwapBuffers)
PS4_EGL_ENTRY(eglWaitKHR, EGLBoolean, (EGLDisplay d, EGLint* mask), (d, mask),
              FallbackSwapBuffers)

#undef PS4_EGL_ENTRY

struct EglLoader {
  EglLoader() {
    g_system_egl = dlopen(SystemEglLibrary(), RTLD_NOW | RTLD_GLOBAL);
    if (g_system_egl == nullptr) {
      std::fprintf(stderr,
                   "[ps4-egl-stub] cannot load system EGL '%s': %s; "
                   "guest EGL calls will fail\n",
                   SystemEglLibrary(), dlerror());
    }
  }
};

EglLoader g_egl_loader;

}  // namespace
