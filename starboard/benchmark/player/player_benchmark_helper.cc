// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/benchmark/player/player_benchmark_helper.h"

#include <limits>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/experimental_features.h"

namespace starboard {
namespace benchmark {

namespace {
// A minimal Application implementation to satisfy SbPlayer dependencies
class MockApplication : public Application {
 public:
  MockApplication() : Application(DummyEventHandleCallback) {
    SetCommandLine(0, nullptr);
  }

  static void DummyEventHandleCallback(const SbEvent* event) {}

 protected:
  // --- Application overrides ---
  Event* GetNextEvent() override { return nullptr; }
  void Inject(Event* event) override { delete event; }
  void InjectTimedEvent(TimedEvent* timed_event) override {
    delete timed_event;
  }
  void CancelTimedEvent(SbEventId event_id) override {}
  TimedEvent* GetNextDueTimedEvent() override { return nullptr; }
  int64_t GetNextTimedEventTargetTime() override {
    return std::numeric_limits<int64_t>::max();
  }
};
}  // namespace

SbPlayerBenchmarkHelper::SbPlayerBenchmarkHelper() {
  mock_application_.reset(new MockApplication());
  StarboardExtensionExperimentalFeatures experimental_features = {};
  experimental_features.entries = nullptr;
  experimental_features.entry_count = 0;
  starboard::SetExperimentalFeaturesForCurrentThread(&experimental_features);
}

SbPlayerBenchmarkHelper::~SbPlayerBenchmarkHelper() {}

void SbPlayerBenchmarkHelper::GetCreationParam(
    SbPlayerCreationParam* out_creation_param,
    SbPlayerOutputMode output_mode) {
  SB_DCHECK(out_creation_param);

  *out_creation_param = {};
  out_creation_param->drm_system = kSbDrmSystemInvalid;
  out_creation_param->output_mode = output_mode;

  // Initialize a default stereo AAC audio config
  static const uint8_t kAacAudioSpecificConfig[16] = {18, 16};
  out_creation_param->audio_stream_info.codec = kSbMediaAudioCodecAac;
  out_creation_param->audio_stream_info.mime = "audio/mp4";
  out_creation_param->audio_stream_info.number_of_channels = 2;
  out_creation_param->audio_stream_info.samples_per_second = 44100;
  out_creation_param->audio_stream_info.bits_per_sample = 16;
  out_creation_param->audio_stream_info.audio_specific_config =
      kAacAudioSpecificConfig;
  out_creation_param->audio_stream_info.audio_specific_config_size =
      sizeof(kAacAudioSpecificConfig);

  // Initialize a default 1080p H.264 video config
  out_creation_param->video_stream_info.codec = kSbMediaVideoCodecH264;
  out_creation_param->video_stream_info.mime = "video/mp4";
  out_creation_param->video_stream_info.max_video_capabilities = "";
  out_creation_param->video_stream_info.color_metadata.bits_per_channel = 8;
  out_creation_param->video_stream_info.color_metadata.primaries =
      kSbMediaPrimaryIdBt709;
  out_creation_param->video_stream_info.color_metadata.transfer =
      kSbMediaTransferIdBt709;
  out_creation_param->video_stream_info.color_metadata.matrix =
      kSbMediaMatrixIdBt709;
  out_creation_param->video_stream_info.color_metadata.range =
      kSbMediaRangeIdLimited;
  out_creation_param->video_stream_info.frame_width = 1920;
  out_creation_param->video_stream_info.frame_height = 1080;
}

// Dummy callbacks
void SbPlayerBenchmarkHelper::DummyDeallocateSampleFunc(
    SbPlayer player,
    void* context,
    const void* sample_buffer) {}

void SbPlayerBenchmarkHelper::DummyDecoderStatusFunc(SbPlayer player,
                                                     void* context,
                                                     SbMediaType type,
                                                     SbPlayerDecoderState state,
                                                     int ticket) {}

void SbPlayerBenchmarkHelper::DummyPlayerStatusFunc(SbPlayer player,
                                                    void* context,
                                                    SbPlayerState state,
                                                    int ticket) {}

void SbPlayerBenchmarkHelper::DummyPlayerErrorFunc(SbPlayer player,
                                                   void* context,
                                                   SbPlayerError error,
                                                   const char* message) {}

}  // namespace benchmark
}  // namespace starboard
