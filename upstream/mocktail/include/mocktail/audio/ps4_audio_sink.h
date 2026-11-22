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

#ifndef MOCKTAIL_AUDIO_PS4_AUDIO_SINK_H_
#define MOCKTAIL_AUDIO_PS4_AUDIO_SINK_H_

#include <memory>

#include "mocktail/audio/audio_sink.h"
#include "mocktail/audio/sdl_audio_sink.h"

namespace mocktail::audio {

// PS4 audio backend selection.
//
// The jailbroken PS4 exposes no standard audio API to homebrew, so audio
// depends on the audio driver bundled with the PS4 homebrew SDK's SDL3
// build. When SDL audio is unavailable, MockPort degrades to a silent sink
// so the Roblox client still launches (the client treats a failed audio
// device as "no sound" rather than a fatal error).
//
// Attempts SDL audio first; on failure installs a silent sink and reports
// which backend was selected through `backend_selected` ("sdl" or "silent").
Status CreatePs4AudioSink(const SdlAudioSinkOptions& options,
                          std::unique_ptr<AudioSink>* sink,
                          std::string* backend_selected);

// Silent no-op AudioSink used when no PS4 audio backend exists.
std::unique_ptr<AudioSink> CreateSilentAudioSink(const PcmSpec& source_spec);

}  // namespace mocktail::audio

#endif  // MOCKTAIL_AUDIO_PS4_AUDIO_SINK_H_
