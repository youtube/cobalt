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

#include "media/starboard/experimental_features_unpacker.h"

#include "base/logging.h"

namespace media {

namespace {

// The value of 12 was chosen after experimentation was done. For more
// details, see b/497891900.
constexpr int kDefaultMaxSamplePerWrite = 12;

bool GetBoolFeature(const base::flat_map<std::string, int64_t>& features,
                    const char* key) {
  auto it = features.find(key);
  return it != features.end() && it->second != 0;
}

std::optional<int> ProcessRangedIntFeature(
    const base::flat_map<std::string, int64_t>& features,
    const char* key,
    int min_val,
    int max_val,
    int unset_sentinel = 0) {
  auto it = features.find(key);
  if (it == features.end()) {
    return std::nullopt;
  }
  int64_t val = it->second;
  if (val == unset_sentinel) {
    LOG(INFO) << "Value for " << key << " matches unset sentinel (" << val
              << "); falling back to system default.";
    return std::nullopt;
  }
  if (val < min_val || max_val < val) {
    LOG(WARNING) << "Invalid value for " << key << ": " << val;
    return std::nullopt;
  }
  return static_cast<int>(val);
}

int GetMaxSamplesPerWriteInternal(
    const base::flat_map<std::string, int64_t>& features) {
  auto it = features.find("Media.MaxSamplesPerWrite");
  if (it == features.end() || it->second == 0) {
    return kDefaultMaxSamplePerWrite;
  }
  if (it->second < 1 || it->second > 100000) {
    LOG(WARNING) << "Invalid value for Media.MaxSamplesPerWrite: "
                 << it->second;
    return kDefaultMaxSamplePerWrite;
  }
  return static_cast<int>(it->second);
}

const int* ToIntPointer(const std::optional<int>& val) {
  if (!val) {
    return nullptr;
  }
  return &*val;
}

const bool* ToBoolPointer(const std::optional<bool>& val) {
  if (!val) {
    return nullptr;
  }
  return &*val;
}

}  // namespace

ExperimentalFeaturesUnpacker::ExperimentalFeaturesUnpacker(
    const base::flat_map<std::string, int64_t>& features)
    : max_samples_per_write_(GetMaxSamplesPerWriteInternal(features)),
      force_decode_to_texture_(
          GetBoolFeature(features, "Media.ForceDecodeToTexture")),
      extension_features_({}) {
  if (auto it = features.find("Media.UseDualThreadsForVideo");
      it != features.end()) {
    use_dual_threads_for_video_ = (it->second != 0);
  }

  video_decoder_initial_preroll_count_ = ProcessRangedIntFeature(
      features, "Media.VideoDecoderInitialPrerollCount", 1, 100000);
  video_renderer_min_decoded_frames_ = ProcessRangedIntFeature(
      features, "Media.VideoRendererMinDecodedFrames", 1, 100000);
  video_renderer_min_input_buffers_ = ProcessRangedIntFeature(
      features, "Media.VideoRendererMinInputBuffers", 1, 100000);

  extension_features_.allow_audio_writing_on_pause =
      GetBoolFeature(features, "Media.AllowAudioWritingOnPause");
  extension_features_.disable_low_performance_sw_decoder =
      GetBoolFeature(features, "Media.DisableLowPerformanceSoftwareDecoder");
  extension_features_.enable_av1_startup_optimization =
      GetBoolFeature(features, "Media.EnableAv1StartupOptimization");
  extension_features_.enable_codec_output_checker =
      GetBoolFeature(features, "Media.EnableCodecOutputChecker");
  extension_features_.enable_video_renderer_vsp_adjustment =
      GetBoolFeature(features, "Media.EnableVideoRendererVspAdjustment");
  extension_features_.flush_decoder_during_reset =
      GetBoolFeature(features, "Media.EnableFlushDuringSeek");
  extension_features_.reset_audio_decoder =
      GetBoolFeature(features, "Media.EnableResetAudioDecoder");
  extension_features_.skip_flush_on_decoder_teardown =
      GetBoolFeature(features, "Media.SkipFlushOnDecoderTeardown");
  extension_features_.skip_video_frames_over_60_fps =
      GetBoolFeature(features, "Media.SkipVideoFramesOver60Fps");

  extension_features_.use_dual_threads_for_video =
      ToBoolPointer(use_dual_threads_for_video_);
  extension_features_.video_decoder_initial_preroll_count =
      ToIntPointer(video_decoder_initial_preroll_count_);
  extension_features_.video_renderer_min_decoded_frames =
      ToIntPointer(video_renderer_min_decoded_frames_);
  extension_features_.video_renderer_min_input_buffers =
      ToIntPointer(video_renderer_min_input_buffers_);
}

}  // namespace media
