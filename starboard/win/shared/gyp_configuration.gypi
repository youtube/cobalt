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
    'target_arch': 'x64',
    'target_os': 'win',

    # TODO: change to hardware and fix revealed build errors.
    # Use a stub rasterizer and graphical setup.
    'rasterizer_type': 'stub',

    # Link with angle
    'gl_type': 'angle',

    'cobalt_media_source_2016': 1,

    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',
    ],
    'linker_flags': [
    ],
    'compiler_flags_debug': [
      '-frtti',
      '-O0',
    ],
    'compiler_flags_devel': [
      '-frtti',
      '-O2',
    ],
    'compiler_flags_qa': [
      '-fno-rtti',
      '-O2',
      '-gline-tables-only',
    ],
    'compiler_flags_gold': [
      '-fno-rtti',
      '-O2',
      '-gline-tables-only',
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
        }
        }],
    ],
  },

  'target_defaults': {
    'configurations': {
      'msvs_base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\build\\<(build_dir_prefix)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
        'msvs_system_include_dirs': [
          '<(windows_sdk_path)/include/<(windows_sdk_version)/shared',
          '<(windows_sdk_path)/include/<(windows_sdk_version)/ucrt',
          '<(windows_sdk_path)/include/<(windows_sdk_version)/um',
          '<(windows_sdk_path)/include/<(windows_sdk_version)/winrt',
          '<(windows_sdk_path)/include/<(windows_sdk_version)',
          '<(visual_studio_install_path)/include',
          '<(visual_studio_install_path)/atlmfc/include',
        ],
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'windowsapp.lib',
            ],
            'AdditionalLibraryDirectories!': [
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/ucrt/x86',
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/um/x86',
              ],
            'AdditionalLibraryDirectories': [
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/ucrt/x64',
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/um/x64',
              '<(visual_studio_install_path)/lib/x64',
              ],
          },
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories!': [
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/ucrt/x86',
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/um/x86',
              ],
            'AdditionalLibraryDirectories': [
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/ucrt/x64',
              '<(windows_sdk_path)/lib/<(windows_sdk_version)/um/x64',
              '<(visual_studio_install_path)/lib/x64',
              ],
          },
        },
        'msvs_target_platform': 'x64',
        # Add the default import libs.
        'conditions': [
          ['cobalt_fastbuild==0', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'DebugInformationFormat': '3',
                'AdditionalOptions': [
                '/FS',
                ],
              },
              'VCLinkerTool': {
                'GenerateDebugInformation': 'true',
              },
            },
          }],
        ],
      },
       'msvs_debug': {
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
       'msvs_devel': {
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
       'msvs_qa': {
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
       'msvs_gold': {
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
    },
    'defines': [
      # Disable suggestions to switch to Microsoft-specific secure CRT.
      '_CRT_SECURE_NO_WARNINGS',
      # Disable support for exceptions in STL in order to detect their use
      # (the use of exceptions is forbidden by Google C++ style guide).
      '_HAS_EXCEPTIONS=0',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
      #'_GNU_SOURCE=1',
      'WIN32_LEAN_AND_MEAN',
      # By defining this, M_PI will get #defined.
      '_USE_MATH_DEFINES',
      # min and max collide with std::min and std::max
      'NOMINMAX',
      # Conform with C99 spec.
      '_CRT_STDIO_ISO_WIDE_SPECIFIERS',
    ],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'ForcedIncludeFiles': [],
        # Check for buffer overruns.
        'BufferSecurityCheck': 'true',
        'Conformance': [
          # "for" loop's initializer go out of scope after the for loop.
          'forScope',
          # wchar_t is treated as a built-in type.
          'wchar_t',
        ],
        # Check for 64-bit portability issues.
        'Detect64BitPortabilityProblems': 'true',
        # Disable Microsoft-specific header dependency tracking.
        # Incremental linker does not support the Windows metadata included
        # in .obj files compiled with C++/CX support (/ZW).
        'MinimalRebuild': 'false',
        # Treat warnings as errors.
        'WarnAsError': 'false',
        # Enable some warnings, even those that are disabled by default.
        # See https://msdn.microsoft.com/en-us/library/23k5d385.aspx
        'WarningLevel': '3',

        'AdditionalOptions': [
          '/errorReport:none', # don't send error reports to MS.
          '/permissive-', # Visual C++ conformance mode.
          '/FS', # Force sync PDB updates for parallel compile.
        ],
      },
      'VCLinkerTool': {
        'RandomizedBaseAddress': '2', # /DYNAMICBASE
        # TODO: SubSystem is hardcoded in
        # win\sources_template.vcxproj. This will have the exe behave in the
        # expected way in MSVS: when it's run without debug (Ctrl+F5), it
        # will pause after finish; when debugging (F5) it will not pause
        # before the cmd window disappears.
        # Currently the value is ignored by msvs_makefile.py which generates
        # the MSVS project files (it's in "data" in GenerateOutput()). Fix
        # it if we ever need to change SubSystem.
        'SubSystem': '1', # CONSOLE
        'TargetMachine': '17', # x86 - 64
        'AdditionalOptions': [
          '/WINMD:NO', # Do not generate a WinMD file.
          '/errorReport:none', # don't send error reports to MS.
        ],
      },
      'VCLibrarianTool': {
        'AdditionalOptions': [
          # Linking statically with C++/CX library is not recommended.
          # TODO: Remove after removing ComponentExtensions
          '/ignore:4264',
        ],
      },
    },
    'msvs_disabled_warnings': [
      # Conditional expression is constant.
      # Triggers in many legitimate cases, like branching on a constant declared
      # in type traits.
      4127,
      # Class has virtual functions, but destructor is not virtual.
      # Far less useful than in GCC because doesn't take into account the fact
      # that destructor is not public.
      4265,
      # no function prototype given: converting '()' to '(void)'.
      # We do not care.
      4255,
      # cast truncates constant value.
      # We do not care.
      4310,
      # An rvalue cannot be bound to a non-const reference.
      # In previous versions of Visual C++, it was possible to bind an rvalue
      # to a non-const reference in a direct initialization. This warning
      # is useless as it simply describes proper C++ behavior.
      4350,
      # layout of class may have changed from a previous version of
      # the compiler due to better packing of member. We don't care about
      # binary compatibility with other compiler versions.
      4371,
      # relative include path contains '..'.
      # This occurs in a lot third party libraries and we don't care.
      4464,
      # decorated name length exceeded, name was truncated.
      4503,
      # assignment operator could not be generated.
      # This is expected for structs with const members.
      4512,
      # Unreferenced inline function has been removed.
      # While detection of dead code is good, this warning triggers in
      # third-party libraries which renders it useless.
      4514,
      # Expression before comma has no effect.
      # Cannot be used because Microsoft uses _ASSERTE(("message", 0)) trick
      # in malloc.h which is included pretty much everywhere.
      4548,
      # Copy constructor could not be generated because a base class copy
      # constructor is inaccessible.
      # This is an expected consequence of using DISALLOW_COPY_AND_ASSIGN().
      4625,
      # Assignment operator could not be generated because a base class
      # assignment operator is inaccessible.
      # This is an expected consequence of using DISALLOW_COPY_AND_ASSIGN().
      4626,
      # Digraphs not supported.
      # Thanks god!
      4628,
      # Symbol is not defined as a preprocessor macro, replacing with '0'.
      # Seems like common practice, used in Windows SDK and gtest.
      4668,
      # Function not inlined.
      # It's up to the compiler to decide what to inline.
      4710,
      # Function selected for inline expansion.
      # It's up to the compiler to decide what to inline.
      4711,
      # The type and order of elements caused the compiler to add padding
      # to the end of a struct.
      # Unsurprisingly, most of the structs become larger because of padding
      # but it's a universally acceptable price for better performance.
      4820,
      # Disable static analyzer warning for std::min and std::max with
      # objects.
      # https://connect.microsoft.com/VisualStudio/feedback/details/783808/static-analyzer-warning-c28285-for-std-min-and-std-max
      28285,
    ],
    'target_conditions': [
      ['cobalt_code==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
        ],
      },
      ],
    ],
  }, # end of target_defaults
}
