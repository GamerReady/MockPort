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

#include "mocktail/audio/ps4_audio_sink.h"

#include <utility>

namespace mocktail::audio {
namespace {

// AudioSink contract: a successful Enqueue borrows the buffer until exactly
// one release callback fires, including on Clear() and Shutdown(). The silent
// sink consumes buffers immediately by invoking that callback.
class SilentAudioSink final : public AudioSink {
 public:
  explicit SilentAudioSink(PcmSpec spec) : source_spec_(std::move(spec)) {}

  const PcmSpec& source_spec() const override { return source_spec_; }

  Status Enqueue(const PcmBuffer& buffer) override {
    if (buffer.release_callback != nullptr) {
      buffer.release_callback(buffer.release_context, buffer.data,
                              buffer.size_bytes);
    }
    return Status::Ok();
  }

  Status Pause() override { return Status::Ok(); }
  Status Resume() override { return Status::Ok(); }
  Status SetGain(float /*linear_gain*/) override { return Status::Ok(); }
  Status Flush() override { return Status::Ok(); }
  Status Clear() override { return Status::Ok(); }

  Status GetQueuedBytes(std::size_t* size_bytes) const override {
    if (size_bytes != nullptr) {
      *size_bytes = 0;
    }
    return Status::Ok();
  }

  void Shutdown() override {}

 private:
  PcmSpec source_spec_;
};

}  // namespace

std::unique_ptr<AudioSink> CreateSilentAudioSink(const PcmSpec& source_spec) {
  return std::make_unique<SilentAudioSink>(source_spec);
}

Status CreatePs4AudioSink(const SdlAudioSinkOptions& options,
                          std::unique_ptr<AudioSink>* sink,
                          std::string* backend_selected) {
  if (sink == nullptr) {
    return Status::Error(StatusCode::kInvalidArgument,
                         "CreatePs4AudioSink requires a sink output");
  }
  sink->reset();
  if (backend_selected != nullptr) {
    backend_selected->clear();
  }

  const Status init_status = InitializeSdlAudioSubsystem();
  if (init_status.ok()) {
    const Status create_status = CreateSdlAudioSink(options, sink);
    if (create_status.ok()) {
      // The SDL audio subsystem stays initialized for the lifetime of the
      // sink; the audio bridge tears it down during shutdown.
      if (backend_selected != nullptr) {
        *backend_selected = "sdl";
      }
      return Status::Ok();
    }
    // SDL audio initialized but the sink could not be opened; tear the
    // subsystem back down before falling back to silence.
    (void)ShutdownSdlAudioSubsystem();
  }

  *sink = CreateSilentAudioSink(options.source_spec);
  if (backend_selected != nullptr) {
    *backend_selected = "silent";
  }
  return Status::Ok();
}

}  // namespace mocktail::audio
