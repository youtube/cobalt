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
  return os << "{allow_audio_writing_on_pause="
            << ToString(features.allow_audio_writing_on_pause)
            << ", bypass_mojo_for_media="
            << ToString(features.bypass_mojo_for_media)
            << ", enable_av1_startup_optimization="
            << ToString(features.enable_av1_startup_optimization)
            << ", enable_flush_during_seek="
            << ToString(features.enable_flush_during_seek)
            << ", enable_low_latency=" << ToString(features.enable_low_latency)
            << ", enable_reset_audio_decoder="
            << ToString(features.enable_reset_audio_decoder)
            << ", enable_video_renderer_vsp_adjustment="
            << ToString(features.enable_video_renderer_vsp_adjustment)
            << ", flush_audio_track_during_seek="
            << ToString(features.flush_audio_track_during_seek)
            << ", force_clear_surface_view="
            << ToString(features.force_clear_surface_view)
            << ", force_decode_to_texture="
            << ToString(features.force_decode_to_texture)
            << ", ignore_mediacodec_callbacks_during_flushing="
            << ToString(features.ignore_mediacodec_callbacks_during_flushing)
            << ", skip_flush_on_decoder_teardown="
            << ToString(features.skip_flush_on_decoder_teardown)
            << ", skip_video_frames_over_60_fps="
            << ToString(features.skip_video_frames_over_60_fps)
            << ", enable_trivial_optimizations="
            << ToString(features.enable_trivial_optimizations)
            << ", enable_simd_based_audio_format_switching="
            << ToString(features.enable_simd_based_audio_format_switching)
            << ", max_samples_per_write="
            << ToString(features.max_samples_per_write)
            << ", video_decoder_initial_preroll_count="
            << ToString(features.video_decoder_initial_preroll_count)
            << ", video_renderer_min_decoded_frames="
            << ToString(features.video_renderer_min_decoded_frames)
            << ", video_renderer_min_input_buffers="
            << ToString(features.video_renderer_min_input_buffers) << "}";
}

}  // namespace media
