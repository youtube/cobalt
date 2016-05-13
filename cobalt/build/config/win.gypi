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
    # required (e.g. platform_delegate_win.cc), actual_target_arch is used
    # instead.
    'target_arch': 'xb1',
    'actual_target_arch': 'win',
    'posix_emulation_target_type': 'shared_library',
    'webkit_target_type': 'shared_library',
    'angle_api': 'win32',  # TODO: Remove in favor of |gl_type|.
    'enable_remote_debugging': 0,

    'gl_type': 'angle',

    # Compile with "PREfast" on by default.
    'static_analysis%': 'true',

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
      # Disable suggestions to switch to Microsoft-specific secure CRT.
      '_CRT_SECURE_NO_WARNINGS',
      # Disable support for exceptions in STL in order to detect their use
      # (the use of exceptions is forbidden by Google C++ style guide).
      '_HAS_EXCEPTIONS=0',
    ],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'ForcedIncludeFiles': ['posix_emulation.h'],
        # Override with our own version of problematic system headers.
        'AdditionalIncludeDirectories': ['<(DEPTH)/lbshell/src/platform/xb1/msvc'],
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
        'WarnAsError': 'true',
        # Enable all warnings, even those that are disabled by default.
        # See https://msdn.microsoft.com/en-us/library/23k5d385.aspx
        'WarningLevel': 'all',

        'AdditionalOptions': [
          '/errorReport:none', # don't send error reports to MS.
        ],
      },
      'VCLinkerTool': {
        'RandomizedBaseAddress': '2', # /DYNAMICBASE
        # TODO(***REMOVED***): SubSystem is hardcoded in
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
          # Since Windows is not a production platform, we don't care.
          # TODO(***REMOVED***): Remove after removing ComponentExtensions
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
      # An rvalue cannot be bound to a non-const reference.
      # In previous versions of Visual C++, it was possible to bind an rvalue
      # to a non-const reference in a direct initialization. This warning
      # is useless as it simply describes proper C++ behavior.
      4350,
      # layout of class may have changed from a previous version of
      # the compiler due to better packing of member. We don't care about
      # binary compatibility with other compiler versions.
      4371,
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
      ['_type=="executable"', {
        'sources': [
          '<(DEPTH)/lbshell/src/platform/xb1/main_wrapper_win.cc',
        ],
      }],
      ['_type in ["executable", "shared_library"]', {
        # Everyone links in their own copy of operator new and friends.
        'sources': [
          '<(PRODUCT_DIR)/obj/lbshell/src/posix_emulation.lb_new.o',
        ],
      }],
      ['cobalt_code==0', {
        # TODO(***REMOVED***): Fix warnings below or disable them on per-module basis.
        'msvs_disabled_warnings': [
          4244, # Conversion from 'type1' to 'type2', possible loss of data.
          4996, # Using unsecure or deprecated version of a function.
          4267, # Conversion from 'size_t' to 'type', possible loss of data.
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'EnablePREfast': '',
            'WarningLevel': '3',
          },
        },
      }],
      ['cobalt_code==1', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            # Enables Native Code Analysis - a static code analysis by Microsoft.
            'EnablePREfast': '<(static_analysis)',
            'AdditionalOptions': [
              # TODO(***REMOVED***): Enable SDL everywhere once C4996 warning is fixed.
              '/sdl', # Security Development Lifecycle checks.
            ],
          },
        },
      }],
    ],

    'default_configuration': 'win_debug',
    'configurations': {
      'msvs_base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\build\\<(build_dir_prefix)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'ws2_32.lib',
            ],
          },
        },
        # TODO(***REMOVED***): For now, msvs_configuration_platform and
        # msvs_target_platform serve similar purposes. Investigate, if ever,
        # when porting to x86.
        'msvs_target_platform': 'x64',
        # Add the default import libs.
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
      'win_debug': {
        'inherit_from': ['debug_base', 'msvs_base'],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'BasicRuntimeChecks': '3', # Check stack frame validity and check for uninitialized variables at run time.
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
      'win_devel': {
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
      'win_qa': {
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
      'win_gold': {
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
