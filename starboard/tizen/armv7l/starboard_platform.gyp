# Copyright 2016 Samsung Electronics. All Rights Reserved.
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
  'targets': [
    {
      'target_name': 'starboard_platform',
      'type': 'static_library',
      'sources': [
        '<(DEPTH)/starboard/shared/dlmalloc/memory_allocate_aligned_unchecked.cc',
        '<(DEPTH)/starboard/shared/dlmalloc/memory_allocate_unchecked.cc',
        '<(DEPTH)/starboard/shared/dlmalloc/memory_free.cc',
        '<(DEPTH)/starboard/shared/dlmalloc/memory_free_aligned.cc',
        '<(DEPTH)/starboard/shared/dlmalloc/memory_map.cc',
        '<(DEPTH)/starboard/shared/dlmalloc/memory_reallocate_unchecked.cc',
        '<(DEPTH)/starboard/shared/dlmalloc/memory_unmap.cc',
        '<(DEPTH)/starboard/shared/starboard/media/media_can_play_mime_and_key_system.cc',
        '<(DEPTH)/starboard/shared/starboard/media/media_is_output_protected.cc',
        '<(DEPTH)/starboard/shared/starboard/media/media_set_output_protection.cc',
        '<(DEPTH)/starboard/shared/starboard/media/mime_type.cc',
        '<(DEPTH)/starboard/shared/starboard/media/mime_type.h',
        '<(DEPTH)/starboard/shared/starboard/player/decoded_audio_internal.cc',
        '<(DEPTH)/starboard/shared/starboard/player/decoded_audio_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/audio_decoder_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_impl_internal.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_impl_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/audio_renderer_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/filter_based_player_worker_handler.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/player_components.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/video_decoder_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/filter/video_renderer_impl_internal.cc',
        '<(DEPTH)/starboard/shared/starboard/player/filter/video_renderer_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/input_buffer_internal.cc',
        '<(DEPTH)/starboard/shared/starboard/player/input_buffer_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/job_queue.cc',
        '<(DEPTH)/starboard/shared/starboard/player/job_queue.h',
        '<(DEPTH)/starboard/shared/starboard/player/player_create.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_destroy.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_get_info.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_internal.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_internal.h',
        '<(DEPTH)/starboard/shared/starboard/player/player_seek.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_set_bounds.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_set_pause.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_set_volume.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_worker.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_worker.h',
        '<(DEPTH)/starboard/shared/starboard/player/player_write_end_of_stream.cc',
        '<(DEPTH)/starboard/shared/starboard/player/player_write_sample.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_frame_internal.cc',
        '<(DEPTH)/starboard/shared/starboard/player/video_frame_internal.h',
        '<(DEPTH)/starboard/shared/stub/drm_close_session.cc',
        '<(DEPTH)/starboard/shared/stub/drm_create_system.cc',
        '<(DEPTH)/starboard/shared/stub/drm_destroy_system.cc',
        '<(DEPTH)/starboard/shared/stub/drm_generate_session_update_request.cc',
        '<(DEPTH)/starboard/shared/stub/drm_system_internal.h',
        '<(DEPTH)/starboard/shared/stub/drm_update_session.cc',
        '<(DEPTH)/starboard/shared/stub/media_is_supported.cc',
        '<(DEPTH)/starboard/tizen/shared/ffmpeg/ffmpeg_audio_decoder.cc',
        '<(DEPTH)/starboard/tizen/shared/ffmpeg/ffmpeg_audio_decoder.h',
        '<(DEPTH)/starboard/tizen/shared/ffmpeg/ffmpeg_common.cc',
        '<(DEPTH)/starboard/tizen/shared/ffmpeg/ffmpeg_common.h',
        '<(DEPTH)/starboard/tizen/shared/ffmpeg/ffmpeg_video_decoder.cc',
        '<(DEPTH)/starboard/tizen/shared/ffmpeg/ffmpeg_video_decoder.h',
        '<(DEPTH)/starboard/tizen/shared/main.cc',
        '<(DEPTH)/starboard/tizen/shared/player/filter/ffmpeg_player_components_impl.cc',
        'atomic_public.h',
        'configuration_public.h',
        'system_get_property.cc',
      ],
      'defines': [
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/starboard/common/common.gyp:common',
        '<(DEPTH)/starboard/tizen/shared/system.gyp:aul',
        '<(DEPTH)/starboard/tizen/shared/system.gyp:elementary',
        '<(DEPTH)/third_party/dlmalloc/dlmalloc.gyp:dlmalloc',
        'starboard_common.gyp:starboard_common',
      ],
    },
  ],
}
