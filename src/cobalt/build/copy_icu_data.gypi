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

# This file is meant to be included by a GYP target that needs to copy the
# platform specific ICU data into <(PRODUCT_DIR)/content.

{
  'includes': [ 'contents_dir.gypi' ],

  'variables': {
    'variables': {
      'variables': {
        'variables': {
          # Target machine uses little endian convention
          'little_endian%': 1,

          # A directory with loose data files.should be used instead of a
	  # single .dat file
          'use_icu_dat_file%': 0,
        },

        'conditions': [
          ['target_arch in ["ps3", "wiiu", "x360"]', {
            'little_endian%': 0,
          }],
        ],

        'little_endian%': '<(little_endian)',
        'use_icu_dat_file%': '<(use_icu_dat_file)',
      },
    },

    #'inputs_icu%': [ '<(inputs_icu)' ],
    'inputs_icu%': [ '<(static_contents_source_dir)/icu/' ],
  },

  'copies': [
    {
      'destination': '<(static_contents_output_data_dir)/',
      'files': [ '<(inputs_icu)' ],
    },
  ],
}
