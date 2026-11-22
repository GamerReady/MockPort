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

#include "ps4/ps4_platform_runtime.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

#include <cstdlib>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "mocktail/platform/sdl_event_converter.h"

namespace mocktail {
namespace ps4 {
namespace {

// The PS4 outputs 1080p by default; allow overriding through the environment
// for non-standard output configurations.
constexpr int kDefaultWidth = 1920;
constexpr int kDefaultHeight = 1080;

Status Ps4Error(const char* operation) {
  std::string message = operation != nullptr ? operation : "PS4 operation";
  message += " failed: ";
  const char* error = SDL_GetError();
  message += error != nullptr && error[0] != '\0' ? error : "unknown SDL error";
  return Status::Error(StatusCode::kPlatformError, std::move(message));
}

int EnvironmentInt(const char* name, int fallback) {
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') {
    return fallback;
  }
  char* end = nullptr;
  const long parsed = std::strtol(value, &end, 10);
  if (end == value || parsed <= 0) {
    return fallback;
  }
  return static_cast<int>(parsed);
}

class Ps4PlatformRuntime final : public platform::PlatformRuntime {
 public:
  ~Ps4PlatformRuntime() override { Shutdown(); }

  Status Initialize(const platform::WindowOptions& options) override {
    if (initialized_) {
      return Status::Error(StatusCode::kFailedPrecondition,
                           "PS4 platform runtime is already initialized");
    }
    if (options.width <= 0 || options.height <= 0) {
      return Status::Error(StatusCode::kInvalidArgument,
                           "window dimensions must be positive");
    }
    if (options.title.empty()) {
      return Status::Error(StatusCode::kInvalidArgument,
                           "window title must not be empty");
    }

    const SDL_InitFlags init_flags = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD;
    if (!SDL_InitSubSystem(init_flags)) {
      return Ps4Error("SDL_InitSubSystem");
    }
    initialized_subsystems_ = init_flags;

    // The PS4 has no resizable, decorated desktop window. Present the client
    // fullscreen at the console resolution; the SDL PS4 driver owns the
    // underlying EGL surface.
    SDL_WindowFlags flags = SDL_WINDOW_FULLSCREEN;
    if (options.initially_hidden) {
      flags |= SDL_WINDOW_HIDDEN;
    }
    if (options.surface_api == platform::WindowSurfaceApi::kDirectVulkan) {
      // The PS4 has no Vulkan driver; request the EGL/GLES surface instead.
      flags |= SDL_WINDOW_OPENGL;
    } else {
      flags |= SDL_WINDOW_OPENGL;
    }

    window_ = SDL_CreateWindow(options.title.c_str(), options.width,
                               options.height, flags);
    if (window_ == nullptr) {
      const Status status = Ps4Error("SDL_CreateWindow");
      Shutdown();
      return status;
    }

    descriptor_ = ResolveNativeWindow(options.surface_api);
    if (options.require_native_window && !descriptor_.HasPlatformHandle()) {
      const Status status = Status::Error(
          StatusCode::kUnavailable,
          "PS4 SDL window has no presentation handle; refusing fake WSI");
      Shutdown();
      return status;
    }

    initialized_ = true;
    return Status::Ok();
  }

  void Shutdown() override {
    if (window_ != nullptr) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
    }
    descriptor_ = {};
    if (initialized_subsystems_ != 0) {
      SDL_QuitSubSystem(initialized_subsystems_);
      initialized_subsystems_ = 0;
    }
    initialized_ = false;
  }

  bool IsInitialized() const override { return initialized_; }

  Status PollEvent(platform::PlatformEvent* event, bool* has_event) override {
    if (event == nullptr || has_event == nullptr) {
      return Status::Error(StatusCode::kInvalidArgument,
                           "PollEvent output pointers must not be null");
    }
    if (!initialized_) {
      return Status::Error(StatusCode::kFailedPrecondition,
                           "platform runtime is not initialized");
    }

    *has_event = false;
    SDL_Event sdl_event;
    while (SDL_PollEvent(&sdl_event)) {
      if (platform::ConvertSdlEvent(window_, sdl_event, event,
                                    SDL_GetWindowRelativeMouseMode(window_))) {
        *has_event = true;
        return Status::Ok();
      }
    }
    return Status::Ok();
  }

  Status SetTextInputEnabled(bool enabled) override {
    // The jailbroken PS4 exposes no IME or text-input surface to homebrew.
    // Text entry is unavailable either way, so this is a deliberate no-op.
    (void)enabled;
    if (!initialized_) {
      return Status::Error(StatusCode::kFailedPrecondition,
                           "platform runtime is not initialized");
    }
    return Status::Ok();
  }

  Status ShowWindow() override {
    if (!initialized_ || window_ == nullptr) {
      return Status::Error(StatusCode::kFailedPrecondition,
                           "platform runtime is not initialized");
    }
    if (!SDL_ShowWindow(window_)) {
      return Ps4Error("SDL_ShowWindow");
    }
    return Status::Ok();
  }

  platform::NativeWindowDescriptor GetNativeWindow() const override {
    return descriptor_;
  }

 private:
  platform::NativeWindowDescriptor ResolveNativeWindow(
      platform::WindowSurfaceApi surface_api) {
    platform::NativeWindowDescriptor descriptor;
    descriptor.window_system = platform::NativeWindowSystem::kPs4;
    descriptor.surface_api = surface_api;
    descriptor.backend_window = window_;
    // The SDL PS4 driver owns the EGL display/surface internally. Homebrew
    // SDL3 builds for the PS4 may expose them through vendor window
    // properties in the future; until then the SDL window itself is the
    // presentation handle the EGL backend binds to.
    return descriptor;
  }

  SDL_Window* window_ = nullptr;
  SDL_InitFlags initialized_subsystems_ = 0;
  platform::NativeWindowDescriptor descriptor_;
  bool initialized_ = false;
};

}  // namespace

std::unique_ptr<platform::PlatformRuntime> CreatePs4PlatformRuntime() {
  return std::make_unique<Ps4PlatformRuntime>();
}

}  // namespace ps4
}  // namespace mocktail
