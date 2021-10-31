# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
    'common_player_sources': [
      '<(DEPTH)/starboard/shared/starboard/player/decoded_audio_internal.cc',
      '<(DEPTH)/starboard/shared/starboard/player/decoded_audio_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/input_buffer_internal.cc',
      '<(DEPTH)/starboard/shared/starboard/player/input_buffer_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/job_queue.cc',
      '<(DEPTH)/starboard/shared/starboard/player/job_queue.h',
      '<(DEPTH)/starboard/shared/starboard/player/job_thread.cc',
      '<(DEPTH)/starboard/shared/starboard/player/job_thread.h',
      '<(DEPTH)/starboard/shared/starboard/player/player_create.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_destroy.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_get_current_frame.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_get_info2.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_get_maximum_number_of_samples_per_write.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_get_preferred_output_mode_prefer_punchout.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_internal.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_internal.h',
      '<(DEPTH)/starboard/shared/starboard/player/player_seek2.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_set_bounds.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_set_playback_rate.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_set_volume.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_worker.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_worker.h',
      '<(DEPTH)/starboard/shared/starboard/player/player_write_end_of_stream.cc',
      '<(DEPTH)/starboard/shared/starboard/player/player_write_sample2.cc',
    ],
  },
}
