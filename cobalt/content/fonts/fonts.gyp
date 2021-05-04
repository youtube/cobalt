# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

{
  'targets': [
    {
      'target_name': 'copy_font_data',
      'type': 'none',
      'includes': [ '../../build/contents_dir.gypi' ],
      'variables': {
        'source_font_files_dir': 'font_files',
        'copy_font_files': 1,

        'package_categories': [
          # These all use late expansion so the conditionals below get evaluated first.
          'sans-serif=>(package_named_sans_serif)',
          'serif=>(package_named_serif)',
          'fcc-captions=>(package_named_fcc_fonts)',
          'fallback-lang-non-cjk=>(package_fallback_lang_non_cjk)',
          'fallback-lang-cjk=>(package_fallback_lang_cjk)',
          'fallback-lang-cjk-low-quality=>(package_fallback_lang_cjk_low_quality)',
          'fallback-historic=>(package_fallback_historic)',
          'fallback-color-emoji=>(package_fallback_color_emoji)',
          'fallback-emoji=>(package_fallback_emoji)',
          'fallback-symbols=>(package_fallback_symbols)',
        ],

        'conditions': [
          # 'unlimited' is deprecated but is mapped to 'standard'
          # 'expanded' also currently maps to 'standard' but this may change in the future
          [ 'cobalt_font_package == "standard" or cobalt_font_package == "unlimited" or cobalt_font_package == "expanded"', {
            'source_font_config_dir': 'config/common',

            'package_named_sans_serif': 4,
            'package_named_serif': 3,
            'package_named_fcc_fonts': 2,
            'package_fallback_lang_non_cjk': 2,
            'package_fallback_lang_cjk': 1,
            'package_fallback_lang_cjk_low_quality': 0,
            'package_fallback_historic': 1,
            'package_fallback_color_emoji': 1,
            'package_fallback_emoji': 0,
            'package_fallback_symbols': 1,
          }],

          # '10megabytes' and 'limited_with_jp' are deprecated but map to 'limited'
          [ 'cobalt_font_package == "limited" or cobalt_font_package == "limited_with_jp" or cobalt_font_package == "10megabytes"', {
            'source_font_config_dir': 'config/common',

            'package_named_sans_serif': 2,
            'package_named_serif': 0,
            'package_named_fcc_fonts': 0,
            'package_fallback_lang_non_cjk': 1,
            'package_fallback_lang_cjk': 0,
            'package_fallback_lang_cjk_low_quality': 1,
            'package_fallback_historic': 0,
            'package_fallback_color_emoji': 0,
            'package_fallback_emoji': 1,
            'package_fallback_symbols': 1,
          }],

          [ 'cobalt_font_package == "minimal"', {
            'source_font_config_dir': 'config/common',

            'package_named_sans_serif': 0,
            'package_named_serif': 0,
            'package_named_fcc_fonts': 0,
            'package_fallback_lang_non_cjk': 0,
            'package_fallback_lang_cjk': 0,
            'package_fallback_lang_cjk_low_quality': 0,
            'package_fallback_historic': 0,
            'package_fallback_color_emoji': 0,
            'package_fallback_emoji': 0,
            'package_fallback_symbols': 0,
          }],

          [ 'cobalt_font_package == "empty"', {
            'source_font_config_dir': 'config/empty',

            'package_named_sans_serif': 0,
            'package_named_serif': 0,
            'package_named_fcc_fonts': 0,
            'package_fallback_lang_non_cjk': 0,
            'package_fallback_lang_cjk': 0,
            'package_fallback_lang_cjk_low_quality': 0,
            'package_fallback_historic': 0,
            'package_fallback_color_emoji': 0,
            'package_fallback_emoji': 0,
            'package_fallback_symbols': 0,
          }],


          [ 'cobalt_font_package == "android_system"', {
            # fonts.xml contains a superset of what we expect to find on Android
            # devices. The Android SbFile implementation falls back to system font
            # files for those not in cobalt content.
            'source_font_config_dir': 'config/android',

            # Don't copy font files for Android since it falls back to system
            # font files for everything listed in fonts.xml.
            'copy_font_files': 0,

            'package_named_sans_serif': 0,
            'package_named_serif': 0,
            'package_named_fcc_fonts': 0,
            'package_fallback_lang_non_cjk': 0,
            'package_fallback_lang_cjk': 0,
            'package_fallback_lang_cjk_low_quality': 0,
            'package_fallback_historic': 0,
            'package_fallback_color_emoji': 0,
            'package_fallback_emoji': 0,
            'package_fallback_symbols': 0,
          }],

          [ 'cobalt_font_package_override_named_sans_serif >= 0', {
            'package_named_sans_serif': '<(cobalt_font_package_override_named_sans_serif)',
          }],
          [ 'cobalt_font_package_override_named_serif >= 0', {
            'package_named_serif': '<(cobalt_font_package_override_named_serif)',
          }],
          [ 'cobalt_font_package_override_named_fcc_fonts >= 0', {
            'package_named_fcc_fonts': '<(cobalt_font_package_override_named_fcc_fonts)',
          }],
          [ 'cobalt_font_package_override_fallback_lang_non_cjk >= 0', {
            'package_fallback_lang_non_cjk': '<(cobalt_font_package_override_fallback_lang_non_cjk)',
          }],
          [ 'cobalt_font_package_override_fallback_lang_cjk >= 0', {
            'package_fallback_lang_cjk': '<(cobalt_font_package_override_fallback_lang_cjk)',
          }],
          [ 'cobalt_font_package_override_fallback_lang_cjk_low_quality >= 0', {
            'package_fallback_lang_cjk_low_quality': '<(cobalt_font_package_override_fallback_lang_cjk_low_quality)',
          }],
          [ 'cobalt_font_package_override_fallback_historic >= 0', {
            'package_fallback_historic': '<(cobalt_font_package_override_fallback_historic)',
          }],
          [ 'cobalt_font_package_override_fallback_color_emoji >= 0', {
            'package_fallback_color_emoji': '<(cobalt_font_package_override_fallback_color_emoji)',
          }],
          [ 'cobalt_font_package_override_fallback_emoji >= 0', {
            'package_fallback_emoji': '<(cobalt_font_package_override_fallback_emoji)',
          }],
          [ 'cobalt_font_package_override_fallback_symbols >= 0', {
            'package_fallback_symbols': '<(cobalt_font_package_override_fallback_symbols)',
          }],
        ],
      },

      'conditions': [
        [ 'cobalt_font_package == "empty"', {
          'copies': [{
            'files': [ 'config/empty/fonts.xml' ],
            'destination': '<(sb_static_contents_output_data_dir)/fonts/',
          }],
        }, {
          'actions': [
            {
              'action_name': 'fonts_xml',
              'inputs': [
                  'scripts/filter_fonts.py',
                  '<(source_font_config_dir)/fonts.xml',
              ],
              'outputs': [
                '<(sb_static_contents_output_data_dir)/fonts/fonts.xml',
              ],
              'action': [
                'python2', 'scripts/filter_fonts.py',
                '-i', '<(source_font_config_dir)/fonts.xml',
                '-o', '<(sb_static_contents_output_data_dir)/fonts/fonts.xml',
                '<@(package_categories)',
              ],
            },
          ],

          'conditions': [
            [ 'copy_font_files == 0', {
              'copies': [{
                # Copy at least the fallback Roboto Subsetted font.
                'files': [ '<(source_font_files_dir)/Roboto-Regular-Subsetted.woff2' ],
                'destination': '<(sb_static_contents_output_data_dir)/fonts/',
              }],
            }, {
              'copies': [{
                # Late expansion so <@(package_categories) is resolved.
                'files': [ '>!@pymod_do_main(cobalt.content.fonts.scripts.filter_fonts -i <(source_font_config_dir)/fonts.xml -f <(source_font_files_dir) <@(package_categories))' ],
                'destination': '<(sb_static_contents_output_data_dir)/fonts/',
              }],
            }],
          ],
        }],
      ],

      'all_dependent_settings': {
        'variables': {
          'content_deploy_subdirs': [ 'fonts' ]
        }
      },
    },
  ],
}
