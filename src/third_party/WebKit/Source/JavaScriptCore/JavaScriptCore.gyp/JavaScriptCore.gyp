#
# Copyright (C) 2009, 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
  'includes': [
    '../JavaScriptCore.gypi',
  ],
  'variables': {
    # Location of the chromium src directory.
    'conditions': [
      ['inside_chromium_build==0', {
        # Webkit is being built outside of the full chromium project.
        'chromium_src_dir': '../../WebKit/chromium',
      },{
        # WebKit is checked out in src/chromium/third_party/WebKit
        'chromium_src_dir': '../../../../..',
      }],
      ['host_os=="win"', {
        'ruby_exe': '<(DEPTH)/third_party/ruby/bin/ruby.exe',
      }, {
        'ruby_exe': 'ruby',
      }],
    ],
  },
  'target_defaults': {
    'msvs_disabled_warnings': [
      # class <foo> needs to have dll-interface to be used by clients of class <bar>
      # This is somewhat unavoidable for classes that are exported but have
      # WTF template member variables.
      4251,
      # A call to a function using explicit template arguments calls a different
      # function than it would if explicit arguments had not been specified.
      4344,
      # A specialization of a function template cannot specify any of the inline
      # specifiers. MSVC compiler mistakenly emits this warning in places where
      # template friend functions are declared.
      4396,
      # This warning is generated when a value that is not bool is assigned or
      # coerced into type bool. Returning integer values from boolean functions
      # is a common practice in WebKit.
      4800,
    ],
  },
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "lb_shell" and gcc_version>=46', {
      'target_defaults': {
        # Disable warnings about c++0x compatibility, as some names (such as nullptr) conflict
        # with upcoming c++0x types.
        'cflags_cc': ['-Wno-c++0x-compat'],
      },
    }],
  ],
  'targets': [
    {
      # This target actually builds JavaScriptCore, as opposed to V8, for
      # when your target architecture isn't supported by v8 :(
      'target_name': 'javascriptcore',
      'type': 'static_library',
      'include_dirs': [
        # prefer these headers to the ones inside ../icu
        '<(DEPTH)/third_party/icu/public/common',
        '<(DEPTH)/third_party/icu/public/i18n',
        '../',
        '../../',
        "<!@(find .. -mindepth 1 -maxdepth 1 -type d -and -not -name 'JavaScriptCore.*' -and -not -name 'os-win32')",
        '../../WTF/wtf/unicode',
        '../../WTF/wtf',
        '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          # prefer these headers to the ones inside ../icu
          '<(DEPTH)/third_party/icu/public/common',
          '<(DEPTH)/third_party/icu/public/i18n',
          "<!@(find .. -mindepth 1 -maxdepth 1 -type d -and -not -name 'JavaScriptCore.*' -and -not -name 'os-win32')",
        ],
        'msvs_disabled_warnings': [
          # A call to a function using explicit template arguments calls a different
          # function than it would if explicit arguments had not been specified.
          4344,
          # A specialization of a function template cannot specify any of the inline
          # specifiers. MSVC compiler mistakenly emits this warning in places where
          # template friend functions are declared.
          4396,
          # This warning is generated when a value that is not bool is assigned or
          # coerced into type bool. Use of integer expressions in boolean contexts
          # is a common practice in WebKit.
          4800,
        ],
      },
      'sources': [
        "<!@(find .. -mindepth 2 -not -path '../wtf/*' -and -not -path '../yarr/*' -and \( -name '*.cpp' -or -name '*.h' \))",
      ],
      'sources/': [
        # Windows-specific:
        ['exclude', 'BSTR'],
        ['exclude', '../os-win32/'],
        # Apple-specific:
        ['exclude', 'CF'],
        # JIT-related:
        ['exclude', '../assembler/'],
      ],
      'dependencies': [
        'jsc_derived_sources',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
        'yarr',
        'llint_assembly',
      ],
      'conditions': [
        [ 'OS=="lb_shell" or OS=="starboard"', {
          'sources!': [
            '../runtime/GCActivityCallbackBlackBerry.cpp',
            '../llint/LLIntOffsetsExtractor.cpp',
          ],
          'sources/': [
            ['exclude', '_nss.cc$'],
            ['exclude', 'dfg/'],
            ['exclude', 'disassembler/'],
          ],
          'defines' : [
            # See JSExportMacros.h and wtf/ExportMacros.h
            'STATICALLY_LINKED_WITH_WTF',
            'BUILDING_JavaScriptCore',
          ],
          'defines!': [
            '__DISABLE_WTF_LOGGING__',
          ],
        }],
        ['webkit_target_type=="shared_library"', {
          'type': 'shared_library',
        }],
        ['target_arch == "ps3"', {
          'sources': [
            '../prx_init.cc',
          ],
          'all_dependent_settings': {
            'defines': [
              # Export this define to all dependents. This lets us know
              # if we need to try to load the javascriptcore library.
              'COBALT_USES_JAVASCRIPTCORE',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'jsc_derived_sources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'CreateHashTables',
          'inputs': [
            '../runtime/ArrayConstructor.cpp',
            '../runtime/ArrayPrototype.cpp',
            '../runtime/BooleanPrototype.cpp',
            '../runtime/DateConstructor.cpp',
            '../runtime/DatePrototype.cpp',
            '../runtime/ErrorPrototype.cpp',
            '../runtime/JSGlobalObject.cpp',
            '../runtime/JSONObject.cpp',
            '../runtime/MathObject.cpp',
            '../runtime/NamePrototype.cpp',
            '../runtime/NumberConstructor.cpp',
            '../runtime/NumberPrototype.cpp',
            '../runtime/ObjectConstructor.cpp',
            '../runtime/ObjectPrototype.cpp',
            '../runtime/RegExpConstructor.cpp',
            '../runtime/RegExpObject.cpp',
            '../runtime/RegExpPrototype.cpp',
            '../runtime/StringConstructor.cpp',
            '../runtime/StringPrototype.cpp',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/ArrayConstructor.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/ArrayPrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/BooleanPrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/DateConstructor.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/DatePrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/ErrorPrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/JSGlobalObject.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/JSONObject.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/MathObject.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/NamePrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/NumberConstructor.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/NumberPrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/ObjectConstructor.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/ObjectPrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/RegExpConstructor.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/RegExpObject.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/RegExpPrototype.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/StringConstructor.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/StringPrototype.lut.h',
          ],
          'action': [
            'python',
            'scripts/action_iterate_over_args.py',
            '../create_hash_table',
            '-i',
            '--',
            '<@(_outputs)',
            '--',
            '<@(_inputs)'
          ],
          'msvs_cygwin_shell': 0,
        },
        {
          'action_name': 'ProcessKeywordsTable',
          'inputs': [
            '../parser/Keywords.table'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/Lexer.lut.h',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/KeywordLookup.h',
          ],
          'action': [
            'python',
            'scripts/action_process_keywords_table.py',
            '<@(_inputs)',
            '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore'
          ],
          'msvs_cygwin_shell': 0,
        }
      ]
    },
    {
      'target_name': 'yarr',
      'type': 'static_library',
      'dependencies': [
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
      ],
      'variables': { 'optimize': 'max' },
      'actions': [
        {
          'action_name': 'retgen',
          'inputs': [
            '../create_regex_tables',
          ],
          'arguments': [
            '--no-tables',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/RegExpJitTables.h',
          ],
          'action': ['python', '<@(_inputs)', '<@(_arguments)', '<@(_outputs)'],
        },
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '../runtime',
      ],
      'sources': [
        "<!@(find ../yarr -name '*.cpp' -or -name '*.h')",
      ],
      'sources/': [
        # The Yarr JIT isn't used in WebCore.
        ['exclude', '../yarr/YarrJIT\\.(h|cpp)$'],
      ],
      'export_dependent_settings': [
        '../../WTF/WTF.gyp/WTF.gyp:wtf',
      ],
      'defines' : [
        'STATICALLY_LINKED_WITH_WTF',
        'BUILDING_JavaScriptCore',
      ],
    },
    {
      'target_name': 'llint_desired_offsets',
      'type': 'none',
      'hard_dependency': 1,
      'actions': [
        {
          'action_name': 'generate LLIntDesiredOffsets.h',
          'variables': {
            'input_rb': '../offlineasm/generate_offset_extractor.rb',
            'input_source' : '../llint/LowLevelInterpreter.asm',
            'extra_inputs' : ['../llint/LowLevelInterpreter32_64.asm',
                              '../llint/LowLevelInterpreter64.asm'],
            'output_h': '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/LLIntDesiredOffsets.h'
          },
          'inputs': [
            '<(input_rb)', '<(input_source)', '<@(extra_inputs)',
          ],
          'outputs': [
            '<(output_h)',
          ],
          'action': [
            '<(ruby_exe)',
            '<(input_rb)',
            '<(input_source)',
            '<(output_h)',
          ],
          'message': 'Generating LLIntDesiredOffsets.h',
        },
      ],
    },
    {
      # This compiles LLIntOffsetsExtractor.cpp
      # into a .o containing the platform-specific offset table
      # for use by the LLIntAssembly generation step.
      # The .a is not used for anything.
      'target_name': 'offsets_extractor',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        'llint_desired_offsets',
      ],
      'include_dirs': [
        '..',
        '../../WTF',
        '../../WTF/icu',
        '../API',
        '../assembler',
        '../bytecode',
        '../debugger',
        '../dfg',
        '../disassembler',
        '../heap',
        '../interpreter',
        '../jit',
        '../llint',
        '../parser',
        '../profiler',
        '../runtime',
        # For LLIntDesiredOffsets.h
        '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore',
      ],
      'defines' : [
        'STATICALLY_LINKED_WITH_WTF',
        'BUILDING_JavaScriptCore',
      ],
      'sources': [
        '../llint/LLIntOffsetsExtractor.cpp',
      ],
      'conditions': [
        [ 'OS=="lb_shell" or OS=="starboard"', {
          'include_dirs': [
            '<(DEPTH)/third_party/icu/public/common',
          ],
          'include_dirs!': [
            '../../WTF/icu',
          ],
        }],
      ],
    },
    {
      'target_name': 'llint_assembly',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'offsets_extractor',
      ],
      'actions': [
        {
          'action_name': 'generate LLIntAssembly.h',
          'variables': {
            'input_rb': '../offlineasm/asm.rb',
            'input_asm': '../llint/LowLevelInterpreter.asm',
            'extra_inputs' : ['../llint/LowLevelInterpreter32_64.asm',
                              '../llint/LowLevelInterpreter64.asm',
                              '../offlineasm/cloop.rb',],
            # TODO: Can this path be specified more nicely?
            'conditions': [
              ['cobalt==1 and actual_target_arch=="win"', {
                'input_binary': '<(PRODUCT_DIR)/obj/third_party/WebKit/Source/JavaScriptCore/llint/offsets_extractor.LLIntOffsetsExtractor.obj',
              }, {
                'input_binary': '<(PRODUCT_DIR)/obj/third_party/WebKit/Source/JavaScriptCore/llint/offsets_extractor.LLIntOffsetsExtractor.o',
              }],
            ],
            'output_h': '<(SHARED_INTERMEDIATE_DIR)/JavaScriptCore/LLIntAssembly.h',
          },
          'inputs' : [
            '<(input_rb)',
            '<(input_asm)',
            '<(input_binary)',
            '<@(extra_inputs)',
          ],
          'outputs' : [
            '<(output_h)',
          ],
          'action': [
            '<(ruby_exe)',
            '<(input_rb)',
            '<(input_asm)',
            '<(input_binary)',
            '<(output_h)',
          ],
          'message': 'Generating LLIntAssembly.h',
        },
      ],
    },
  ], # targets
}
