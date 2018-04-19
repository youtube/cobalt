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
      'analog_thumbstick_input.cc',
      'analog_thumbstick_input.h',
      'analog_thumbstick_input_thread.cc',
      'analog_thumbstick_input_thread.h',
      'application_uwp_key_event.cc',
      'application_uwp_get_commandline_pointer.cc',
      'application_uwp.cc',
      'application_uwp.h',
      'async_utils.h',
      'get_home_directory.cc',
      'log_file_impl.cc',
      'log_file_impl.h',
      'log_raw.cc',
      'log_raw_format.cc',
      'log_writer_interface.h',
      'log_writer_uwp.cc',
      'log_writer_uwp.h',
      'log_writer_win32.cc',
      'log_writer_win32.h',
      'media_get_audio_configuration.cc',
      'media_is_audio_supported.cc',
      'media_is_output_protected.cc',
      'media_set_output_protection.cc',
      'sso.cc',
      'system_clear_platform_error.cc',
      'system_get_device_type.cc',
      'system_get_property.cc',
      'system_get_total_cpu_memory.cc',
      'system_get_used_cpu_memory.cc',
      'system_platform_error_internal.cc',
      'system_platform_error_internal.h',
      'system_raise_platform_error.cc',
      'wasapi_audio.cc',
      'wasapi_audio.h',
      'watchdog_log.cc',
      'watchdog_log.h',
      'window_create.cc',
      'window_destroy.cc',
      'window_get_platform_handle.cc',
      'window_get_size.cc',
      'window_internal.cc',
      'window_internal.h',
      'window_set_default_options.cc',
      'winrt_workaround.h',

      'private/keys.cc',
      'private/keys.h',

      # Microphone section.
      '<(DEPTH)/starboard/shared/uwp/microphone_impl.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_get_available.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_get_available.h',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_close.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_close.h',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_create.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_create.h',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_destroy.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_destroy.h',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_get_available.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_get_available.h',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_internal.h',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_is_sample_rate_supported.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_open.cc',
      '<(DEPTH)/starboard/shared/starboard/microphone/microphone_read.cc',

      '<(DEPTH)/starboard/shared/starboard/localized_strings.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_pause.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_stop.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_suspend.cc',
      '<(DEPTH)/starboard/shared/starboard/system_request_unpause.cc',
      '<(DEPTH)/starboard/shared/starboard/system_supports_resume.cc',
    ]
  }
}
