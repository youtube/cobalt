# Copyright 2016 Google Inc. All Rights Reserved.
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
  'includes': [
    'mozjs.gypi',
  ],
  'variables': {
    'common_jit_defines': [
      'JS_ION=1',
      'JS_METHODJIT=1',
      'JS_METHODJIT_TYPED_ARRAY=1',
      'ENABLE_YARR_JIT=1',
    ],
  },
  'target_defaults': {
    'defines': [
      # This makes it so the embedded JavaScript that is generated in the
      # CreateEmbeddedJS action is compressed.
      'USE_ZLIB',
      # Do not use the default js_xxx allocator function implementations, but
      # include "jscustomallocator.h", which should implement them.
      'JS_USE_CUSTOM_ALLOCATOR',
      # Do not export symbols that are declare with JS_PUBLIC_[API|DATA].
      'STATIC_JS_API',
    ],
    'include_dirs': [
      'cobalt_config/include',
      'js/src',
      'js/src/assembler',
      'mfbt/double-conversion',
      '<(DEPTH)/third_party/icu/public/common',
      '<(generated_include_directory)',
    ],
    'conditions': [
      [ 'target_arch == "x64"', {
        'defines': [
          'JS_CPU_X64=1',
          'JS_PUNBOX64=1',
          '<@(common_jit_defines)',
        ],
      }],
      [ 'target_arch == "x86"', {
        'defines': [
          'JS_CPU_X86=1',
          'JS_NUNBOX32=1',
          '<@(common_jit_defines)',
        ],
      }],
      [ 'target_arch == "arm"', {
        'defines': [
          'JS_CPU_ARM=1',
          'JS_NUNBOX32=1',
          '<@(common_jit_defines)',
        ],
      }],
    ],
  },
  'targets': [
    {
      # The SpiderMonkey library.
      'target_name': 'mozjs_lib',
      'type': 'static_library',
      'sources': [ '<@(mozjs_sources)' ],
      'defines': [
        # Enable incremental garbage collection.
        'JSGC_INCREMENTAL=1',

        # See vm/Probes.h for different values. This was the value set by the
        # configure script when building for linux-release.
        'JS_DEFAULT_JITREPORT_GRANULARITY=3',

        # Set this to enable the ECMAScript Internationalization API. Consider
        # doing this after upgrading to a more recent version of ICU, since
        # Cobalt's is older than the one SpiderMonkey needs for this.
        # 'ENABLE_INTL_API=1',
      ],
      'cflags' : [
        '-Wno-invalid-offsetof',
        '-Wno-uninitialized',
        '-Wno-unused',
        '-include',
        'js-confdefs.h',
      ],
      'conditions': [
        [ 'target_arch == "x64"', {
          'sources': [
            'js/src/assembler/assembler/MacroAssemblerX86Common.cpp',
            'js/src/jit/shared/Assembler-x86-shared.cpp',
            'js/src/jit/shared/BaselineCompiler-x86-shared.cpp',
            'js/src/jit/shared/BaselineIC-x86-shared.cpp',
            'js/src/jit/shared/CodeGenerator-x86-shared.cpp',
            'js/src/jit/shared/IonFrames-x86-shared.cpp',
            'js/src/jit/shared/Lowering-x86-shared.cpp',
            'js/src/jit/shared/MoveEmitter-x86-shared.cpp',
            'js/src/jit/x64/Assembler-x64.cpp',
            'js/src/jit/x64/Bailouts-x64.cpp',
            'js/src/jit/x64/BaselineCompiler-x64.cpp',
            'js/src/jit/x64/BaselineIC-x64.cpp',
            'js/src/jit/x64/CodeGenerator-x64.cpp',
            'js/src/jit/x64/Lowering-x64.cpp',
            'js/src/jit/x64/MacroAssembler-x64.cpp',
            'js/src/jit/x64/Trampoline-x64.cpp',
            '<@(mozjs_jit_sources)',
          ],
        }],
        [ 'target_arch == "x86"', {
          'sources': [
            'js/src/assembler/assembler/MacroAssemblerX86Common.cpp',
            'js/src/jit/shared/Assembler-x86-shared.cpp',
            'js/src/jit/shared/BaselineCompiler-x86-shared.cpp',
            'js/src/jit/shared/BaselineIC-x86-shared.cpp',
            'js/src/jit/shared/CodeGenerator-x86-shared.cpp',
            'js/src/jit/shared/IonFrames-x86-shared.cpp',
            'js/src/jit/shared/Lowering-x86-shared.cpp',
            'js/src/jit/shared/MoveEmitter-x86-shared.cpp',
            'js/src/jit/x86/Assembler-x86.cpp',
            'js/src/jit/x86/Bailouts-x86.cpp',
            'js/src/jit/x86/BaselineCompiler-x86.cpp',
            'js/src/jit/x86/BaselineIC-x86.cpp',
            'js/src/jit/x86/CodeGenerator-x86.cpp',
            'js/src/jit/x86/Lowering-x86.cpp',
            'js/src/jit/x86/MacroAssembler-x86.cpp',
            'js/src/jit/x86/Trampoline-x86.cpp',
            '<@(mozjs_jit_sources)',
          ],
        }],
        [ 'target_arch == "arm"', {
          'sources': [
            'js/src/assembler/assembler/ARMAssembler.cpp',
            'js/src/assembler/assembler/MacroAssemblerARM.cpp',
            'js/src/jit/arm/Architecture-arm.cpp',
            'js/src/jit/arm/Assembler-arm.cpp',
            'js/src/jit/arm/Bailouts-arm.cpp',
            'js/src/jit/arm/BaselineCompiler-arm.cpp',
            'js/src/jit/arm/BaselineIC-arm.cpp',
            'js/src/jit/arm/CodeGenerator-arm.cpp',
            'js/src/jit/arm/IonFrames-arm.cpp',
            'js/src/jit/arm/Lowering-arm.cpp',
            'js/src/jit/arm/MacroAssembler-arm.cpp',
            'js/src/jit/arm/MoveEmitter-arm.cpp',
            'js/src/jit/arm/Trampoline-arm.cpp',
            '<@(mozjs_jit_sources)',
          ],
        }],
      ],
      'dependencies': [
        'build_include_directory',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'cobalt_config/include',
          'js/src',
          '<(generated_include_directory)',
        ],
        'defines': [
          'JS_USE_CUSTOM_ALLOCATOR',
          'STATIC_JS_API',
        ],
        'cflags' : [
          '-Wno-invalid-offsetof',
          '-Wno-uninitialized',
          '-Wno-unused',
          '-include',
          'js-confdefs.h',
        ],
      },
      # Mark this target as a hard dependency because targets that depend on
      # this one need to wait for the build_include_directory to be generated.
      # The more correct things would be to insert that target as a dependency
      # in direct_dependent_settings, but it wasn't as straightforward as I'd
      # hoped.
      'hard_dependency': 1,
    },
    {
      # Command line SpiderMonkey shell.
      'target_name': 'mozjs_shell',
      'type': '<(final_executable_type)',
      'sources': [
        'js/src/shell/js.cpp',
        'js/src/shell/jsheaptools.cpp',
        'js/src/shell/jsoptparse.cpp',
      ],
      'dependencies': [
        'mozjs_lib',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ]
    },
    {
      # SpiderMonkey source expects to include files from a certain directory
      # structure that is made by copying (or symlinking) the real source files
      # into the destination directory structure.
      'target_name': 'build_include_directory',
      'type': 'none',
      'copies': [
        {
          'destination': '<(generated_include_directory)/mozilla',
          'files': [
            'mfbt/Assertions.h',
            'mfbt/Atomics.h',
            'mfbt/Attributes.h',
            'mfbt/BloomFilter.h',
            'mfbt/Casting.h',
            'mfbt/Char16.h',
            'mfbt/CheckedInt.h',
            'mfbt/Compiler.h',
            'mfbt/Constants.h',
            'mfbt/DebugOnly.h',
            'mfbt/decimal/Decimal.h',
            'mfbt/Endian.h',
            'mfbt/EnumSet.h',
            'mfbt/FloatingPoint.h',
            'mfbt/GuardObjects.h',
            'mfbt/HashFunctions.h',
            'mfbt/Likely.h',
            'mfbt/LinkedList.h',
            'mfbt/MathAlgorithms.h',
            'mfbt/MemoryChecking.h',
            'mfbt/MSStdInt.h',
            'mfbt/NullPtr.h',
            'mfbt/PodOperations.h',
            'mfbt/Poison.h',
            'mfbt/Range.h',
            'mfbt/RangedPtr.h',
            'mfbt/RefPtr.h',
            'mfbt/Scoped.h',
            'mfbt/SHA1.h',
            'mfbt/SplayTree.h',
            'mfbt/StandardInteger.h',
            'mfbt/ThreadLocal.h',
            'mfbt/TypedEnum.h',
            'mfbt/Types.h',
            'mfbt/TypeTraits.h',
            'mfbt/Util.h',
            'mfbt/WeakPtr.h',
          ],
        },
        {
          'destination': '<(generated_include_directory)/js',
          'files': [
            'js/public/Anchor.h',
            'js/public/CallArgs.h',
            'js/public/CharacterEncoding.h',
            'js/public/Date.h',
            'js/public/GCAPI.h',
            'js/public/HashTable.h',
            'js/public/HeapAPI.h',
            'js/public/LegacyIntTypes.h',
            'js/public/MemoryMetrics.h',
            'js/public/PropertyKey.h',
            'js/public/RequiredDefines.h',
            'js/public/RootingAPI.h',
            'js/public/TemplateLib.h',
            'js/public/Utility.h',
            'js/public/Value.h',
            'js/public/Vector.h',
          ],
        },
        {
          'destination': '<(generated_include_directory)',
          'files': [
            'js/src/perf/jsperf.h',
          ],
        },
      ],
      'dependencies': [
        'generated_headers',
      ],
      'hard_dependency': 1,
    },
    {
      'target_name': 'generated_headers',
      'type': 'none',
      'actions': [
        {
          # Embed JavaScript source into a header file.
          'action_name': 'CreateEmbeddedJS',
          'inputs': [ '<@(embedded_js_sources)', 'js/src/js.msg'],
          'outputs': [
            '<(embedded_js_file)',
            '<(embedded_js_header)',
          ],
          'action': [
            'python',
            'js/src/builtin/embedjs.py',
            '-DUSE_ZLIB',
            '-p',
            '<(CC) -E',
            '-m',
            'js/src/js.msg',
            '-o',
            '<(embedded_js_header)',
            '-s',
            '<(embedded_js_file)',
            '<@(embedded_js_sources)'
          ],
        },
        {
          'action_name': 'CreateKeywordHeader',
          'inputs': [],
          'outputs': [ '<(generated_keyword_header)' ],
          'action': [
            '<(PRODUCT_DIR)/mozjs_keyword_header_gen',
            '<(generated_keyword_header)',
          ],
        },
        {
          'action_name': 'CreateOpcodeLengthHeader',
          'inputs': [],
          'outputs': [ '<(generated_opcode_length_header)' ],
          'action': [
            '<(PRODUCT_DIR)/mozjs_opcode_length_header_gen',
            '<(generated_opcode_length_header)',
          ],
        },
      ],
      'dependencies': [
        'mozjs_keyword_header_gen#host',
        'mozjs_opcode_length_header_gen#host'
      ],
      'hard_dependency': 1,
    },
    {
      # Host tool used to generate a header file that defines a huge switch
      # statement for JavaScript keywords.
      'target_name': 'mozjs_keyword_header_gen',
      'type': '<(final_executable_type)',
      'toolsets': ['host'],
      'sources': [
        'js/src/jskwgen.cpp',
      ],
      'hard_dependency': 1,
    },
    {
      # Host tool used to generate a header file that defines opcode lengths.
      'target_name': 'mozjs_opcode_length_header_gen',
      'type': '<(final_executable_type)',
      'toolsets': ['host'],
      'sources': [
        'js/src/jsoplengen.cpp',
      ],
      'hard_dependency': 1,
    },
  ]
}
