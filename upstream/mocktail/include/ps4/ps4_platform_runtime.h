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

#ifndef MOCKTAIL_PS4_PS4_PLATFORM_RUNTIME_H_
#define MOCKTAIL_PS4_PS4_PLATFORM_RUNTIME_H_

#include <memory>

#include "mocktail/platform/platform_runtime.h"

namespace mocktail {
namespace ps4 {

// Creates the PS4 (jailbroken PlayStation 4) platform runtime.
//
// The runtime drives SDL3's PS4 video/gamepad drivers (provided by the PS4
// homebrew SDK's SDL3 build) and presents the Roblox client fullscreen at the
// console's native resolution.  All events are normalised into the shared
// PlatformEvent contract so the rest of Mocktail stays platform agnostic.
std::unique_ptr<platform::PlatformRuntime> CreatePs4PlatformRuntime();

}  // namespace ps4
}  // namespace mocktail

#endif  // MOCKTAIL_PS4_PS4_PLATFORM_RUNTIME_H_
