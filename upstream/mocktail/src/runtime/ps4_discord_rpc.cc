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

// PS4 port replacement for src/runtime/discord_rpc.cc. The upstream
// implementation talks to the desktop Discord client over a Unix socket and
// uses libcurl for place metadata. The PS4 has no Discord client, so the
// session is a no-op; the pure URL/activity builders are preserved with the
// same behaviour.

#include "runtime/discord_rpc.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace mocktail::runtime {
namespace {

constexpr char kJoinPage[] = "https://komaruworld.github.io/mocktail/join.html";

std::string TruncateUtf8(std::string value, std::size_t maximum) {
  if (value.size() <= maximum) {
    return value;
  }
  std::size_t boundary = maximum;
  while (boundary != 0 &&
         (static_cast<unsigned char>(value[boundary]) & 0xc0U) == 0x80U) {
    --boundary;
  }
  value.resize(boundary);
  return value;
}

bool HasDiscordJoinTarget(const RobloxExperienceLaunchRequest& request) {
  return request.place_id > 0 &&
         (request.game_instance_id.empty() ||
          (request.game_instance_id.size() <= 128 &&
           std::none_of(request.game_instance_id.begin(),
                        request.game_instance_id.end(),
                        [](unsigned char byte) {
                          return byte < 0x20 || byte == 0x7f;
                        })));
}

std::string PercentEncode(std::string_view value) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size() * 3);
  for (const unsigned char byte : value) {
    if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
        (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' ||
        byte == '.' || byte == '~') {
      encoded.push_back(static_cast<char>(byte));
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[byte >> 4U]);
      encoded.push_back(hex[byte & 0x0fU]);
    }
  }
  return encoded;
}

std::string RenderPlaceTemplate(std::string text, std::string_view place_name) {
  constexpr std::string_view placeholder = "{place_name}";
  std::size_t offset = 0;
  while ((offset = text.find(placeholder, offset)) != std::string::npos) {
    text.replace(offset, placeholder.size(), place_name);
    offset += place_name.size();
  }
  return TruncateUtf8(std::move(text), 128);
}

bool IsSafeExternalImageUrl(std::string_view url) {
  return url.size() <= 512 && url.substr(0, 8) == "https://" &&
         std::none_of(url.begin(), url.end(), [](unsigned char byte) {
           return byte < 0x20U || byte == 0x7fU;
         });
}

}  // namespace

bool IsPublicDiscordJoin(const RobloxExperienceLaunchRequest& request) {
  return HasDiscordJoinTarget(request) &&
         request.reserved_server_access_code.empty() &&
         request.access_code.empty() && request.link_code.empty();
}

std::string BuildDiscordJoinUrl(const RobloxExperienceLaunchRequest& request) {
  if (!HasDiscordJoinTarget(request)) {
    return {};
  }
  std::string url =
      std::string(kJoinPage) + "#placeId=" + std::to_string(request.place_id);
  if (!request.game_instance_id.empty()) {
    url += "&gameInstanceId=" + PercentEncode(request.game_instance_id);
  }
  return url.size() <= 512 ? std::move(url) : std::string();
}

DiscordRpcActivity BuildDiscordRpcActivity(
    const DiscordRpcConfig& config, RobloxExperiencePresencePhase phase,
    const RobloxExperienceLaunchRequest* request, std::string place_name,
    int64_t session_started_at, std::string place_icon_url) {
  DiscordRpcActivity activity;
  activity.state = TruncateUtf8(config.text.state, 128);
  switch (phase) {
    case RobloxExperiencePresencePhase::kBrowsing:
      activity.details = TruncateUtf8(config.text.browsing, 128);
      break;
    case RobloxExperiencePresencePhase::kJoining:
    case RobloxExperiencePresencePhase::kPlaying: {
      if (place_name.empty()) {
        if (phase == RobloxExperiencePresencePhase::kJoining) {
          activity.details = TruncateUtf8(config.text.joining, 128);
          break;
        }
        place_name = config.text.unknown_place;
      }
      if (config.show_place_name) {
        activity.details = RenderPlaceTemplate(config.text.playing, place_name);
      } else {
        activity.details = activity.state;
        activity.state.clear();
      }
      if (phase == RobloxExperiencePresencePhase::kPlaying &&
          config.show_elapsed_time && session_started_at > 0) {
        activity.start_timestamp = session_started_at;
      }
      if (IsSafeExternalImageUrl(place_icon_url)) {
        activity.large_image = std::move(place_icon_url);
        activity.large_text = TruncateUtf8(place_name, 128);
      }
      if (phase == RobloxExperiencePresencePhase::kPlaying &&
          config.join_enabled && request != nullptr &&
          (!config.public_servers_only || IsPublicDiscordJoin(*request))) {
        activity.button_url = BuildDiscordJoinUrl(*request);
        if (!activity.button_url.empty()) {
          activity.button_label = config.join_button_label;
        }
      }
      break;
    }
  }
  return activity;
}

class DiscordRpcSession::Impl {
 public:
  explicit Impl(DiscordRpcConfig config) : config_(std::move(config)) {}

 private:
  DiscordRpcConfig config_;
};

DiscordRpcSession::DiscordRpcSession(DiscordRpcConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

DiscordRpcSession::~DiscordRpcSession() = default;

bool DiscordRpcSession::Start(std::string* detail) {
  if (detail != nullptr) {
    *detail = "Discord Rich Presence is not available on the PS4 port";
  }
  return false;
}

void DiscordRpcSession::Stop() {}

RobloxExperiencePresenceObserver DiscordRpcSession::observer() {
  return RobloxExperiencePresenceObserver{};
}

void DiscordRpcSession::Notify(void* context,
                               RobloxExperiencePresencePhase phase,
                               const RobloxExperienceLaunchRequest* request) {
  (void)context;
  (void)phase;
  (void)request;
}

}  // namespace mocktail::runtime
