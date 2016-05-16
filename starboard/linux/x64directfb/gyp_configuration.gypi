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

# Platform specific configuration for Linux on Starboard.  Automatically
# included by gyp_cobalt in all .gyp files by Cobalt together with base.gypi.
#

{
  'variables': {
    'platform_libraries': [
      '-ldirectfb',
      '-ldirect',
    ],

    'gl_type': 'none',
  },

  'target_defaults': {
    'default_configuration': 'SbLinuxDirectFB_Debug',
    'configurations': {
      'SbLinuxDirectFB_Debug': {
        'inherit_from': ['debug_base'],
      },
      'SbLinuxDirectFB_Devel': {
        'inherit_from': ['devel_base'],
      },
      'SbLinuxDirectFB_QA': {
        'inherit_from': ['qa_base'],
      },
      'SbLinuxDirectFB_Gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  },

  'includes': [
    '../../../cobalt/build/config/starboard_linux.gypi',
  ],
}
