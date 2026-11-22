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

// PS4 build of the guest-facing libvulkan.so stub.
//
// The jailbroken PS4 has no Vulkan driver. This stub presents a minimal,
// deterministic "Vulkan is unavailable" surface: vkGetInstanceProcAddr
// resolves the core entry points, and every initialization entry point
// returns VK_ERROR_INITIALIZATION_FAILED. The Roblox client's Vulkan
// renderer sees the failure and falls back to its OpenGL ES renderer, which
// is the only graphics path the PS4 port supports.
//
// Types below mirror the Vulkan ABI (all handles are pointer-sized on
// x86_64); no Vulkan headers are required to build this stub.

#include <cstdint>
#include <cstring>

using VkResult = int32_t;
using VkInstance = void*;
using VkPhysicalDevice = void*;
using VkDevice = void*;
using VkSurfaceKHR = void*;
using PFN_vkVoidFunction = void (*)();

namespace {

constexpr VkResult kVkErrorInitializationFailed = -3;
constexpr VkResult kVkErrorDeviceLost = -4;

VkResult FailInitialization() { return kVkErrorInitializationFailed; }
VkResult FailDeviceLost() { return kVkErrorDeviceLost; }

}  // namespace

extern "C" VkResult vkCreateInstance(const void*, const void*, void*) {
  return FailInitialization();
}

extern "C" void vkDestroyInstance(VkInstance, void*) {}

extern "C" VkResult vkCreateDevice(VkPhysicalDevice, const void*,
                                   const void*, void*, void*, VkDevice*) {
  return FailInitialization();
}

extern "C" void vkDestroyDevice(VkDevice, void*) {}

extern "C" VkResult vkGetPhysicalDeviceProperties(VkPhysicalDevice, void*) {
  return FailDeviceLost();
}

extern "C" VkResult vkGetPhysicalDeviceFeatures(VkPhysicalDevice, void*) {
  return FailDeviceLost();
}

extern "C" VkResult vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice,
                                                        void*) {
  return FailDeviceLost();
}

extern "C" VkResult vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice, void*, void*) {
  return FailDeviceLost();
}

extern "C" VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice, VkSurfaceKHR, void*) {
  return FailInitialization();
}

extern "C" PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice, const char*) {
  return nullptr;
}

extern "C" PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance,
                                                    const char* name) {
  if (name == nullptr) {
    return nullptr;
  }
  // Resolve only the entry points the loader may probe before it accepts the
  // "no Vulkan" verdict from vkCreateInstance. Anything else reports no
  // symbol so the loader cannot obtain a device function table.
  if (std::strcmp(name, "vkCreateInstance") == 0) {
    return reinterpret_cast<PFN_vkVoidFunction>(&vkCreateInstance);
  }
  if (std::strcmp(name, "vkDestroyInstance") == 0) {
    return reinterpret_cast<PFN_vkVoidFunction>(&vkDestroyInstance);
  }
  if (std::strcmp(name, "vkGetInstanceProcAddr") == 0) {
    return reinterpret_cast<PFN_vkVoidFunction>(&vkGetInstanceProcAddr);
  }
  if (std::strcmp(name, "vkGetDeviceProcAddr") == 0) {
    return reinterpret_cast<PFN_vkVoidFunction>(&vkGetDeviceProcAddr);
  }
  if (std::strcmp(name, "vkCreateDevice") == 0) {
    return reinterpret_cast<PFN_vkVoidFunction>(&vkCreateDevice);
  }
  if (std::strcmp(name, "vkDestroyDevice") == 0) {
    return reinterpret_cast<PFN_vkVoidFunction>(&vkDestroyDevice);
  }
  return nullptr;
}
