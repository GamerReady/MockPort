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

// PS4 port replacement for src/webview/webview_helper_policy.cc. The upstream
// implementation uses GLib URI parsing and JavaScriptCore value extraction.
// The PS4 build keeps the same URL policy and callback script behaviour but
// implements URI parsing with the PS4 HTTP transport's URL parser and has no
// JavaScriptCore (the webview helper itself is not available on PS4).

#include "webview/webview_helper_policy.h"

#include <cstdlib>
#include <string>
#include <string_view>

#include "ps4/ps4_http_transport.h"

namespace mocktail::webview {
namespace {

constexpr char kBridgeSource[] =
    "https://cdn.roblox.com/compatibility/mocktail/android_bridge.js";
constexpr char kFallbackRobloxVersion[] = "42.0.0";

bool IsSafeVersion(std::string_view version) {
  if (version.empty() || version.size() > 32) {
    return false;
  }
  for (const char character : version) {
    if ((character < '0' || character > '9') && character != '.') {
      return false;
    }
  }
  return true;
}

bool IsRobloxHost(const char* host) {
  if (host == nullptr) {
    return false;
  }
  constexpr std::string_view kRoot = "roblox.com";
  const std::string_view value(host);
  if (value.size() < kRoot.size()) {
    return false;
  }
  const std::string_view suffix = value.substr(value.size() - kRoot.size());
  if (suffix != kRoot) {
    return false;
  }
  return value.size() == kRoot.size() ||
         value[value.size() - kRoot.size() - 1] == '.';
}

bool EqualAsciiIgnoreCase(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto a = static_cast<unsigned char>(left[index]);
    const auto b = static_cast<unsigned char>(right[index]);
    if (std::tolower(a) != std::tolower(b)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool ShouldDisableWebKitSandbox(std::string_view kernel_version,
                                const char* sandbox_override) {
  return sandbox_override == nullptr &&
         (kernel_version.find("FreeBSD") != std::string_view::npos ||
          kernel_version.find("freebsd.org") != std::string_view::npos);
}

bool ShouldDisableWebViewHardwareAcceleration(
    bool wayland_display, const char* compositing_override) {
  if (compositing_override != nullptr) {
    return std::string_view(compositing_override) != "0";
  }
  return wayland_display;
}

const char* AndroidBridgeSource() { return kBridgeSource; }

std::string BuildRobloxAndroidUserAgent() {
  const char* configured_version = std::getenv("MOCKTAIL_ROBLOX_VERSION");
  const std::string_view version =
      configured_version != nullptr && IsSafeVersion(configured_version)
          ? std::string_view(configured_version)
          : std::string_view(kFallbackRobloxVersion);
  return "Mozilla/5.0 AppleWebKit/605.1.15 (KHTML, like Gecko)  ROBLOX "
         "Android App " +
         std::string(version) +
         " Tablet Hybrid()  GooglePlayStore RobloxApp/" +
         std::string(version) + " (GlobalDist; GooglePlayStore)";
}

bool IsBrowserLoginUrl(std::string_view url) {
  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  if (!ps4::ParseHttpUrl(std::string(url), &scheme, &host, &port, &path)) {
    return false;
  }
  const UriPolicyResult policy = EvaluateNavigationUri(url.data());
  const bool browser_login =
      policy.allowed &&
      (EqualAsciiIgnoreCase(host, "www.roblox.com") ||
       EqualAsciiIgnoreCase(host, "roblox.com")) &&
      (path == "/login" || path == "/login/");
  return browser_login;
}

bool IsEssentialWebResource(const char* uri) {
  if (uri == nullptr) {
    return false;
  }
  const UriPolicyResult policy = EvaluateNavigationUri(uri);
  if (!policy.allowed) {
    return false;
  }
  if (policy.host == "css.rbxcdn.com" || policy.host == "js.rbxcdn.com") {
    return true;
  }
  if (policy.host != "arkoselabs.roblox.com" &&
      policy.host != "apis.rbxcdn.com") {
    return false;
  }
  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  if (!ps4::ParseHttpUrl(uri, &scheme, &host, &port, &path)) {
    return false;
  }
  if (policy.host == "apis.rbxcdn.com") {
    return path == "/captcha/v1/metadata";
  }
  return path.rfind("/v2/", 0) == 0 || path.rfind("/fc/", 0) == 0;
}

std::string BoundedLogToken(const char* value, std::string_view fallback) {
  if (value == nullptr || *value == '\0') {
    return std::string(fallback);
  }
  constexpr std::size_t kMaximumLogTokenBytes = 128;
  std::string token;
  for (const unsigned char character : std::string_view(value)) {
    if (token.size() == kMaximumLogTokenBytes) {
      break;
    }
    token.push_back(character >= 0x21 && character <= 0x7e
                        ? static_cast<char>(character)
                        : '?');
  }
  return token;
}

UriPolicyResult EvaluateNavigationUri(const char* uri) {
  UriPolicyResult result;
  if (uri == nullptr) {
    return result;
  }
  if (std::string_view(uri) == "about:blank") {
    result.allowed = true;
    result.scheme = "about";
    result.host = "none";
    return result;
  }

  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  if (!ps4::ParseHttpUrl(uri, &scheme, &host, &port, &path)) {
    return result;
  }

  result.scheme = BoundedLogToken(scheme.c_str(), "invalid");
  result.host = BoundedLogToken(host.c_str(), "none");
  result.allowed = scheme == "https" && !host.empty() &&
                   (port.empty() || port == "443");
  result.privileged_bridge_allowed = result.allowed && IsRobloxHost(host.c_str());
  return result;
}

const char* CaptchaEventName(CaptchaEventType type) {
  return type == CaptchaEventType::kSuccess ? "CaptchaSuccess" : "CaptchaShown";
}

bool ExtractExecuteRobloxCommand(JSCValue* value, std::string* command) {
  // No JavaScriptCore on PS4: the webview helper is unavailable, so no
  // command can be extracted.
  (void)value;
  if (command != nullptr) {
    command->clear();
  }
  return false;
}

bool ExtractRobloxWkHybridCommand(JSCValue* value, std::string* command) {
  (void)value;
  if (command != nullptr) {
    command->clear();
  }
  return false;
}

bool ParseCaptchaEvent(std::string_view command, CaptchaEvent* event) {
  if (event == nullptr) {
    return false;
  }
  if (command == "CaptchaSuccess") {
    event->type = CaptchaEventType::kSuccess;
    return true;
  }
  if (command == "CaptchaShown") {
    event->type = CaptchaEventType::kShown;
    return true;
  }
  return false;
}

std::string BuildCallbackScript(std::string_view callback_id) {
  return "window.mocktailRobloxBridge && "
         "window.mocktailRobloxBridge.onCallback(\"" +
         std::string(callback_id) + "\");";
}

}  // namespace mocktail::webview
