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
   'starboard/android/shared/media_snapshot_<version>/'.  This helps with
   running multiple Starboard media implementations side by side.'''

import gn_utils
import os
import source_utils
import utils


class Snapshot:
  ''' Snapshot an SbPlayer implementation on Android TV

      It creates a snapshot of an SbPlayer implementation on Android TV
      specified by 'source_project_root_dir' and 'ninja_output_pathname' into
      '<destination_project_root_dir>/starboard/android/shared/
          <media_snapshot_version>'.

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
               ninja_output_pathname, media_snapshot_version):
    assert isinstance(media_snapshot_version, str)

    self.source_project_root_dir = os.path.abspath(
        os.path.expanduser(source_project_root_dir))
    self.destination_project_root_dir = os.path.abspath(
        os.path.expanduser(destination_project_root_dir))
    self.ninja_output_pathname = ninja_output_pathname
    self.media_snapshot_version = media_snapshot_version

    assert os.path.isdir(self.source_project_root_dir)
    assert os.path.isdir(
        os.path.join(self.source_project_root_dir, self.ninja_output_pathname))
    assert os.path.isdir(self.destination_project_root_dir)
    # The snapshot process may modify some of the source files.  Requiring a
    # separate checkout as source folder to avoid accidentally snapshotting
    # modified source files.
    assert not os.path.samefile(self.source_project_root_dir,
                                self.destination_project_root_dir)
    # Some of the logic relies on snapshotted files are inside a subfolder
    # containing 'media_snapshot/'.  This will break if the root dir of the
    # destination project happens to contain 'media_snapshot/'.
    assert self.destination_project_root_dir.find('media_snapshot/') == -1

  def _get_destination_pathname(self, source_pathname):
    # If the source is 'starboard/shared/starboard/media/media_util.cc', the
    # destination will be
    # 'starboard/android/shared/media_snapshot/<version>/media/media_util.cc'.
    # Note that the leading 'starboard/' of the source will be removed.
    source_rel_pathname = os.path.relpath(source_pathname,
                                          self.source_project_root_dir)
    assert source_rel_pathname.find('starboard' + os.path.sep) == 0
    source_rel_pathname = source_rel_pathname[len('starboard') + 1:]

    return os.path.join(self.destination_project_root_dir,
                        self._DESTINATION_SUB_DIR,
                        'media_snapshot/' + self.media_snapshot_version,
                        source_rel_pathname)

  def snapshot(self):
    self.try_create_canonical_sb_implementation()

    source_files = []

    for target in self._GN_TARGETS:
      source_files += gn_utils.get_source_pathnames(
          self.source_project_root_dir, self.ninja_output_pathname, target)

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
    self._create_snapshot_gn_file(destination_files)

    android_gn_pathname = os.path.join(self.destination_project_root_dir,
                                       'starboard/android/shared/BUILD.gn')
    android_gn_content = utils.read_file(android_gn_pathname)

    assert android_gn_content.find(
        '"//starboard/android/shared/media_snapshot",') != -1

    gn_target = ('//starboard/android/shared/media_snapshot/' +
                 self.media_snapshot_version)

    # Check if there is already a reference
    if android_gn_content.find(gn_target) == -1:
      android_gn_content = android_gn_content.replace(
          '"//starboard/android/shared/media_snapshot",',
          '"//starboard/android/shared/media_snapshot",\n    "' + gn_target +
          '",')

      utils.write_file(android_gn_pathname, android_gn_content)

    self.amend_canonical_sb_implementations()

  def try_create_canonical_sb_implementation(self):
    android_gn_pathname = os.path.join(self.destination_project_root_dir,
                                       'starboard/android/shared/BUILD.gn')
    canonical_snapshot_dir = os.path.join(self.destination_project_root_dir,
                                          self._DESTINATION_SUB_DIR,
                                          'media_snapshot/')

    android_gn_content = utils.read_file(android_gn_pathname)

    # Ensure that we are operating on the correct file
    assert android_gn_content.find('":starboard_base_symbolize",') != -1
    assert android_gn_content.find('snapshotted_media_files = [') != -1

    if android_gn_content.find('snapshotted_media_files = []') == -1:
      print('Canonical snapshot already created.')
      assert os.path.isfile(
          os.path.join(canonical_snapshot_dir, 'audio_sink_create.cc'))
      return

    print('Creating canonical snapshot ...')
    sb_implementation_files = []

    for target in self._GN_TARGETS:
      for pathname in gn_utils.get_source_pathnames(
          self.destination_project_root_dir, self.ninja_output_pathname,
          target):
        if not utils.is_media_file(pathname):
          continue
        # The canonical implementation is based on the primary implementation
        if pathname.find('media_snapshot/') != -1:
          continue

        content = utils.read_file(pathname)
        if source_utils.is_sb_implementation_file(pathname, content):
          sb_implementation_files.append(pathname)

    # We haven't snapshotted yet, `sb_implementation_files` shouldn't be empty
    assert len(sb_implementation_files) > 0

    # Now create the canonical implementation inside
    # 'starboard/android/shared/media_snapshot'.  Note that any non-canonical
    # snapshot will be put into a subfolder of it.
    source_file_pathnames = []
    for pathname in sb_implementation_files:
      destination_pathname = os.path.join(canonical_snapshot_dir,
                                          os.path.basename(pathname))
      source_file_pathnames.append(destination_pathname)
      source_utils.create_canonical_file(self.destination_project_root_dir,
                                         pathname, destination_pathname)

    # Create a gn file referring to the above source files
    gn_utils.create_gn_file(self.destination_project_root_dir,
                            os.path.join(canonical_snapshot_dir + 'BUILD.gn'),
                            'media_snapshot', source_file_pathnames)

    # Exclude the original source files from 'starboard/android/shared/BUILD.gn'
    sb_implementation_files = gn_utils.convert_source_list_to_gn_format(
        self.destination_project_root_dir, android_gn_pathname,
        sb_implementation_files)
    android_gn_content = android_gn_content.replace(
        'snapshotted_media_files = []', 'snapshotted_media_files = [\n    ' +
        '\n    '.join(sb_implementation_files) + '\n  ]')

    # Add reference to canonical snapshot
    android_gn_content = android_gn_content.replace(
        '":starboard_base_symbolize",',
        ('":starboard_base_symbolize",\n' +
         '    "//starboard/android/shared/media_snapshot",'))

    utils.write_file(android_gn_pathname, android_gn_content)

  def amend_canonical_sb_implementations(self):
    sb_implementation_files = []

    for pathname in gn_utils.get_source_pathnames(
        self.destination_project_root_dir, self.ninja_output_pathname,
        '//starboard/android/shared/media_snapshot'):
      if not utils.is_media_file(pathname):
        continue
      # We only amend the canonical implementation
      if pathname.find('media_snapshot/') == -1:
        continue

      content = utils.read_file(pathname)
      if source_utils.is_sb_implementation_file(pathname, content):
        sb_implementation_files.append(pathname)

    assert len(sb_implementation_files) > 0
    print(sb_implementation_files)

    for pathname in sb_implementation_files:
      with open(pathname, encoding='utf-8') as f:
        content = f.read()
      content = source_utils.patch_sb_implementation_with_branching_call(
          pathname, content, self.media_snapshot_version)
      with open(pathname, 'w+', encoding='utf-8') as f:
        f.write(content)

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
                              'namespace shared_' + self.media_snapshot_version)

    for class_name in class_names:
      content = source_utils.replace_class_under_namespace(
          content, class_name, 'shared',
          'shared_' + self.media_snapshot_version)

    for symbol_name in [
        'AudioDurationToFrames', 'AudioFramesToDuration',
        'CanPlayMimeAndKeySystem', 'ErrorCB', 'EndedCB',
        'GetAudioConfiguration', 'GetBytesPerSample',
        'GetMaxVideoInputSizeForCurrentThread', 'IsSDRVideo', 'IsWidevineL1',
        'IsWidevineL3', 'PrerolledCB', 'SetMaxVideoInputSizeForCurrentThread'
    ]:
      content = source_utils.replace_class_under_namespace(
          content, symbol_name, 'shared',
          'shared_' + self.media_snapshot_version)

    content = content.replace(
        'starboard::shared::starboard::player', 'starboard::shared_' +
        self.media_snapshot_version + '::starboard::player')

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

    # Replace JNI calls
    for jni_prefix in [
        'Java_dev_cobalt_media_MediaCodecBridge',
        'Java_dev_cobalt_media_MediaDrmBridge'
    ]:
      content = content.replace(jni_prefix,
                                jni_prefix + self.media_snapshot_version)

    if source_utils.is_sb_implementation_file(source_pathname, content):
      content = source_utils.append_suffix_to_sb_function(
          source_pathname, content, self.media_snapshot_version)

    with open(destination_pathname, 'w+', encoding='utf-8') as f:
      f.write(source_utils.update_starboard_usage(content))

  def _create_snapshot_gn_file(self, file_pathnames):
    gn_utils.create_gn_file(
        self.destination_project_root_dir,
        self._get_destination_pathname(
            os.path.join(self.source_project_root_dir, 'starboard/BUILD.gn')),
        self.media_snapshot_version, file_pathnames)


snapshot = Snapshot('~/cobalt_c25', '~/cobalt', 'out/android-arm_devel', '2500')
snapshot.snapshot()
