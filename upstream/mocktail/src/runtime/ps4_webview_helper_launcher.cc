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

// PS4 port replacement for src/runtime/webview_helper_launcher.cc. The
// packet codecs are platform-neutral and preserved; URL validation uses the
// PS4 HTTP transport's URL parser instead of libcurl. The helper process
// itself is not available on PS4 (no WebKit), so LaunchWebViewHelper reports
// the webview as unavailable.

#include "runtime/webview_helper_launcher.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "ps4/ps4_http_transport.h"

namespace mocktail::runtime {
namespace {

constexpr std::string_view kProtocolHeader = "MOCKTAIL-WEBVIEW 1\n";
constexpr std::string_view kControlMagic = "MWVC";
constexpr std::string_view kEventMagic = "MWVE";
constexpr unsigned char kControlProtocolVersion = 1;
constexpr std::size_t kControlHeaderBytes = 12;
constexpr std::size_t kMaximumRequestBytes =
    kMaximumWebViewUrlBytes + kProtocolHeader.size() + 32;
constexpr std::string_view kInternalRoutePrefix = "www:";
constexpr std::string_view kRobloxWebOrigin = "https://www.roblox.com/";

bool ContainsControlBytes(std::string_view value) {
  return std::any_of(value.begin(), value.end(), [](char byte) {
    const auto byte_value = static_cast<unsigned char>(byte);
    return byte_value < 0x20 && byte_value != '\t';
  });
}

bool IsRobloxHost(std::string_view host) {
  constexpr std::string_view kRoot = "roblox.com";
  if (host.size() < kRoot.size()) {
    return false;
  }
  const std::string_view suffix = host.substr(host.size() - kRoot.size());
  if (suffix != kRoot) {
    return false;
  }
  return host.size() == kRoot.size() ||
         host[host.size() - kRoot.size() - 1] == '.';
}

bool HasUnsafePath(std::string_view path) {
  return path.find("..") != std::string_view::npos ||
         path.find('\\') != std::string_view::npos;
}

bool EqualAscii(std::string_view left, std::string_view right) {
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

bool ValidateWebViewUrl(std::string_view url, std::string* error) {
  const auto fail = [error](const char* message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (url.empty() || url.size() > kMaximumWebViewUrlBytes ||
      ContainsControlBytes(url)) {
    return fail("webview URL is empty, oversized, or contains control bytes");
  }

  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  if (!ps4::ParseHttpUrl(std::string(url), &scheme, &host, &port, &path)) {
    return fail("webview URL is malformed");
  }
  if (scheme != "https" || !IsRobloxHost(host)) {
    return fail("webview URL must use HTTPS on roblox.com");
  }
  if (!port.empty() && port != "443") {
    return fail("webview URL must use the default HTTPS port");
  }
  if (url.find('@') != std::string_view::npos) {
    return fail("webview URL must not contain user information");
  }
  if (HasUnsafePath(path)) {
    return fail("webview URL contains an unsafe path");
  }
  return true;
}

bool NormalizeWebViewUrl(std::string_view url, std::string* normalized_url,
                         std::string* error) {
  const auto fail = [error](const char* message) {
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (normalized_url == nullptr) {
    return fail("webview URL has no output destination");
  }
  if (url.empty() || url.size() > kMaximumWebViewUrlBytes ||
      ContainsControlBytes(url)) {
    return fail("webview URL is empty, oversized, or contains control bytes");
  }

  if (url.compare(0, kInternalRoutePrefix.size(), kInternalRoutePrefix) == 0) {
    const std::string_view route = url.substr(kInternalRoutePrefix.size());
    if (route.empty() || route.front() == '/') {
      return fail("webview internal Roblox route is invalid");
    }
    normalized_url->assign(kRobloxWebOrigin);
    normalized_url->append(route);
  } else {
    normalized_url->assign(url);
  }
  if (!ValidateWebViewUrl(*normalized_url, error)) {
    normalized_url->clear();
    return false;
  }
  return true;
}

bool DecodeWebViewRequest(std::string_view request, std::string* url,
                          std::string* error) {
  if (url == nullptr) {
    if (error != nullptr) {
      *error = "webview request has no output destination";
    }
    return false;
  }
  if (request.size() > kMaximumRequestBytes ||
      request.size() < kProtocolHeader.size() ||
      request.compare(0, kProtocolHeader.size(), kProtocolHeader) != 0) {
    if (error != nullptr) {
      *error = "webview request has an invalid protocol header";
    }
    return false;
  }
  const std::size_t length_end = request.find('\n', kProtocolHeader.size());
  if (length_end == std::string_view::npos) {
    if (error != nullptr) {
      *error = "webview request has no bounded URL length";
    }
    return false;
  }
  const std::string_view length_text =
      request.substr(kProtocolHeader.size(), length_end - kProtocolHeader.size());
  std::size_t expected_length = 0;
  const auto conversion = std::from_chars(
      length_text.data(), length_text.data() + length_text.size(),
      expected_length);
  const std::string_view decoded = request.substr(length_end + 1);
  if (length_text.empty() || conversion.ec != std::errc() ||
      conversion.ptr != length_text.data() + length_text.size() ||
      expected_length != decoded.size() ||
      expected_length > kMaximumWebViewUrlBytes ||
      !ValidateWebViewUrl(decoded, error)) {
    if (error != nullptr && error->empty()) {
      *error = "webview request URL length is invalid";
    }
    return false;
  }
  url->assign(decoded);
  return true;
}

bool DecodeWebViewHelperControlPacket(std::string_view packet,
                                      WebViewHelperControlCommand* command,
                                      std::string* error) {
  if (error != nullptr) {
    error->clear();
  }
  const auto fail = [command, error](const char* message) {
    if (command != nullptr) {
      command->operation = WebViewHelperControlOperation::kClose;
      command->payload.clear();
      command->visible = false;
    }
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (command == nullptr) {
    return fail("webview control command has no output destination");
  }
  if (packet.size() < kControlHeaderBytes ||
      packet.size() > kMaximumWebViewControlPacketBytes ||
      packet.compare(0, kControlMagic.size(), kControlMagic) != 0 ||
      static_cast<unsigned char>(packet[4]) != kControlProtocolVersion ||
      packet[6] != '\0' || packet[7] != '\0') {
    return fail("webview control packet has an invalid header");
  }
  const std::size_t payload_size =
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[8])) << 24) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[9])) << 16) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[10])) << 8) |
      static_cast<std::size_t>(static_cast<unsigned char>(packet[11]));
  if (payload_size != packet.size() - kControlHeaderBytes) {
    return fail("webview control packet has an invalid payload length");
  }

  const auto operation = static_cast<WebViewHelperControlOperation>(
      static_cast<unsigned char>(packet[5]));
  const std::string_view payload = packet.substr(kControlHeaderBytes);
  const bool contains_nul = payload.find('\0') != std::string_view::npos;
  switch (operation) {
    case WebViewHelperControlOperation::kLoadUrl:
      if (!ValidateWebViewUrl(payload, error)) {
        command->operation = WebViewHelperControlOperation::kClose;
        command->payload.clear();
        command->visible = false;
        return false;
      }
      break;
    case WebViewHelperControlOperation::kSetTitle:
      if (payload.size() > kMaximumWebViewTitleBytes ||
          ContainsControlBytes(payload)) {
        return fail("webview control title is oversized or unsafe");
      }
      break;
    case WebViewHelperControlOperation::kSetVisible:
    case WebViewHelperControlOperation::kSetBackNavigationDisabled:
    case WebViewHelperControlOperation::kSetShowDomainAsTitle:
      if (payload.size() != 1 ||
          (static_cast<unsigned char>(payload.front()) != 0 &&
           static_cast<unsigned char>(payload.front()) != 1)) {
        return fail("webview control boolean payload is invalid");
      }
      break;
    case WebViewHelperControlOperation::kEvaluateJavaScript:
      if (payload.size() > kMaximumWebViewJavaScriptBytes || contains_nul) {
        return fail("webview control JavaScript is oversized or contains NUL");
      }
      break;
    case WebViewHelperControlOperation::kSetRobloxCookie:
      if (payload.empty() || payload.size() > kMaximumWebViewCookieBytes ||
          !std::all_of(payload.begin(), payload.end(), [](char byte) {
            const unsigned char value = static_cast<unsigned char>(byte);
            return value == 0x21 || (value >= 0x23 && value <= 0x2b) ||
                   (value >= 0x2d && value <= 0x3a) ||
                   (value >= 0x3c && value <= 0x5b) ||
                   (value >= 0x5d && value <= 0x7e);
          })) {
        return fail("webview Roblox cookie is oversized or unsafe");
      }
      break;
    case WebViewHelperControlOperation::kClearRobloxCookie:
      if (!payload.empty()) {
        return fail("webview Roblox cookie clear payload must be empty");
      }
      break;
    case WebViewHelperControlOperation::kRetainPersistentRobloxCookie:
      if (!payload.empty()) {
        return fail("webview Roblox cookie retention payload must be empty");
      }
      break;
    case WebViewHelperControlOperation::kClose:
      if (!payload.empty()) {
        return fail("webview control close payload must be empty");
      }
      break;
    default:
      return fail("webview control operation is unsupported");
  }

  command->operation = operation;
  command->payload.assign(payload);
  command->visible = !payload.empty() && payload.front() != '\0';
  return true;
}

bool EncodeWebViewHelperEventPacket(WebViewHelperEventType type,
                                    std::string_view payload,
                                    std::string* packet) {
  const bool ready = type == WebViewHelperEventType::kReady;
  const bool cookie = type == WebViewHelperEventType::kRobloxCookie;
  if (packet == nullptr || (!ready && payload.empty()) ||
      payload.size() > (cookie ? kMaximumWebViewCookieBytes
                               : kMaximumWebViewHybridEventBytes) ||
      payload.find('\0') != std::string_view::npos ||
      (type != WebViewHelperEventType::kExecuteRoblox &&
       type != WebViewHelperEventType::kRobloxWkHybrid && !ready && !cookie)) {
    return false;
  }
  packet->assign(kControlHeaderBytes, '\0');
  std::copy(kEventMagic.begin(), kEventMagic.end(), packet->begin());
  (*packet)[4] = static_cast<char>(kControlProtocolVersion);
  (*packet)[5] = static_cast<char>(type);
  const std::size_t size = payload.size();
  (*packet)[8] = static_cast<char>((size >> 24) & 0xff);
  (*packet)[9] = static_cast<char>((size >> 16) & 0xff);
  (*packet)[10] = static_cast<char>((size >> 8) & 0xff);
  (*packet)[11] = static_cast<char>(size & 0xff);
  packet->append(payload);
  return true;
}

bool DecodeWebViewHelperEventPacket(std::string_view packet,
                                    WebViewHelperEvent* event,
                                    std::string* error) {
  if (error != nullptr) {
    error->clear();
  }
  const auto fail = [event, error](const char* message) {
    if (event != nullptr) {
      event->type = WebViewHelperEventType::kExecuteRoblox;
      event->payload.clear();
    }
    if (error != nullptr) {
      *error = message;
    }
    return false;
  };
  if (event == nullptr) {
    return fail("webview event has no output destination");
  }
  if (packet.size() < kControlHeaderBytes ||
      packet.size() > kMaximumWebViewEventPacketBytes ||
      packet.compare(0, kEventMagic.size(), kEventMagic) != 0 ||
      static_cast<unsigned char>(packet[4]) != kControlProtocolVersion ||
      packet[6] != '\0' || packet[7] != '\0') {
    return fail("webview event packet has an invalid header");
  }
  const std::size_t payload_size =
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[8])) << 24) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[9])) << 16) |
      (static_cast<std::size_t>(static_cast<unsigned char>(packet[10])) << 8) |
      static_cast<std::size_t>(static_cast<unsigned char>(packet[11]));
  if (payload_size != packet.size() - kControlHeaderBytes) {
    return fail("webview event packet has an invalid payload length");
  }
  event->type = static_cast<WebViewHelperEventType>(
      static_cast<unsigned char>(packet[5]));
  event->payload.assign(packet.substr(kControlHeaderBytes));
  return true;
}

WebViewHelperLaunchResult LaunchWebViewHelper(
    const std::filesystem::path& helper, std::string_view url,
    WebViewHelperExitObserver exit_observer) {
  (void)helper;
  (void)exit_observer;
  WebViewHelperLaunchResult result;
  std::string normalized_url;
  if (!NormalizeWebViewUrl(url, &normalized_url, &result.error)) {
    return result;
  }
  result.error = "the webview helper is not available on the PS4 port";
  return result;
}

}  // namespace mocktail::runtime
