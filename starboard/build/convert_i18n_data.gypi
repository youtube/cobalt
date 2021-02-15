# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
# e.g. out/stub_debug/content/data/i18n.
#
# To use this, define the variable xlb_files and include this file.
#  'variables': {
#    'xlb_files':
#      '<!(find <(DEPTH)/cobalt/content/i18n/platform/linux/*.xlb)',
#  },
#
# Meaning of the variables:
#   xlb_files: list of paths to XLB files; directories are not expanded.

{
  'variables': {
    'output_dir': '<(sb_static_contents_output_data_dir)/i18n'
  },
  'targets': [
    {
      'target_name': 'convert_i18n_data',
      'type': 'none',
      'actions': [
        {
          'action_name': 'convert_i18n_data',
          'inputs': [
            '<!@pymod_do_main(starboard.build.convert_i18n_data -o <@(output_dir) --inputs <@(xlb_files))',
          ],
          'outputs': [
            '<!@pymod_do_main(starboard.build.convert_i18n_data -o <@(output_dir) --outputs <@(xlb_files))',
          ],
          'action': [
            'python2',
            '<(DEPTH)/starboard/build/convert_i18n_data.py',
            '-o', '<@(output_dir)',
            '<@(xlb_files)',
          ],
        },
      ],
      # Makes collect_deploy_content aware of the directory so that all binaries
      # depending on this target will then transitively include i18n.
      'all_dependent_settings': {
        'variables': {
          'content_deploy_subdirs': [ 'i18n' ]
        }
      }
    },
  ],
}
