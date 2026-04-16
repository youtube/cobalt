// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/base/starboard/starboard_renderer_config.h"

#include "starboard/common/string.h"

namespace media {
using starboard::ToString;

StarboardRendererConfig::StarboardRendererConfig() = default;
StarboardRendererConfig::StarboardRendererConfig(
    const StarboardRendererConfig&) = default;
StarboardRendererConfig& StarboardRendererConfig::operator=(
    const StarboardRendererConfig&) = default;

StarboardRendererConfig::StarboardRendererConfig(
    const base::UnguessableToken& overlay_plane_id,
    base::TimeDelta audio_write_duration_local,
    base::TimeDelta audio_write_duration_remote,
    const std::string& max_video_capabilities,
    const ExperimentalFeatures& experimental_features,
    const gfx::Size& viewport_size)
    : overlay_plane_id(overlay_plane_id),
      audio_write_duration_local(audio_write_duration_local),
      audio_write_duration_remote(audio_write_duration_remote),
      max_video_capabilities(max_video_capabilities),
      experimental_features(experimental_features),
      viewport_size(viewport_size) {}

std::ostream& operator<<(
    std::ostream& os,
    const StarboardRendererConfig::ExperimentalFeatures& features) {
  return os << "{disable_low_performance_sw_decoder="
            << ToString(features.disable_low_performance_sw_decoder)
            << ", enable_av1_startup_optimization="
            << ToString(features.enable_av1_startup_optimization)
            << ", enable_flush_during_seek="
            << ToString(features.enable_flush_during_seek)
            << ", enable_reset_audio_decoder="
            << ToString(features.enable_reset_audio_decoder)
            << ", skip_flush_on_decoder_teardown="
            << ToString(features.skip_flush_on_decoder_teardown)
            << ", max_samples_per_write="
            << ToString(features.max_samples_per_write)
            << ", video_decoder_initial_preroll_count="
            << ToString(features.video_decoder_initial_preroll_count)
            << ", video_renderer_min_input_buffers="
            << ToString(features.video_renderer_min_input_buffers)
            << ", video_renderer_min_decoded_frames="
            << ToString(features.video_renderer_min_decoded_frames)
            << ", use_dual_threads_for_video="
            << ToString(features.use_dual_threads_for_video) << "}";
}

}  // namespace media
