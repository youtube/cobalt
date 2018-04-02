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

# This file is meant to be included into a target to add an action
# that copies test data files into the DIR_TEST_DATA directory,
# e.g. out/linux-x64x11_devel/content/data/test/.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'target_name_copy_test_data',
#   'type': 'none',
#   'variables': {
#     'content_test_input_files': [
#       'path/to/datafile.txt',     # The file will be copied.
#       'path/to/data/directory',   # The directory and its content will be copied.
#       'path/to/data/directory/',  # The directory's content will be copied.
#     ],
#     'content_test_output_subdir' : 'path/to/output/directory',
#   },
#   'includes': ['<(DEPTH)/starboard/build/copy_test_data.gypi'],
# },
#
# Meaning of the variables:
#   content_test_input_files: list:
#       Paths to data files or directories. When an item is a directory,
#       without the final "/", the directory (along with the content) will be
#       copied, otherwise only the content will be copied.
#
#   content_test_output_subdir: string:
#       Directory within the test directory that all input files will be
#       copied to. Generally, this should be the directory of the gypi file
#       containing the target (e.g. it should be "base" for the target in
#       base/base.gyp).
#
# It's recommended that content_test_input_files and content_test_output_subdir
# have similar paths, so the directory structure in data/test/ will reflect
# that in the source folder.

{
  'actions': [
    {
      'action_name': 'copy_test',
      'inputs': [
        '<!@pymod_do_main(starboard.build.copy_data --inputs <(content_test_input_files))',
      ],
      'outputs': [
        '<!@pymod_do_main(starboard.build.copy_data -o <(sb_static_contents_output_data_dir)/test/<(content_test_output_subdir) --outputs <(content_test_input_files))',
      ],
      'action': [
        'python',
        '<(DEPTH)/starboard/build/copy_data.py',
        '-o', '<(sb_static_contents_output_data_dir)/test/<(content_test_output_subdir)',
        '<@(content_test_input_files)',
      ],
    },
  ],
  'all_dependent_settings': {
    'variables': {
      'content_deploy_subdirs': [ 'test/<(content_test_output_subdir)' ]
    }
  },
}
