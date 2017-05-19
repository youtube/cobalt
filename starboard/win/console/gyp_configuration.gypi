# Copyright 2017 Google Inc. All Rights Reserved.
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
  'variables': {
    'javascript_engine': 'mozjs',
    'cobalt_enable_jit': 0,
  },
  'includes': [
    '../shared/gyp_configuration.gypi',
  ],
  'target_defaults': {
    'default_configuration': 'win-console_debug',
    'configurations': {
      'win-console_debug': {
        'inherit_from': ['debug_base', 'msvs_base'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'BasicRuntimeChecks': '3', # Check stack frame validity and check for uninitialized variables at run time.
            'AdditionalOptions': [
              '/MDd', # Use debug multithreaded library.
              '/GS',
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': ['dbghelp.lib'],
            'LinkIncremental': '2',  # INCREMENTAL:YES
          },
        },
      },
      'win-console_devel': {
        'inherit_from': ['devel_base', 'msvs_base'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',
            'AdditionalOptions': [
              '/MDd', # Use debug multithreaded library.
              '/GS',
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': ['dbghelp.lib'],
            'LinkIncremental': '2',  # INCREMENTAL:YES
          },
        },
      },
      'win-console_qa': {
        'inherit_from': ['qa_base', 'msvs_base'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',
            'AdditionalOptions': [
              '/MD', # Use release multithreaded library.
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': ['dbghelp.lib'],
            'LinkIncremental': '1',  # INCREMENTAL:NO
            'OptimizeReferences' : '2',  # OPT:REF
            'EnableCOMDATFolding' : '2',  # OPT:ICF
          },
        },
      },
      'win-console_gold': {
        'inherit_from': ['gold_base', 'msvs_base'],
         'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',
            'AdditionalOptions': [
              '/MD', # Use release multithreaded library.
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',  # INCREMENTAL:NO
            'OptimizeReferences' : '2',  # OPT:REF
            'EnableCOMDATFolding' : '2',  # OPT:ICF
          },
        },
      },
    },  # end of configurations
  },
}
