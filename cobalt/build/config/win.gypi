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

# Platform specific configuration.
# Should be included in all .gyp files used by Cobalt together with base.gypi.
# Normally, this should be done automatically by gyp_cobalt.
{
  'variables': {
    # Cobalt on Windows is pretending to be XB1 when compiling Steel code by
    # setting target_arch="xb1". In places, where knowing the actual platform is
    # required (e.g. platform_detegate_win.cc), actual_target_arch is used
    # instead.
    'target_arch': 'xb1',
    'actual_target_arch': 'win',
    'posix_emulation_target_type': 'static_library',

    'compiler_flags_host': [
    ],
    'linker_flags_host': [
    ],
    'compiler_flags': [
    ],
    'linker_flags': [
    ],

    'compiler_flags_debug': [
    ],
    'linker_flags_debug': [
      'DEBUG',
    ],

    'compiler_flags_devel': [
    ],
    'linker_flags_devel': [
    ],

    'compiler_flags_qa': [
    ],
    'linker_flags_qa': [
    ],

    'compiler_flags_gold': [
    ],
    'linker_flags_gold': [
    ],

    'platform_libraries': [
    ],

    'conditions': [
      ['cobalt_fastbuild==0', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'DebugInformationFormat': '3',
          },
          'VCLinkerTool': {
            'GenerateDebugInformation': 'true',
          },
        },
      }],
    ],
  },  # end of variables.

  'target_defaults': {
    'defines': [
      # Cobalt on Windows flag
      'COBALT_WIN',
      # This define makes Steel code compile for XB1.
      '__LB_XB1__',
      # Exclude specific headers in Windows.h
      'WIN32_LEAN_AND_MEAN',
    ],
    'msvs_disabled_warnings': [
      # Using unsecure or deprecated version of a function. Turned off /sdl
      # below to not treat this as error.
      4996,
      # TODO(***REMOVED***): Investigate following warnings:
      # 4530: exception handler used, but unwind semantics are not enabled. Specify /EHsc
      # 4244: conversion from 'type1' to 'type2', possible loss of data
      4530, 4244,
      # Warnings below are introdcued by changing WarningLevel from 2 to 3.
      4267, 4018, 4800
    ],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'ForcedIncludeFiles': ['posix_emulation.h'],
      },
    },
    'target_conditions': [
      ['_type=="executable"', {
        'msvs_settings': {
          # TODO(***REMOVED***): Remove this by switching POSIX emulation to use
          # pthreads-win32 just like XB360.
          'VCCLCompilerTool': {
            'ComponentExtensions': 'true'
          },
        },
      }],
      ['_type in ["executable", "shared_library"]', {
        # Everyone links in their own copy of operator new and friends.
        'sources': [
          '<(PRODUCT_DIR)/obj/lbshell/src/posix_emulation.lb_new.o',
        ],
      }],
    ],

    'default_configuration': 'Win_Debug',
    'configurations': {
      'msvs_base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\build\\<(build_dir_prefix)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
        'msvs_configuration_platform': 'x64',
        'msvs_target_platform': 'x64',
        # Add the default import libs.
        'msvs_settings':{
          'VCCLCompilerTool': {
            'BufferSecurityCheck': 'true',
            'Conformance': ['wchar_t', 'forScope'],
            'Detect64BitPortabilityProblems': 'false',
            'MinimalRebuild': 'false',
            'WarnAsError': 'false',
            'WarningLevel': '3',

            'AdditionalOptions': [
              '/arch:AVX', # Advanced Vector Extensions
              '/errorReport:none', # don't send error reports to MS.
            ],
          },
          'VCLinkerTool': {
            'RandomizedBaseAddress': '2', # /DYNAMICBASE
            'SubSystem': '1', # CONSOLE
            'TargetMachine': '17', # x86 - 64
            'AdditionalOptions': [
              '/WINMD:NO', # Do not generate a WinMD file.
              '/errorReport:none', # don't send error reports to MS.
            ],
          },
          'VCLibrarianTool': {
            # TODO(***REMOVED***): Remove after removing ComponentExtensions
            'AdditionalOptions': ['/ignore:4264'],
          },
        },
        'conditions': [
          ['cobalt_fastbuild==0', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'DebugInformationFormat': '3',
              },
              'VCLinkerTool': {
                'GenerateDebugInformation': 'true',
              },
            },
          }],
        ],
      },
      'Win_Debug': {
        'inherit_from': ['debug_base', 'msvs_base'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'BasicRuntimeChecks': '1',
            'AdditionalOptions': [
              '/MDd', # Use debug multithreaded library.
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': ['dbghelp.lib'],
            'LinkIncremental': '2',  # INCREMENTAL:YES
          },
        },
      },
      'Win_Devel': {
        'inherit_from': ['devel_base', 'msvs_base'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',
            'AdditionalOptions': [
              '/MDd', # Use debug multithreaded library.
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': ['dbghelp.lib'],
            'LinkIncremental': '2',  # INCREMENTAL:YES
          },
        },
      },
      'Win_QA': {
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
      'Win_Gold': {
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
    }, # end of configurations
  }, # end of target_defaults
}
