# Copyright 2018 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
{
  'variables': {
    'filter_based_player_sources': [
      '<(DEPTH)/starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_channel_layout_mixer.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_channel_layout_mixer_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_decoder_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_frame_tracker.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_frame_tracker.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_internal_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_internal_impl.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_sink.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_sink_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_resampler.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_resampler_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_time_stretcher.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/audio_time_stretcher.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/common.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/cpu_video_frame.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/cpu_video_frame.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/decoded_audio_queue.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/decoded_audio_queue.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/filter_based_player_worker_handler.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/interleaved_sinc_resampler.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/interleaved_sinc_resampler.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/media_time_provider.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/media_time_provider_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/media_time_provider_impl.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/mock_audio_decoder.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/mock_audio_renderer_sink.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/player_components.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/player_components.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/punchout_video_renderer_sink.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/punchout_video_renderer_sink.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/stub_audio_decoder.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/stub_audio_decoder.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/stub_player_components_factory.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/stub_player_components_factory.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/stub_video_decoder.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/stub_video_decoder.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_decoder_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_frame_cadence_pattern_generator.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_frame_cadence_pattern_generator.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_frame_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_frame_rate_estimator.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_frame_rate_estimator.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_render_algorithm.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_render_algorithm_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_render_algorithm_impl.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_renderer_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_renderer_internal_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_renderer_internal_impl.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/video_renderer_sink.h',
      '<(DEPTH)/starboard/shared/starboard/player/filter/wsola_internal.cc',
      '<(DEPTH)/starboard/shared/starboard/player/filter/wsola_internal.h',
    ],
  },
}
