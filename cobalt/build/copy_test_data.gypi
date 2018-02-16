# Copyright 2014 Google Inc. All Rights Reserved.
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

# This file is meant to be included into an action to copy test data files into
# the DIR_TEST_DATA directory, e.g. out/PS3_Debug/content/data/test/.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'copy_data_target_name',
#   'type': 'none',
#   'actions': [
#     {
#       'action_name': 'copy_data_target_name',
#       'variables': {
#         'input_files': [
#           'path/to/datafile.txt',     # The file will be copied.
#           'path/to/data/directory',   # The directory and its content will be copied.
#           'path/to/data/directory/',  # The directory's content will be copied.
#         ],
#         'output_dir' : 'path/to/output/directory',
#       },
#       'includes': [ 'path/to/this/gypi/file' ],
#     },
#   ],
# },
#
# Meaning of the variables:
#   input_files: list: paths to data files or directories. When an item is a
#                      directory, without the final "/", the directory (along
#                      with the content) will be copied, otherwise only the
#                      content will be copied.
#   output_dir: string: The directory that all input files will be copied to.
#                       Generally, this should be the directory of the gypi file
#                       containing the target (e.g. it should be "base" for the
#                       target in base/base.gyp).
# It is recommended that input_files and output_dir have similar path, so the
# directory structure in test/ will reflect that in the source folder.

{
  'includes': [ 'contents_dir.gypi' ],
  'inputs': [
    '<!@pymod_do_main(starboard.build.copy_data --inputs <(input_files))',
  ],
  'outputs': [
    '<!@pymod_do_main(starboard.build.copy_data -o <(sb_static_contents_output_data_dir)/test/<(output_dir) --outputs <(input_files))',
  ],
  'action': [
    'python',
    '<(DEPTH)/starboard/build/copy_data.py',
    '-o', '<(sb_static_contents_output_data_dir)/test/<(output_dir)',
    '<@(input_files)',
  ],
}
