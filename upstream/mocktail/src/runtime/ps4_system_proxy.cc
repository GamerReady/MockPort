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

// PS4 port replacement for src/runtime/system_proxy.cc. The upstream
// implementation uses GIO to detect the desktop system proxy. Orbis OS has
// no system proxy configuration, so the PS4 build reports no system proxy;
// proxy settings come from the environment (MOCKTAIL_HTTP_PROXY_*) instead.

#include "runtime/system_proxy.h"

namespace mocktail::runtime {

SystemProxyResult SelectSystemProxy(
    const std::vector<std::string>& candidates) {
  (void)candidates;
  SystemProxyResult result;
  return result;
}

SystemProxyResult ResolveSystemProxy() {
  SystemProxyResult result;
  return result;
}

}  // namespace mocktail::runtime
