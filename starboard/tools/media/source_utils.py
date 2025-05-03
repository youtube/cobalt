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

from datetime import datetime
from textwrap import dedent

import os
import re
import utils

_COPYRIGHT_HEADER = '''\
  // Copyright {0} The Cobalt Authors. All Rights Reserved.
  //
  // Licensed under the Apache License, Version 2.0 (the "License");
  // you may not use this file except in compliance with the License.
  // You may obtain a copy of the License at
  //
  //     http://www.apache.org/licenses/LICENSE-2.0
  //
  // Unless required by applicable law or agreed to in writing, software
  // distributed under the License is distributed on an "AS IS" BASIS,
  // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  // See the License for the specific language governing permissions and
  // limitations under the License.
  '''

_CANONICAL_FILE_CONTENT = '''\

  {0}

  extern "C" {{

  SB_EXPORT {1};

  }}  // extern "C"

  {2} {{
    return {3};
  }}

  #define {4} {5}
  #define REFERENCE_SOURCE_FILE "{6}"

  #include REFERENCE_SOURCE_FILE
  '''


def _get_copyright_header():
  return dedent(_COPYRIGHT_HEADER).format(datetime.now().year)


# for `player_create.cc', return 'starboard/player.h'.
def _get_starboard_includes(pathname):
  basename = os.path.basename(pathname)

  if basename.find('audio_sink_') == 0:
    return '#include "starboard/audio_sink.h"'
  if basename.find('decode_target_') == 0:
    return '#include "starboard/decode_target.h"'
  if basename.find('drm_') == 0:
    return '#include "starboard/drm.h"'
  if basename.find('media_') == 0:
    return '#include "starboard/media.h"'
  if basename.find('player_') == 0:
    return '#include "starboard/player.h"'
  assert False


# starboard/player.h => STARBOARD_PLAYER_H_
def generate_include_guard_macro(project_root_dir, file_pathname):
  rel_pathname = os.path.relpath(file_pathname, project_root_dir)
  return rel_pathname.replace('/', '_').replace('.', '_').upper() + '_'


def extract_class_or_struct_names(content):
  pattern = r'^\s*class\s+(\w+)\s*[:{]'
  matches = re.findall(pattern, content, re.MULTILINE)

  pattern = r'^\s*struct\s+(\w+)\s*[:{]'
  return matches + re.findall(pattern, content, re.MULTILINE)


def extract_project_includes(content):
  pattern = r'#\s*include\s*"([^"]+)"'
  return re.findall(pattern, content, re.MULTILINE)


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
    # Already included
    return content

  index = search_func(content, include_directive_prefix)
  if index >= 0:
    index = content.find('\n', index)
    return content[:index] + '\n' + include_directive + '\n' + content[index:]

  # Cannot find the group, add it to the very beginning and refine it manually
  return include_directive + '\n' + content


def is_sb_implementation_file(pathname, content):
  basename, ext = utils.get_base_file_name_and_ext(pathname)
  if ext != 'cc':
    return False

  function_name = utils.base_name_to_sb_function_name(basename)
  if content.find(' ' + function_name + '(') == -1:
    return False
  return True


def append_suffix_to_sb_function(pathname, content, suffix, verbose):
  if verbose:
    print('appending suffix for Sb function to', pathname)

  basename, ext = utils.get_base_file_name_and_ext(pathname)
  assert ext == 'cc'

  function_name = utils.base_name_to_sb_function_name(basename)

  assert content.find(' ' + function_name + '(') != -1

  return content.replace(function_name, function_name + suffix)


# For the content of player_destroy.cc and 'SbPlayerDestroy', returns
# 'void SbPlayerDestroy(SbPlayer player)'.
def get_function_prototype(content, function_name):
  # We assume the first occurrence is the prototype.  This won't work when
  # there is a comment before the prototype, which isn't currently used.
  left = content.find(' ' + function_name + '(')

  # Move to right after the previous line end
  left = content.rfind('\n', 0, left) + 1
  assert left > 0

  # Now keep find until the next ')'
  right = content.find(')', left)
  assert right != -1

  return content[left:right + 1]


def patch_sb_implementation_with_branching_call(pathname, content, suffix,
                                                verbose):
  if verbose:
    print('patching', pathname)

  basename, ext = utils.get_base_file_name_and_ext(pathname)
  assert ext == 'cc'

  function_name = utils.base_name_to_sb_function_name(basename)

  if content.find(function_name + suffix) != -1:
    # Already patched, don't patch further.
    return content

  assert content.find(' ' + function_name + '(') != -1

  prototype = get_function_prototype(content, function_name)

  branched_prototype = prototype.replace(function_name, function_name + suffix)

  content = add_header_file(content,
                            'starboard/shared/media_snapshot/media_snapshot.h')

  offset = content.find(prototype)
  content = content[:offset] + branched_prototype + ';\n\n' + content[offset:]

  offset = content.find(prototype)
  offset = content.find('{', offset)
  assert offset != -1
  offset += 1

  calling_statement = utils.get_calling_statement_from_prototype(
      branched_prototype)
  if_statement = ('\n' + '  if (GetMediaSnapshotVersion() == ' + suffix +
                  ') {\n' + '    return ' + calling_statement + ';\n'
                  '  }\n')

  return content[:offset] + if_statement + content[offset:]


def create_canonical_file(source_project_root_dir, source_pathname,
                          destination_pathname):
  assert os.path.basename(source_pathname) == os.path.basename(
      destination_pathname)

  with open(source_pathname, encoding='utf-8') as f:
    content = f.read()

  basename, ext = utils.get_base_file_name_and_ext(source_pathname)
  assert ext == 'cc'

  # '.../decode_target_release.cc' => SbDecodeTargetRelease
  function_name = utils.base_name_to_sb_function_name(basename)

  assert content.find(' ' + function_name + '(') != -1

  # SbDecodeTargetRelease => starboard/decode_target.h
  starboard_includes = _get_starboard_includes(source_pathname)

  # SbDecodeTargetRelease =>
  # void SbDecodeTargetRelease(SbDecodeTarget decode_target)
  prototype = get_function_prototype(content, function_name)

  canonical_function_name = function_name + 'Canonical'

  # void SbDecodeTargetRelease(SbDecodeTarget decode_target) =>
  # void SbDecodeTargetReleaseCanonical(SbDecodeTarget decode_target)
  canonical_prototype = prototype.replace(function_name,
                                          canonical_function_name)

  # void SbDecodeTargetReleaseCanonical(SbDecodeTarget decode_target) =>
  # SbDecodeTargetReleaseCanonical(decode_target)
  calling_statement = utils.get_calling_statement_from_prototype(
      canonical_prototype)

  source_rel_pathname = os.path.relpath(source_pathname,
                                        source_project_root_dir)
  with open(destination_pathname, 'w+', encoding='utf-8') as f:
    f.write(_get_copyright_header() + dedent(_CANONICAL_FILE_CONTENT).format(
        starboard_includes, canonical_prototype, prototype, calling_statement,
        function_name, canonical_function_name, source_rel_pathname))
