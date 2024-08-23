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

_GN_CONTENT = '''##########################################################
# Configuration to extract GameActivity native files.
##########################################################

game_activity_aar_file = "//starboard/android/apk/app/src/main/java/dev/cobalt/libraries/game_activity/games-activity-2.0.2.aar"

game_activity_source_files = [
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-activity/GameActivity.cpp",
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-activity/GameActivity.h",
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-activity/GameActivityEvents.cpp",
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-activity/GameActivityEvents.h",
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-activity/GameActivityLog.h",
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-text-input/gamecommon.h",
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-text-input/gametextinput.cpp",
  "$target_gen_dir/game_activity/prefab/modules/game-activity/include/game-text-input/gametextinput.h",
]

game_activity_include_dirs =
  [ "$target_gen_dir/game_activity/prefab/modules/game-activity/include" ]

action("game_activity_sources") {{
  script = "//starboard/tools/unzip_file.py"
  sources = [ game_activity_aar_file ]
  outputs = game_activity_source_files
  args = [
    "--zip_file",
    rebase_path(game_activity_aar_file, root_build_dir),
    "--output_dir",
    rebase_path(target_gen_dir, root_build_dir) + "/game_activity",
  ]
}}

static_library("{0}") {{
  check_includes = false

  sources = [
    {1}
  ]

  configs += [ "//starboard/build/config:starboard_implementation" ]

  include_dirs = game_activity_include_dirs

  public_deps = [
    ":game_activity_sources",
    "//starboard/common",
  ]
  deps = [
    "//third_party/opus",
  ]
}}
'''


def _get_copyright_header():
  return dedent(_COPYRIGHT_HEADER).format(datetime.now().year)


def create_gn_file(project_root_dir, gn_pathname, library_name, file_pathnames):
  abs_project_root_dir = os.path.abspath(project_root_dir)
  abs_gn_pathname = os.path.abspath(gn_pathname)

  # The gn file should reside in the project dir.
  assert abs_gn_pathname.find(abs_project_root_dir) == 0

  source_list = []

  for file_pathname in file_pathnames:
    abs_file_pathname = os.path.abspath(file_pathname)
    # The file should reside in the directory containing the gn file.
    assert abs_file_pathname.find(os.path.dirname(abs_gn_pathname)) == 0
    rel_file_pathname = os.path.relpath(abs_file_pathname, abs_project_root_dir)
    source_list.append('\"//' + rel_file_pathname + '",')

  source_list.sort()

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
  assert os.path.isfile(pathname)
  return pathname


def get_source_pathnames(project_root_dir, ninja_root_dir, target_name):
  ''' Return a list of source files built for a particular ninja target

      project_root_dir: The project root directory, e.g. '/home/.../cobalt'
      ninja_root_dir: The output directory, e.g. 'out/android-arm'
      target_name: The name of the ninja target, e.g. '//cobalt/base:base'.
  '''
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
