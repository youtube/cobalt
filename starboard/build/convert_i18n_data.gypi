# Copyright 2016 Google Inc. All Rights Reserved.
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

# This file is meant to be included into an action to convert a set of XLB files
# into files of a simpler format (e.g. CSV) in the product directory, e.g.
# e.g. out/ps4_debug/content/data/i18n.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'convert_i18n_data',
#   'type': 'none',
#   'actions': [
#     {
#       'action_name': 'convert_i18n_data',
#       'variables': {
#         'input_files':
#           '<!(find <(DEPTH)/cobalt/content/i18n/platform/linux/*.xlb)',
#       },
#       'includes': [ '../build/convert_i18n_data.gypi' ],
#     },
#   ],
# },
#
# Meaning of the variables:
#   input_files: list of paths to XLB files; directories are not expanded.

{
  'variables': {
    'output_dir': '<(PRODUCT_DIR)/content/data/i18n'
  },

  'inputs': [
    '<!@pymod_do_main(starboard.build.convert_i18n_data -o <@(output_dir) --inputs <@(input_files))',
  ],

  'outputs': [
    '<!@pymod_do_main(starboard.build.convert_i18n_data -o <@(output_dir) --outputs <@(input_files))',
  ],

  'action': [
    'python',
    '<(DEPTH)/starboard/build/convert_i18n_data.py',
    '-o', '<@(output_dir)',
    '<@(input_files)',
  ],
}
