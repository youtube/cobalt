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
  'variables': {
    'starboard_platform_dependent_files': [
      'application_uwp.cc',
      'application_uwp.h',
      'application_uwp_key_event.cc',
      'async_utils.h',
      'system_get_property.cc',
      'window_create.cc',
      'window_destroy.cc',
      'window_get_platform_handle.cc',
      'window_get_size.cc',
      'window_set_default_options.cc',
      'window_internal.cc',
      'window_internal.h',
      'winrt_workaround.h',
      '<(DEPTH)/starboard/shared/starboard/system_request_pause.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_stop.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_suspend.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_unpause.cc',
    ]
  }
}
