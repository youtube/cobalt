# Copyright 2017 Google Inc. All Rights Reserved.
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
  'includes': [
    '../shared/starboard_platform.gypi',
  ],
  'variables': {
    'base_win32_starboard_platform_dependent_files': [
      'atomic_public.h',
      'configuration_public.h',
      'thread_types_public.h',
      '<(DEPTH)/starboard/shared/starboard/localized_strings.cc',
      '<(DEPTH)/starboard/shared/starboard/localized_strings.cc',
      '<(DEPTH)/starboard/shared/starboard/queue_application.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_pause.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_pause.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_stop.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_stop.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_suspend.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_suspend.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_unpause.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_unpause.cc',
      '<(DEPTH)/starboard/shared/stub/media_is_output_protected.cc',
      '<(DEPTH)/starboard/shared/stub/media_set_output_protection.cc',
      '<(DEPTH)/starboard/win/shared/system_get_path.cc',
      '<@(uwp_incompatible_win32)',
    ],
  },
}
