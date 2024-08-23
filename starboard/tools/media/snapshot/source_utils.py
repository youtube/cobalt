#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
'''Utility functions to manipulate source file.'''

import os
import re


# starboard/player.h => STARBOARD_PLAYER_H_
def generate_include_guard_macro(project_root_dir, file_pathname):
  rel_pathname = os.path.relpath(file_pathname, project_root_dir)
  return rel_pathname.replace('/', '_').replace('.', '_').upper() + '_'


def extract_class_or_struct_names(content):
  pattern = r'^\s*class\s+(\w+)\s*[:{]'
  matches = re.findall(pattern, content, re.MULTILINE)

  pattern = r'^\s*struct\s+(\w+)\s*[:{]'
  return matches + re.findall(pattern, content, re.MULTILINE)


# This replace 'JNIEnv' to the content of new_text, but won't replace
# 'shared::JNIEnv'.
def replace_class_name_without_namespace(content, class_name, new_text):
  pattern = r'(?<!:)' + class_name
  return re.sub(pattern, new_text, content)


def add_namespace(content, class_name, namespace):
  assert len(namespace) > 0
  if namespace[-2:] != '::':
    namespace += '::'
  return replace_class_name_without_namespace(content, class_name,
                                              namespace + class_name)


def replace_class_under_namespace(content, class_name, old_namespace,
                                  new_namespace):
  pattern = rf'{old_namespace}::(\S*)?{class_name}'
  replacement = rf'{new_namespace}::\1{class_name}'
  return re.sub(pattern, replacement, content)


def is_header_file(file_pathname):
  return file_pathname[-2:] == '.h'


def add_header_file(content, header_file):
  is_project_header = header_file.find('/') >= 0

  if is_project_header:
    include_directive = '#include "' + header_file + '"'
    include_directive_prefix = '#include "'
    search_func = str.rfind
  else:
    include_directive = '#include <' + header_file + '>'
    include_directive_prefix = '#include <'
    if header_file.find('.'):
      search_func = str.find  # C header files first
    else:
      search_func = str.rfind  # C++ header files second

  if content.find(include_directive) >= 0:
    return content

  index = search_func(content, include_directive_prefix)
  if index >= 0:
    index = content.find('\n', index)
    return content[:index] + '\n' + include_directive + '\n' + content[index:]

  # Cannot find the group, add it to the very beginning and refine it manually
  return include_directive + '\n' + content


def is_trivially_modified_c24_file(input_pathname):
  pathnames = [
      'starboard/android/shared/audio_sink_min_required_frames_tester.cc',
      'starboard/android/shared/audio_track_audio_sink_type.cc',
      'starboard/android/shared/media_decoder.cc',
      'starboard/android/shared/video_max_video_input_size.cc',
      'starboard/shared/starboard/audio_sink/stub_audio_sink_type.cc',
      'starboard/shared/starboard/player/job_queue.cc',
      'starboard/shared/starboard/player/job_thread.cc',
      'starboard/shared/starboard/player/player_worker.cc',
  ]
  for pathname in pathnames:
    if input_pathname.find(pathname) > 0:
      return True

  return False


def update_starboard_usage(source_content):
  # The function can be further optimized using regex to conduct multiple
  # replaces at once, but the current implementation (i.e. replacing multiple
  # times) works.

  # SbTime
  source_content = re.sub(r'\bSbTime\b', 'int64_t', source_content)
  source_content = re.sub(r'\bSbTimeMonotonic\b', 'int64_t', source_content)
  source_content = re.sub(r'\bkSbTimeMillisecond\b', '1000LL', source_content)
  source_content = re.sub(r'\bkSbTimeNanosecondsPerMicrosecond\b', '1000LL',
                          source_content)
  source_content = re.sub(r'\bkSbTimeSecond\b', '1\'000\'000LL', source_content)
  if re.search(r'\bSbTimeGetMonotonicNow\b', source_content):
    source_content = re.sub(r'\bSbTimeGetMonotonicNow\b',
                            '::starboard::CurrentMonotonicTime', source_content)
    source_content = add_header_file(source_content, 'starboard/common/time.h')

  source_content = source_content.replace('kSbTimeMax', 'kSbInt64Max')
  source_content = source_content.replace('kSbTimeDay', 'kSbInt64Max')
  source_content = source_content.replace('#include "starboard/time.h"\n', '')

  # SbThread
  source_content = re.sub(r'\bSbThread\b', 'pthread_t', source_content)
  source_content = re.sub(r'\bkSbThreadInvalid\b', '0', source_content)
  source_content = re.sub(r'SbThreadIsValid\((.*?)\)', r'((\1) != 0)',
                          source_content)
  source_content = source_content.replace('SbThreadJoin(', 'pthread_join(')
  if re.search(r'\bSbThreadSleep\b', source_content):
    source_content = re.sub(r'\bSbThreadSleep\b', 'usleep', source_content)
    source_content = add_header_file(source_content, 'unistd.h')
  source_content = source_content.replace('SbThreadYield(', 'sched_yield(')

  # SbThreadLocal
  source_content = source_content.replace('kSbThreadLocalKeyInvalid', '0')
  source_content = source_content.replace('SbThreadLocalKey', 'pthread_key_t')
  source_content = source_content.replace('SbThreadSetLocalValue',
                                          'pthread_setspecific')

  # scoped_ptr<> and scoped_array<>
  source_content = source_content.replace(
      '#include "starboard/common/scoped_ptr.h"\n', '')
  source_content = add_header_file(source_content, 'memory')
  # The following replacement is very specific.  Including here so we don't have
  # to modify the generated code manually.
  source_content = source_content.replace(
      'return audio_decoder_impl.PassAs<AudioDecoderBase>();',
      ('return std::unique_ptr<AudioDecoderBase>(audio_decoder_impl.' +
       'release());'))

  source_content = re.sub(r'scoped_ptr<(\w*)>\(NULL\)',
                          r'std::unique_ptr<\1>()', source_content)
  source_content = re.sub(r'\bstarboard::scoped_ptr\b', 'std::unique_ptr',
                          source_content)
  source_content = re.sub(r'\bstarboard::make_scoped_ptr\b', 'std::unique_ptr',
                          source_content)
  source_content = re.sub(r'\bmake_scoped_ptr\b', 'std::unique_ptr',
                          source_content)
  source_content = re.sub(r'\bscoped_ptr\b', 'std::unique_ptr', source_content)
  source_content = re.sub(r'\b(\w*)\.Pass\(\)', r'std::move(\1)',
                          source_content)
  source_content = re.sub(r'scoped_array<(\w*)>', r'std::unique_ptr<\1[]>',
                          source_content)

  # SbOnce
  source_content = source_content.replace('#include "starboard/once.h"',
                                          '#include "starboard/common/once.h"')
  source_content = source_content.replace('SbOnceControl', 'pthread_once_t')
  source_content = source_content.replace('SbOnce(', 'pthread_once(')
  source_content = source_content.replace('SB_ONCE_INITIALIZER',
                                          'PTHREAD_ONCE_INIT')

  # string ops
  source_content = source_content.replace('SbStringScanF', 'sscanf')
  source_content = source_content.replace('SbStringCompareNoCase', 'strcasecmp')

  # reset_and_return.h
  source_content = source_content.replace(
      '#include "starboard/common/reset_and_return.h"\n', '')
  source_content = source_content.replace(
      'using common::ResetAndReturn;',
      ('template <typename T>\nT ResetAndReturn(T* t) {\n' +
       '  T result(std::move(*t));\n  *t = T();\n  return result;\n}'))

  return source_content
