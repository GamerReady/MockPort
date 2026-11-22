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

// PS4 port replacement for src/runtime/fleasion.cc. Fleasion is the optional
// TLS-interception proxy; the PS4 port does not implement it (the Roblox
// client performs its own TLS). The feature is disabled by default, so this
// stub is only reached when MOCKTAIL_FLEASION_ENABLED=1 is set explicitly.

#include "runtime/fleasion.h"

namespace mocktail::runtime {

FleasionPreparation PrepareFleasion(const RuntimeConfig& config,
                                   const Environment& environment,
                                   const RuntimePaths& paths) {
  (void)config;
  (void)environment;
  (void)paths;
  FleasionPreparation preparation;
  preparation.error =
      "Fleasion TLS interception is not available on the PS4 port";
  return preparation;
}

}  // namespace mocktail::runtime
