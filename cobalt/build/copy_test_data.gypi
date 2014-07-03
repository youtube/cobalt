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
# the content directory in out/.
# To use this the following variables need to be defined:
#   test_data_files: list: paths to test data files or directories
#   test_data_prefix: string: a directory prefix that will be prepended to each
#                             output path.  Generally, this should be the base
#                             directory of the gypi file containing the test
#                             target (e.g. "base" or "cobalt").
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_unittests',
#   'conditions': [
#     ['cobalt == 1', {
#       'actions': [
#         {
#           'action_name': 'copy_test_data',
#           'variables': {
#             'test_data_files': [
#               'path/to/datafile.txt',
#               'path/to/data/directory/',
#             ]
#             'test_data_prefix' : 'prefix',
#           },
#           'includes': [ 'path/to/this/gypi/file' ],
#         },
#       ],
#     }],
# }
#

{
  'inputs': [
    '<!@pymod_do_main(copy_test_data --inputs <(test_data_files))',
  ],
  'outputs': [
    '<!@pymod_do_main(copy_test_data -o <(PRODUCT_DIR)/content/dir_source_root/<(test_data_prefix) --outputs <(test_data_files))',
  ],
  'action': [
    'python',
    '<(DEPTH)/cobalt/build/copy_test_data.py',
    '-o', '<(PRODUCT_DIR)/content/dir_source_root/<(test_data_prefix)',
    '<@(_inputs)',
  ],
}
