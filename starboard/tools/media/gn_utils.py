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
'''Utility to work with gn.'''

from datetime import datetime
from textwrap import dedent

import os
import subprocess

_COPYRIGHT_HEADER = '''\
  # Copyright {0} The Cobalt Authors. All Rights Reserved.
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
  '''

_GN_CONTENT = '''\
static_library("{0}") {{
  check_includes = false

  sources = [
    {1}
  ]

  configs += [ "//starboard/build/config:starboard_implementation" ]

  public_deps = [
    "//starboard/common",
  ]
  deps = [
    "//base",  # TODO: Remove once the upstream is refined.
    "//third_party/opus",
  ]
}}
'''

_CANONICAL_GN_CONTENT = '''
static_library("media_snapshot") {{
  check_includes = false

  sources = [
    {0}
  ]

  configs += [ "//starboard/build/config:starboard_implementation" ]

  public_deps = [
    "//starboard/common",
  ]
}}
'''


def _get_copyright_header():
  return dedent(_COPYRIGHT_HEADER).format(datetime.now().year)


def convert_source_list_to_gn_format(project_root_dir, gn_pathname,
                                     file_pathnames):
  abs_project_root_dir = os.path.abspath(project_root_dir)
  abs_gn_pathname = os.path.abspath(gn_pathname)

  # The gn file should reside in the project dir.
  assert abs_gn_pathname.find(abs_project_root_dir) == 0

  source_list = []

  for file_pathname in file_pathnames:
    abs_file_pathname = os.path.abspath(file_pathname)
    if os.path.dirname(file_pathname) == os.path.dirname(abs_gn_pathname):
      rel_file_pathname = os.path.basename(file_pathname)
    else:
      rel_file_pathname = '//' + os.path.relpath(abs_file_pathname,
                                                 abs_project_root_dir)
    source_list.append('\"' + rel_file_pathname + '",')

  source_list.sort()

  return source_list


def create_gn_file(project_root_dir, gn_pathname, library_name, file_pathnames):
  abs_project_root_dir = os.path.abspath(project_root_dir)
  abs_gn_pathname = os.path.abspath(gn_pathname)

  # The gn file should reside in the project dir.
  assert abs_gn_pathname.find(abs_project_root_dir) == 0

  source_list = convert_source_list_to_gn_format(project_root_dir, gn_pathname,
                                                 file_pathnames)

  with open(abs_gn_pathname, 'w+', encoding='utf-8') as f:
    f.write(_get_copyright_header() + '\n' +
            _GN_CONTENT.format(library_name, '\n    '.join(source_list)))


def _get_full_pathname(project_root_dir, pathname_in_gn_format):
  ''' Transform a pathname in gn format to unix format

      project_root_dir: The project root directory in unix format, e.g.
                        '/home/.../cobalt'
      pathname_in_gn_format: A pathname in gn format, e.g. '//starboard/media.h'
      return: the full path name as '/home/.../cobalt/starboard/media.h'
  '''
  assert pathname_in_gn_format.find('//') == 0
  pathname_in_gn_format = pathname_in_gn_format[2:]
  pathname = os.path.join(project_root_dir, pathname_in_gn_format)
  if pathname.find('game-activity') < 0:
    assert os.path.isfile(pathname), pathname
  return pathname


def get_source_pathnames(project_root_dir, ninja_root_dir,
                         target_name_or_names):
  ''' Return a list of source files built for a particular ninja target or a
      series of target names.

      project_root_dir: The project root directory, e.g. '/home/.../cobalt'
      ninja_root_dir: The output directory, e.g. 'out/android-arm'
      target_name_or_names: The name of the ninja target, e.g.
          '//cobalt/base:base', or a series pf target names, e.g.
          ('//cobalt/base:base', '//cobalt/media:media').
  '''
  if isinstance(target_name_or_names, str):
    target_name = target_name_or_names
  else:
    # Assume `target_name_or_names` is a container
    source_files = []
    for target_name in target_name_or_names:
      source_files += get_source_pathnames(project_root_dir, ninja_root_dir,
                                           target_name)
    return source_files

  saved_python_path = os.environ['PYTHONPATH']
  os.environ['PYTHONPATH'] = os.path.abspath(
      project_root_dir) + ':' + os.environ['PYTHONPATH']
  gn_desc = subprocess.check_output(['gn', 'desc', ninja_root_dir, target_name],
                                    cwd=project_root_dir).decode('utf-8')
  os.environ['PYTHONPATH'] = saved_python_path

  # gn_desc is in format:
  # ...
  # sources
  #   //path/name1
  #   //path/name2
  # <empty line>
  # ...
  lines = gn_desc.split('\n')
  sources_index = lines.index('sources')
  assert sources_index >= 0
  sources_index += 1
  sources = []
  while sources_index < len(lines) and lines[sources_index]:
    sources.append(
        _get_full_pathname(project_root_dir, lines[sources_index].strip()))
    sources_index += 1
  return sources
