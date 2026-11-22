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

// PS4 port replacement for src/webview/mocktail_webview_helper.cc. The
// upstream helper is a GTK/WebKitGTK application that renders Roblox's web
// runtime. Orbis OS has no WebKit, so this stub reports the helper as
// unavailable; the launcher never starts it on PS4.

#include <cstdio>

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
  std::fprintf(stderr,
               "[mocktail_webview_helper] the webview helper is not "
               "available on the PS4 port\n");
  return 1;
}
