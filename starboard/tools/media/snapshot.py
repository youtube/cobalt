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
      specified by 'source_project_root_dir' and `_BUILD_CONFIG` into
      '<destination_project_root_dir>/starboard/android/shared/
          <media_snapshot_version>'.

      The snapshot can be used side by side with the default SbPlayer
      implementation on Android TV of '<destination_project_root_dir>'.'''

  _BUILD_CONFIG = 'out/android-arm_devel'

  _GN_TARGETS = [
      '//starboard/common:common',
      '//starboard/android/shared:starboard_platform',
      '//starboard/shared/starboard/media:media_util',
      '//starboard/shared/starboard/player/filter:filter_based_player_sources',
  ]

  _DESTINATION_SUB_DIR = 'starboard/android/shared'

  def __init__(self, source_project_root_dir, destination_project_root_dir,
               media_snapshot_version, verbose):
    ''' Constructs an object used to create a snapshot of an SbPlayer
        implementation on Android TV specified by 'source_project_root_dir'
        and `_BUILD_CONFIG` into '<destination_project_root_dir>/
          starboard/android/shared/<media_snapshot_version>'.

        For example, when called with '~/chromium_clean/src', '~/chromium/src',
        'out/android-arm_devel', and '2500', the object will create an Android
        TV SbPlayer implementation under the destination project inside
        '~/chromium/src/starboard/android/shared/media_snapshot_2500' according
        to the files used in the source project using android-arm devel build.
        It will also adjust gn references to the newly created project.

        Note that gn.py has to be called on both projects before calling this
        function. '''

    print('Creating snapshot from', source_project_root_dir, 'to',
          destination_project_root_dir, '...')

    # In case an integer version like 2500 is passed in accidentally.
    assert isinstance(media_snapshot_version, str)

    self.source_project_root_dir = os.path.abspath(
        os.path.expanduser(source_project_root_dir))
    self.destination_project_root_dir = os.path.abspath(
        os.path.expanduser(destination_project_root_dir))
    self.media_snapshot_version = media_snapshot_version
    self.verbose = verbose

    assert os.path.isdir(self.source_project_root_dir)
    assert os.path.isdir(
        os.path.join(self.source_project_root_dir, self._BUILD_CONFIG))
    assert os.path.isdir(self.destination_project_root_dir)
    # The snapshot process may modify some of the destination files.  Requiring
    # a separate checkout as source folder to avoid accidentally snapshotting
    # modified source files.
    assert not os.path.samefile(self.source_project_root_dir,
                                self.destination_project_root_dir)
    # Some of the logic relies on snapshotted files are inside a subfolder
    # containing 'media_snapshot/'.  This will break if in the unlikely case
    # that the root dir of the destination project contains the string
    # 'media_snapshot/'.
    assert self.destination_project_root_dir.find('media_snapshot/') == -1

  def _get_destination_pathname(self, source_pathname):
    ''' If the source is 'starboard/shared/starboard/media/media_util.cc', the
        destination will be
        'starboard/android/shared/media_snapshot/<version>/shared/starboard/
          media/media_util/media_util.cc'.
        Note that the leading 'starboard/' of the source will be removed. '''

    source_rel_pathname = os.path.relpath(source_pathname,
                                          self.source_project_root_dir)
    # Only works with Starboard implementation files
    assert source_rel_pathname.find('starboard' + os.path.sep) == 0

    source_rel_pathname = source_rel_pathname[len('starboard') + 1:]

    return os.path.join(self.destination_project_root_dir,
                        self._DESTINATION_SUB_DIR,
                        'media_snapshot/' + self.media_snapshot_version,
                        source_rel_pathname)

  def snapshot(self):
    print('Creating media snapshot', self.media_snapshot_version, '...')

    self._try_create_canonical_sb_implementation()

    print('Gathering source files ...')
    source_files = gn_utils.get_source_pathnames(self.source_project_root_dir,
                                                 self._BUILD_CONFIG,
                                                 self._GN_TARGETS)
    source_files = [f for f in source_files if utils.is_media_file(f)]
    source_files.sort()

    destination_files = []

    class_names = []
    headers_dict = {}

    for source_file in source_files:
      destination_file = self._get_destination_pathname(source_file)
      if source_utils.is_header_file(source_file):
        headers_dict[os.path.relpath(
            source_file, self.source_project_root_dir)] = os.path.relpath(
                destination_file, self.destination_project_root_dir)
        with open(source_file, encoding='utf-8') as f:
          content = f.read()
          class_names += source_utils.extract_class_or_struct_names(content)

      destination_files.append(destination_file)

    class_names.sort()

    print('Snapshotting files ...')
    assert len(source_files) == len(destination_files)
    for source_file, destination_file in zip(source_files, destination_files):
      self._snapshot_file(source_file, destination_file, class_names,
                          headers_dict)

    print('Generating and amending gn files ...')
    # Ensure that it's no longer used, as it's no longer matched with
    # `destination_files` in order after it's sorted below.
    source_files = None

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

    self._amend_canonical_sb_implementations()

  def _try_create_canonical_sb_implementation(self):
    ''' The canonical Starboard implementation contains files for Sb functions,
        e.g. 'player_create.cc' or 'drm_create_system.cc'.  They are supposed to
        be used without any modifications so they can serve as the reference
        during experimentation.
        Instead of using the files in the destination folder directly, we make a
        copy of these files to `starboard/android/shared/media_snapshot/`
        (without any versions), so we can add dispatching code to them, i.e.

          if (GetMediaSnapshotVersion() == 2500) {
            return SbPlayerCreate2500();
          }
          // Follow the existing implementation

        When a file is copied as canonical implementation, the original file
        will be excluded from build in `starboard/android/shared/BUILD.gn`
        through `snapshotted_media_files`.

        To simplify implementation, the canonical implementation actually
        includes the current implementation using #include directive.
    '''
    android_gn_pathname = os.path.join(self.destination_project_root_dir,
                                       'starboard/android/shared/BUILD.gn')
    canonical_snapshot_dir = os.path.join(self.destination_project_root_dir,
                                          self._DESTINATION_SUB_DIR,
                                          'media_snapshot/')

    android_gn_content = utils.read_file(android_gn_pathname)

    # Sanity check that we are operating on the correct gn file.
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
          self.destination_project_root_dir, self._BUILD_CONFIG, target):
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

      if not os.path.isdir(os.path.dirname(destination_pathname)):
        os.makedirs(os.path.dirname(destination_pathname))

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

  def _amend_canonical_sb_implementations(self):
    ''' Amend the canonical implementation with branching call to the new
        version, e.g.
        SbPlayer SbPlayerCreate(...) {
          if (GetMediaSnapshotVersion() == 2500) {
            return SbPlayerCreate2500(...);
          }
          // Follow the existing implementation
          return ...;
        }

        When a file is copied as canonical implementation, the original file
        will be excluded from build in `starboard/android/shared/BUILD.gn`
        through `snapshotted_media_files`.
    '''
    sb_implementation_files = []

    for pathname in gn_utils.get_source_pathnames(
        self.destination_project_root_dir, self._BUILD_CONFIG,
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
    if self.verbose:
      print('Starboard implementation files', sb_implementation_files)

    for pathname in sb_implementation_files:
      with open(pathname, encoding='utf-8') as f:
        content = f.read()
      content = source_utils.patch_sb_implementation_with_branching_call(
          pathname, content, self.media_snapshot_version, self.verbose)
      with open(pathname, 'w+', encoding='utf-8') as f:
        f.write(content)

  def _snapshot_file(self, source_pathname, destination_pathname, class_names,
                     headers_dict):
    if self.verbose:
      print('snapshotting', source_pathname, '=>', destination_pathname)

    with open(source_pathname, encoding='utf-8') as f:
      content = f.read()

    # Ensure that the destination dir exists to avoid file creation failure.
    if not os.path.isdir(os.path.dirname(destination_pathname)):
      os.makedirs(os.path.dirname(destination_pathname))

    # For header files, refine the include guard macro to reflect its new path
    # name.
    if utils.is_header_file(source_pathname):
      source_macro = source_utils.generate_include_guard_macro(
          self.source_project_root_dir, source_pathname)
      destination_macro = source_utils.generate_include_guard_macro(
          self.destination_project_root_dir, destination_pathname)
      assert content.find(source_macro) > 0
      content = content.replace(source_macro, destination_macro)
      assert content.find(source_macro) < 0

    # `headers_dict` contains a map between the old path name and the new path
    # name of a header file.  Update the current file so it includes the header
    # file by its new path name.
    for source in headers_dict:
      content = content.replace('#include "' + source + '"',
                                '#include "' + headers_dict[source] + '"')

    content = content.replace('namespace shared',
                              'namespace shared_' + self.media_snapshot_version)

    # Namespace magic to replace symbols like `...::shared::SomeClass`
    # with `...::shared_<snapshot_version>::SomeClass`.
    for class_name in class_names:
      content = source_utils.replace_class_under_namespace(
          content, class_name, 'shared',
          'shared_' + self.media_snapshot_version)

    # It's hard to identify functions in the snapshotted namespace, so we
    # explicitly list the functions below and replace them.
    for symbol_name in [
        'AudioDurationToFrames',
        'AudioFramesToDuration',
        'CanPlayMimeAndKeySystem',
        'EndedCB',
        'ErrorCB',
        'GetAudioConfiguration',
        'GetBytesPerSample',
        'GetMaxVideoInputSizeForCurrentThread',
        'IsSDRVideo',
        'IsWidevineL1',
        'IsWidevineL3',
        'PrerolledCB',
        'SetMaxVideoInputSizeForCurrentThread',
    ]:
      content = source_utils.replace_class_under_namespace(
          content, symbol_name, 'shared',
          'shared_' + self.media_snapshot_version)

    content = content.replace(
        'starboard::shared::starboard::player', 'starboard::shared_' +
        self.media_snapshot_version + '::starboard::player')

    # All media related symbols are resolved above.  For media code in
    # ::starboard::android::shared, where they can refer to no media symbols in
    # the same namespace directly, they won't be able to do so after moved into
    # ::starboard::android::shared_<snapshot_version>, so we have to refer to
    # the symbol with explicit namespace.
    # There are no straight-forward way to collect these symbols automatically,
    # and we add them below when we encounter any build errors, so we don't have
    # to modify the generated code manually.
    content = content.replace(' StarboardBridge::GetInstance()',
                              ' shared::StarboardBridge::GetInstance()')
    content = content.replace(' ThreadChecker ',
                              ' shared::starboard::ThreadChecker ')
    content = content.replace(
        ' Application::Get',
        ' ::starboard::shared::starboard::Application::Get')
    content = content.replace('worker_ = starboard::make_scoped_ptr(',
                              'worker_.reset(')

    content = source_utils.add_namespace(content, 'JniEnvExt',
                                         '::starboard::android::shared')
    content = source_utils.add_namespace(content, 'ScopedJavaByteBuffer',
                                         '::starboard::android::shared')
    content = source_utils.add_namespace(content, 'ScopedLocalJavaRef',
                                         '::starboard::android::shared')

    # JNI calls are in the global namespace, and the duplicated copies in the
    # snapshot and the canonical implementation lead to link errors.
    # TODO: We still have to address them to ensure that JNI calls are handled
    #       by the correct C++ class.
    for jni_prefix in [
        'Java_dev_cobalt_media_AudioOutputManager_nativeOnAudioDeviceChanged',
        'Java_dev_cobalt_media_MediaCodecBridge',
        'Java_dev_cobalt_media_MediaDrmBridge',
        'Java_dev_cobalt_media_VideoSurfaceTexture_nativeOnFrameAvailable',
        'Java_dev_cobalt_media_VideoSurfaceView_nativeOnVideoSurfaceChanged',
        'Java_dev_cobalt_media_VideoSurfaceView_nativeSetNeedResetSurface',
        'Java_dev_cobalt_util_DisplayUtil_nativeOnDisplayChanged',
    ]:
      content = content.replace(jni_prefix,
                                jni_prefix + self.media_snapshot_version)

    # The Starboard functions are in the global namespace, so we add suffix to
    # them to avoid linking errors due to multiple symbols with the same name.
    if source_utils.is_sb_implementation_file(source_pathname, content):
      content = source_utils.append_suffix_to_sb_function(
          source_pathname, content, self.media_snapshot_version, self.verbose)

    with open(destination_pathname, 'w+', encoding='utf-8') as f:
      f.write(content)

  def _create_snapshot_gn_file(self, file_pathnames):
    gn_utils.create_gn_file(
        self.destination_project_root_dir,
        self._get_destination_pathname(
            os.path.join(self.source_project_root_dir, 'starboard/BUILD.gn')),
        self.media_snapshot_version, file_pathnames)


snapshot = Snapshot(
    '~/chromium_clean/src', '~/chromium/src', '2600', verbose=False)
snapshot.snapshot()

snapshot = Snapshot(
    '~/chromium_clean/src', '~/chromium/src', '2601', verbose=False)
snapshot.snapshot()
