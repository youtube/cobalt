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


def is_media_file(pathname):
  # Special cases
  if pathname.find('max_output_buffers_lookup_table') >= 0:
    return True
  if pathname.find('image') >= 0 or pathname.find('media_session') >= 0:
    return False
  # While these two are media files, they are unused on Android TV and cause
  # build errors.
  if pathname.find('media_get_buffer_storage_type.cc') >= 0:
    return False
  if pathname.find('punchout_video_renderer_sink.cc') >= 0:
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
