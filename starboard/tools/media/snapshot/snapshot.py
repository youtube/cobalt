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
'''Create a snapshot of an Starboard Android TV implementation under
   'starboard/android/shared/media_<version>/'.  This helps with running
   multiple Starboard media implementations side by side.'''

import gn_utils
import os
import source_utils
import utils


class Snapshot:
  ''' Snapshot an SbPlayer implementation on Android TV

      It creates a snapshot of an SbPlayer implementation on Android TV
      specified by 'source_project_root_dir' and 'ninja_output_pathname' into
      '<destination_project_root_dir>/starboard/android/shared/<media_version>'.

      The snapshot can be used side by side with the default SbPlayer
      implementation on Android TV of '<destination_project_root_dir>'.'''

  _GN_TARGETS = [
      '//starboard/common:common',
      '//starboard/android/shared:starboard_platform',
      '//starboard/shared/starboard/media:media_util',
      '//starboard/shared/starboard/player/filter:filter_based_player_sources',
  ]

  _DESTINATION_SUB_DIR = 'starboard/android/shared'

  def __init__(self, source_project_root_dir, destination_project_root_dir,
               ninja_output_pathname, media_version):
    assert isinstance(media_version, str)

    self.source_project_root_dir = os.path.abspath(
        os.path.expanduser(source_project_root_dir))
    self.destination_project_root_dir = os.path.abspath(
        os.path.expanduser(destination_project_root_dir))
    self.ninja_output_pathname = ninja_output_pathname
    self.media_version = media_version

    assert os.path.isdir(self.source_project_root_dir)
    assert os.path.isdir(
        os.path.join(self.source_project_root_dir, self.ninja_output_pathname))
    assert os.path.isdir(self.destination_project_root_dir)

  def _get_destination_pathname(self, source_pathname):
    # If the source is 'starboard/shared/starboard/media/media_util.cc', the
    # destination will be
    # 'starboard/android/shared/<media_version>/media/media_util.cc'.
    # Note that the leading 'starboard/' of the source will be removed.
    source_rel_pathname = os.path.relpath(source_pathname,
                                          self.source_project_root_dir)
    assert source_rel_pathname.find('starboard' + os.path.sep) == 0
    source_rel_pathname = source_rel_pathname[len('starboard') + 1:]

    return os.path.join(self.destination_project_root_dir,
                        self._DESTINATION_SUB_DIR,
                        'media_' + self.media_version, source_rel_pathname)

  def snapshot(self):
    source_files = []

    for target in self._GN_TARGETS:
      source_files += gn_utils.get_source_pathnames(
          self.source_project_root_dir, self.ninja_output_pathname, target)

    source_files.append(
        os.path.abspath(
            os.path.join(
                self.source_project_root_dir,
                'starboard/shared/starboard/media/media_support_internal.h')))
    source_files = [f for f in source_files if utils.is_media_file(f)]

    source_files.sort()

    destination_files = []

    class_names = []
    headers_dict = {}
    self.is_source_c24 = False

    for source_file in source_files:
      destination_file = self._get_destination_pathname(source_file)
      if source_utils.is_header_file(source_file):
        headers_dict[os.path.relpath(
            source_file, self.source_project_root_dir)] = os.path.relpath(
                destination_file, self.destination_project_root_dir)
        with open(source_file, encoding='utf-8') as f:
          content = f.read()
          # SbTime is deprecated in C25, use it as a hint to determine whether
          # the source is C24.
          if content.find(' SbTime ') >= 0:
            self.is_source_c24 = True
          class_names += source_utils.extract_class_or_struct_names(content)

      destination_files.append(destination_file)

    class_names.sort()

    for i in range(0, len(source_files)):
      self.snapshot_file(source_files[i], destination_files[i], class_names,
                         headers_dict)

    destination_files.sort()
    self._create_gn_file(destination_files)

  def snapshot_file(self, source_pathname, destination_pathname, class_names,
                    headers_dict):
    print('snapshotting', source_pathname, '=>', destination_pathname)

    if (self.is_source_c24 and
        source_utils.is_trivially_modified_c24_file(source_pathname)):
      # These files are trivially modified between C24 and C25, and they
      # contain changes hard to replace automatically (e.g. SbThreadCreate()).
      # Use the destination implementation of these files instead, and we should
      # manually diff these files offline to ensure that such replacement is
      # safe.
      rel_pathname = os.path.relpath(source_pathname,
                                     self.source_project_root_dir)
      canonical_pathname = os.path.join(self.destination_project_root_dir,
                                        rel_pathname)
      with open(canonical_pathname, encoding='utf-8') as f:
        content = f.read()
    else:
      with open(source_pathname, encoding='utf-8') as f:
        content = f.read()

    if utils.is_header_file(source_pathname):
      source_macro = source_utils.generate_include_guard_macro(
          self.source_project_root_dir, source_pathname)
      destination_macro = source_utils.generate_include_guard_macro(
          self.destination_project_root_dir, destination_pathname)
      assert content.find(source_macro) > 0
      content = content.replace(source_macro, destination_macro)
      assert content.find(source_macro) < 0

    if not os.path.isdir(os.path.dirname(destination_pathname)):
      os.makedirs(os.path.dirname(destination_pathname))

    for source in headers_dict:
      content = content.replace('#include "' + source + '"',
                                '#include "' + headers_dict[source] + '"')

    content = content.replace('namespace shared',
                              'namespace shared_' + self.media_version)

    for class_name in class_names:
      content = source_utils.replace_class_under_namespace(
          content, class_name, 'shared', 'shared_' + self.media_version)

    for symbol_name in [
        'AudioDurationToFrames', 'AudioFramesToDuration',
        'CanPlayMimeAndKeySystem', 'ErrorCB', 'EndedCB',
        'GetAudioConfiguration', 'GetBytesPerSample',
        'GetMaxVideoInputSizeForCurrentThread', 'IsSDRVideo', 'IsWidevineL1',
        'IsWidevineL3', 'PrerolledCB', 'SetMaxVideoInputSizeForCurrentThread'
    ]:
      content = source_utils.replace_class_under_namespace(
          content, symbol_name, 'shared', 'shared_' + self.media_version)

    content = content.replace(
        'starboard::shared::starboard::player',
        'starboard::shared_' + self.media_version + '::starboard::player')

    # The following replacements are very specific.  Including here so we don't
    # have to modify the generated code manually.
    content = content.replace(' ThreadChecker ',
                              ' shared::starboard::ThreadChecker ')
    content = content.replace(
        ' Application::Get',
        ' ::starboard::shared::starboard::Application::Get')
    content = content.replace('worker_ = starboard::make_scoped_ptr(',
                              'worker_.reset(')

    content = content.replace('#include "third_party/opus/include',
                              '#include "third_party/opus/src/include')

    content = source_utils.add_namespace(content, 'JniEnvExt',
                                         '::starboard::android::shared')
    content = source_utils.add_namespace(content, 'ScopedJavaByteBuffer',
                                         '::starboard::android::shared')
    content = source_utils.add_namespace(content, 'ScopedLocalJavaRef',
                                         '::starboard::android::shared')

    with open(destination_pathname, 'w+', encoding='utf-8') as f:
      f.write(source_utils.update_starboard_usage(content))

  def _create_gn_file(self, file_pathnames):
    gn_utils.create_gn_file(
        self.destination_project_root_dir,
        self._get_destination_pathname(
            os.path.join(self.source_project_root_dir, 'starboard/BUILD.gn')),
        'media_' + self.media_version, file_pathnames)


snapshot = Snapshot('~/cobalt_c24', '.', 'out/android-arm_devel', '2400')
snapshot.snapshot()
