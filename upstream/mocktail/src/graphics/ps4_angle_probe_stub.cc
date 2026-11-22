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

// PS4 port replacement for src/graphics/angle_probe.cc. The PS4 has no ANGLE
// distribution; the graphics stack uses the system EGL/GLES backend
// (GraphicsBackendKind::kSystemEgl) backed by the console GPU. The probe
// reports ANGLE as unavailable so window creation falls through to the
// system EGL path.

#include "mocktail/graphics/angle_probe.h"

namespace mocktail::graphics {

BackendCapability InspectAngleLibraries(const AngleProbeOptions& options) {
  (void)options;
  BackendCapability capability;
  capability.backend = GraphicsBackendKind::kAngleVulkan;
  capability.state = CapabilityState::kUnavailable;
  capability.detail = "ANGLE is not available on the PS4 port";
  return capability;
}

BackendCapability ProbeAngleVulkan(const AngleProbeOptions& options) {
  return InspectAngleLibraries(options);
}

}  // namespace mocktail::graphics
