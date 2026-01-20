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
"""Utility functions."""

import os


def is_media_file(pathname):
  # Image decoder and media session are not media
  if pathname.find('image') >= 0 or pathname.find('media_session') >= 0:
    return False

  # While these two are media files, they are unused on Android TV and cause
  # build errors.
  if pathname.find('media_get_buffer_storage_type.cc') >= 0:
    return False
  if pathname.find('punchout_video_renderer_sink.cc') >= 0:
    return False

  # These files should be the same among media implementations, and should be
  # manually consolidated if there are any differences
  # TODO: Print them to log if there are any differences
  if (pathname.find('player_create.cc') >= 0 or
      pathname.find('player_destroy.cc') >= 0):
    return False

  # Assume the function in starboard/common are the same across all versions.
  # Exclude them to avoid the extra handling required as they are under the root
  # starboard namespace and the functions are called directly without any
  # namespace specifier.
  if pathname.find('starboard/common/') >= 0:
    return False

  if (pathname.find('starboard/shared/starboard/audio_sink') >= 0 or
      pathname.find('starboard/shared/starboard/decode_target') >= 0 or
      pathname.find('starboard/shared/starboard/drm') >= 0 or
      pathname.find('starboard/shared/starboard/media') >= 0 or
      pathname.find('starboard/shared/starboard/opus') >= 0 or
      pathname.find('starboard/shared/starboard/player') >= 0 or
      pathname.find('starboard/shared/starboard/drm') >= 0):
    return True

  return (pathname.find('audio') >= 0 or pathname.find('decode') >= 0 or
          pathname.find('drm') >= 0 or pathname.find('media') >= 0 or
          pathname.find('player') >= 0 or pathname.find('video') >= 0)


def is_header_file(pathname):
  return pathname[-2:] == '.h'


# '.../starboard/android/shared/player_create.cc' => 'player_create', 'cc'
def get_base_file_name_and_ext(pathname):
  # '.../starboard/android/shared/player_create.cc' => player_create.cc
  basename = os.path.basename(pathname)

  return basename.split('.')


def base_name_to_sb_function_name(pathname):
  # 'player_create' => 'SbPlayerCreate'
  return 'Sb' + ''.join(x.capitalize() for x in pathname.split('_'))


# For prototype
#   const void* SbDrmGetMetrics(SbDrmSystem drm_system, int* size)
# returns
#   SbDrmGetMetrics(drm_system, size)
# i.e. remove the return type, and all types of parameters.
def get_calling_statement_from_prototype(prototype):
  # multiline to single line
  calling_statement = prototype.replace('\n', '')
  calling_statement = calling_statement.strip()

  # const void* SbDrmGetMetrics(SbDrmSystem drm_system, int* size) =>
  # SbDrmGetMetrics(SbDrmSystem drm_system, int* size)
  assert calling_statement.find(' Sb') != -1
  calling_statement = calling_statement[calling_statement.find(' Sb') + 1:]

  # SbDrmGetMetrics(SbDrmSystem drm_system, int* size) =>
  # SbDrmGetMetrics(drm_system, size)
  assert calling_statement[-1] == ')'
  calling_statement = calling_statement[:-1]
  name, parameters = calling_statement.split('(')

  arguments = []
  for parameter in parameters.split(','):
    arguments.append(parameter.split(' ')[-1])

  return name + '(' + ', '.join(arguments) + ')'


def read_file(pathname):
  with open(pathname, encoding='utf-8') as f:
    return f.read()


def write_file(pathname, content):
  with open(pathname, 'w+', encoding='utf-8') as f:
    f.write(content)
