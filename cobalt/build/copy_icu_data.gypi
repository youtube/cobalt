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

# This file is meant to be included by a GYP target that needs to copy the
# platform specific ICU data into <(PRODUCT_DIR)/content.

{
  'includes': [ 'contents_dir.gypi' ],

  'variables': {
    'variables': {
      # Target machine uses little endian convention
      'little_endian%': 1,
    },

    'conditions': [
      ['little_endian==1', {
        'inputs_icu%': [ '<(static_contents_source_dir)/icu/icudt68l.dat' ],
      }, {
        'inputs_icu%': [ '<(static_contents_source_dir)/icu/icudt68b.dat' ],
      }],
    ],
  },

  'copies': [
    {
      'destination': '<(sb_static_contents_output_data_dir)/icu/',
      'files': [ '<(inputs_icu)' ],
    },
  ],

  'all_dependent_settings': {
    'variables': {
      'content_deploy_subdirs': [ 'icu' ]
    }
  },
}
