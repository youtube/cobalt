# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
    '<(DEPTH)/starboard/stub/stub_sources.gypi'
  ],
  'targets': [
    {
      'target_name': 'starboard_platform',
      'type': 'static_library',
      'sources': [
        # TODO: Convert all stubs to mocks.
        '<@(stub_sources)',
        # Minimal Starboard implementation that allows to run unit tests.
        '<(DEPTH)/starboard/linux/shared/atomic_public.h',
        '<(DEPTH)/starboard/shared/iso/memory_allocate_unchecked.cc',
        '<(DEPTH)/starboard/shared/iso/memory_compare.cc',
        '<(DEPTH)/starboard/shared/iso/memory_copy.cc',
        '<(DEPTH)/starboard/shared/iso/memory_find_byte.cc',
        '<(DEPTH)/starboard/shared/iso/memory_free.cc',
        '<(DEPTH)/starboard/shared/iso/memory_move.cc',
        '<(DEPTH)/starboard/shared/iso/memory_reallocate_unchecked.cc',
        '<(DEPTH)/starboard/shared/iso/memory_set.cc',
        '<(DEPTH)/starboard/shared/linux/memory_get_stack_bounds.cc',
        '<(DEPTH)/starboard/shared/posix/log.cc',
        '<(DEPTH)/starboard/shared/posix/log_flush.cc',
        '<(DEPTH)/starboard/shared/posix/log_format.cc',
        '<(DEPTH)/starboard/shared/posix/log_is_tty.cc',
        '<(DEPTH)/starboard/shared/posix/log_raw.cc',
        '<(DEPTH)/starboard/shared/posix/memory_allocate_aligned_unchecked.cc',
        '<(DEPTH)/starboard/shared/posix/memory_free_aligned.cc',
        '<(DEPTH)/starboard/shared/starboard/application.cc',
        '<(DEPTH)/starboard/shared/starboard/command_line.cc',
        '<(DEPTH)/starboard/shared/starboard/command_line.h',
        '<(DEPTH)/starboard/shared/starboard/event_cancel.cc',
        '<(DEPTH)/starboard/shared/starboard/event_schedule.cc',
        '<(DEPTH)/starboard/shared/starboard/file_mode_string_to_flags.cc',
        '<(DEPTH)/starboard/shared/starboard/log_message.cc',
        '<(DEPTH)/starboard/shared/starboard/log_mutex.cc',
        '<(DEPTH)/starboard/shared/starboard/log_mutex.h',
        '<(DEPTH)/starboard/shared/starboard/log_raw_dump_stack.cc',
        '<(DEPTH)/starboard/shared/starboard/log_raw_format.cc',
        '<(DEPTH)/starboard/shared/starboard/media/codec_util.cc',
        '<(DEPTH)/starboard/shared/starboard/media/codec_util.h',
        '<(DEPTH)/starboard/shared/starboard/media/mime_type.cc',
        '<(DEPTH)/starboard/shared/starboard/media/mime_type.h',
        '<(DEPTH)/starboard/shared/starboard/queue_application.cc',
        '<(DEPTH)/starboard/shared/starboard/system_request_stop.cc',
        '<(DEPTH)/starboard/stub/application_stub.cc',
        '<(DEPTH)/starboard/stub/application_stub.h',
        'atomic_public.h',
        'main.cc',
        'thread_types_public.h',
      ],
      # Exclude unused stub sources.
      'sources!' : [
        '<(DEPTH)/starboard/shared/stub/atomic_public.h',
        '<(DEPTH)/starboard/shared/stub/log.cc',
        '<(DEPTH)/starboard/shared/stub/log_flush.cc',
        '<(DEPTH)/starboard/shared/stub/log_format.cc',
        '<(DEPTH)/starboard/shared/stub/log_is_tty.cc',
        '<(DEPTH)/starboard/shared/stub/log_raw.cc',
        '<(DEPTH)/starboard/shared/stub/log_raw_dump_stack.cc',
        '<(DEPTH)/starboard/shared/stub/log_raw_format.cc',
        '<(DEPTH)/starboard/shared/stub/memory_allocate_aligned_unchecked.cc',
        '<(DEPTH)/starboard/shared/stub/memory_allocate_unchecked.cc',
        '<(DEPTH)/starboard/shared/stub/memory_compare.cc',
        '<(DEPTH)/starboard/shared/stub/memory_copy.cc',
        '<(DEPTH)/starboard/shared/stub/memory_find_byte.cc',
        '<(DEPTH)/starboard/shared/stub/memory_free.cc',
        '<(DEPTH)/starboard/shared/stub/memory_free_aligned.cc',
        '<(DEPTH)/starboard/shared/stub/memory_get_stack_bounds.cc',
        '<(DEPTH)/starboard/shared/stub/memory_move.cc',
        '<(DEPTH)/starboard/shared/stub/memory_reallocate_unchecked.cc',
        '<(DEPTH)/starboard/shared/stub/memory_set.cc',
        '<(DEPTH)/starboard/shared/stub/system_request_stop.cc',
      ],
      'defines': [
        # This must be defined when building Starboard, and must not when
        # building Starboard client code.
        'STARBOARD_IMPLEMENTATION',
      ],
    },
  ],
}
