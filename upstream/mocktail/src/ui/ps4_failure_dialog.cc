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

// PS4 port replacement for src/ui/mocktail_failure_dialog.cc. The upstream
// helper renders failure dialogs with GTK/libadwaita. The PS4 has no desktop
// toolkit, so failures are reported on the console/log output instead.

#include "runtime/failure_dialog.h"

#include <cstdio>
#include <string>

namespace mocktail::runtime {
namespace {

void EmitMessage(const char* prefix, std::string_view message) {
  const std::string line = std::string(prefix) + std::string(message) + "\n";
  std::fwrite(line.data(), 1, line.size(), stderr);
  std::fflush(stderr);
}

}  // namespace

bool FailureDialogsEnabled(const Environment& environment) {
  // The PS4 port always reports failures through the console.
  (void)environment;
  return true;
}

bool ShowFailureDialog(const Environment& environment,
                       std::string_view message) {
  (void)environment;
  EmitMessage("[mocktail] FAILURE: ", message);
  return true;
}

bool ShowWarningDialog(const Environment& environment,
                       std::string_view message) {
  (void)environment;
  EmitMessage("[mocktail] WARNING: ", message);
  return true;
}

FailureDialogMonitor::~FailureDialogMonitor() = default;

FailureDialogMonitor::FailureDialogMonitor(FailureDialogMonitor&& other)
    noexcept = default;

FailureDialogMonitor& FailureDialogMonitor::operator=(
    FailureDialogMonitor&& other) noexcept = default;

FailureDialogMonitor FailureDialogMonitor::Start(
    const Environment& environment, std::string_view initial_message) {
  (void)environment;
  if (!initial_message.empty()) {
    EmitMessage("[mocktail] FAILURE: ", initial_message);
  }
  return FailureDialogMonitor{};
}

void FailureDialogMonitor::SetMessage(std::string_view message) {
  if (!message.empty()) {
    EmitMessage("[mocktail] FAILURE: ", message);
  }
}

void FailureDialogMonitor::MarkSuccessful() {}

void FailureDialogMonitor::Finish(char disposition) {
  (void)disposition;
}

}  // namespace mocktail::runtime
