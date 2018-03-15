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
    'mozjs-45.gypi',
  ],
  'variables': {
    'generated_include_directory': '<(SHARED_INTERMEDIATE_DIR)/mozjs-45/include',

    'common_cflags': [
      '-include',
      '<(DEPTH)/third_party/mozjs-45/cobalt_config/include/js-confdefs.h',
      '-Wno-inline-new-delete',
      '-Wno-invalid-offsetof',
      '-Wno-unused-function',
    ],
    'common_defines': [
      # Do not export symbols that are declare with JS_PUBLIC_[API|DATA].
      'STATIC_JS_API=1',
      # Do not use the default js_xxx allocator function implementations, but
      # include "jscustomallocator.h", which should implement them.
      'JS_USE_CUSTOM_ALLOCATOR=1',
      # See vm/Probes.h for different values. This was the value set by the
      # configure script when building for linux-release.
      'JS_DEFAULT_JITREPORT_GRANULARITY=3',
      # |COBALT_JS_GC_TRACE| can be set here to enable the JSGCThingTracer,
      # which logs GCThings that are alive in the JavaScript heap, along with
      # where each thing was allocated, every 15 minutes.
    ],
    'common_include_dirs': [
      '<(DEPTH)/third_party/mozjs-45/js/src',
      '<(generated_include_directory)',
      'cobalt_config/include',
      '<(DEPTH)/third_party/icu/source/common',
    ],
    'common_msvs_force_includes': [
      'third_party/mozjs-45/cobalt_config/include/js-confdefs.h',
    ],
    'common_msvs_disabled_warnings': [
      # Level 2, Possible loss of data due to type conversion.
      4244,
      # Level 3, Possible loss of data due to type conversion from size_t.
      4267,
      # Level 1, conversion from 'type1' to 'type2' of greater size.
      4312,
      # Level 1, no suitable definition provided for explicit template
      # instantiation request.
      4661,
    ],

    'conditions': [
      ['target_arch == "x86"', {
        'common_defines': [
          'JS_CPU_X86=1',
          'JS_CODEGEN_X86=1',
          'JS_NUNBOX32=1',
        ],
      }],
      ['target_arch == "x64"', {
        'common_defines': [
          'JS_CPU_X64=1',
          'JS_CODEGEN_X64=1',
          'JS_PUNBOX64=1',
        ],
      }],
      ['target_arch == "arm"', {
        'common_defines': [
          'JS_CPU_ARM=1',
          'JS_CODEGEN_ARM=1',
          'JS_NUNBOX32=1',
        ],
      }],
      ['target_arch == "arm64"', {
        'common_defines': [
          'JS_CPU_ARM64=1',
          'JS_CODEGEN_ARM64=1',
          'JS_PUNBOX64=1',
          # arm64 jit appears to not be ready, won't even compile without
          # compiling in the simulator.  It is highly recommended that
          # |cobalt_enable_jit| be set to |0| when building for architecture
          # |arm64|.
          'JS_SIMULATOR=1',
          'JS_SIMULATOR_ARM64=1',
        ],
      }],
      ['target_arch == "mips"', {
        'common_defines': [
          'JS_CPU_MIPS=1',
          'JS_CODEGEN_MIPS32=1',
          'JS_NUNBOX32=1',
        ],
      }],
      ['target_arch == "mips64"', {
        'common_defines': [
          'JS_CPU_MIPS=1',
          'JS_CODEGEN_MIPS64=1',
          'JS_PUNBOX64=1',
        ],
      }],

      # TODO: Remove once ps4 configuration todos are addressed.
      ['target_arch == "ps4" or sb_target_platform == "ps4"', {
        'common_defines': [
          'JS_CPU_X64=1',
          'JS_CODEGEN_X64=1',
          'JS_PUNBOX64=1',
        ],
      }],

      ['cobalt_config != "gold"', {
        'common_defines': [
          'JS_TRACE_LOGGING=1',
        ],
      }],
      ['cobalt_config == "debug"', {
        'common_defines': [
          'JS_JITSPEW=1',
          'JS_METHODJIT_SPEW=1',
        ],
      }]
    ],
  },
  # Required GYP massaging to allow |cobalt_enable_jit| to be a default
  # variable.  Just pretend this is part of |variables| above.
  'conditions': [
    ['cobalt_enable_jit != 1', {
      'variables': {
        'common_defines': [
          'COBALT_DISABLE_JIT=1',
        ],
      },
    }],
  ],
  'target_defaults': {
    'defines': [ '<@(common_defines)', ],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'ForcedIncludeFiles': ['<@(common_msvs_force_includes)', ],
      },
    },
    'msvs_disabled_warnings': [
      '<@(common_msvs_disabled_warnings)',
      # Level 2, unary minus operator applied to unsigned type, result still
      # unsigned.
      4146,
      # Level 1, unsafe use of type 'bool' in operation.
      4804,
      # Level 1, unsafe mix of type 'type' and type 'type' in operation
      4805,
    ],

    # Unfortunately, there is code that generate warnings in the headers.
    'direct_dependent_settings': {
      'msvs_disabled_warnings': [ '<@(common_msvs_disabled_warnings)', ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ForcedIncludeFiles': [ '<@(common_msvs_force_includes)', ],
        },
      },
    },
  },

  'targets': [
    {
      'target_name': 'mozjs-45_lib',
      'type': 'static_library',
      'cflags': ['<@(common_cflags)'],
      'dependencies': [
        'build_include_directory',
        '<(DEPTH)/starboard/client_porting/pr_starboard/pr_starboard.gyp:pr_starboard',
        '<(DEPTH)/starboard/starboard.gyp:starboard',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ '<@(common_include_dirs)' ],
        'cflags': ['<@(common_cflags)'],
        'defines': ['<@(common_defines)'],
      },
      'export_dependent_settings': [
        'build_include_directory',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/mozjs-45/js/src',
        '<@(common_include_dirs)',
      ],
      'sources': [
        '<@(mozjs-45_sources)',
      ],
      'conditions': [
        ['target_arch == "x86"', {
          'sources': [
            'js/src/jit/x86-shared/Architecture-x86-shared.cpp',
            'js/src/jit/x86-shared/Assembler-x86-shared.cpp',
            'js/src/jit/x86-shared/AssemblerBuffer-x86-shared.cpp',
            'js/src/jit/x86-shared/BaselineCompiler-x86-shared.cpp',
            'js/src/jit/x86-shared/BaselineIC-x86-shared.cpp',
            'js/src/jit/x86-shared/CodeGenerator-x86-shared.cpp',
            'js/src/jit/x86-shared/Disassembler-x86-shared.cpp',
            'js/src/jit/x86-shared/Lowering-x86-shared.cpp',
            'js/src/jit/x86-shared/MacroAssembler-x86-shared.cpp',
            'js/src/jit/x86-shared/MoveEmitter-x86-shared.cpp',
            'js/src/jit/x86/Assembler-x86.cpp',
            'js/src/jit/x86/Bailouts-x86.cpp',
            'js/src/jit/x86/BaselineCompiler-x86.cpp',
            'js/src/jit/x86/BaselineIC-x86.cpp',
            'js/src/jit/x86/CodeGenerator-x86.cpp',
            'js/src/jit/x86/Lowering-x86.cpp',
            'js/src/jit/x86/MacroAssembler-x86.cpp',
            'js/src/jit/x86/SharedIC-x86.cpp',
            'js/src/jit/x86/Trampoline-x86.cpp',
          ],
        }],
        # TODO: Remove "* == ps4" once ps4 configuration todos are addressed.
        ['target_arch == "x64" or target_arch == "ps4" or sb_target_platform == "ps4"', {
          'sources': [
            'js/src/jit/x64/Assembler-x64.cpp',
            'js/src/jit/x64/Bailouts-x64.cpp',
            'js/src/jit/x64/BaselineCompiler-x64.cpp',
            'js/src/jit/x64/BaselineIC-x64.cpp',
            'js/src/jit/x64/CodeGenerator-x64.cpp',
            'js/src/jit/x64/Lowering-x64.cpp',
            'js/src/jit/x64/MacroAssembler-x64.cpp',
            'js/src/jit/x64/SharedIC-x64.cpp',
            'js/src/jit/x64/Trampoline-x64.cpp',
            'js/src/jit/x86-shared/Architecture-x86-shared.cpp',
            'js/src/jit/x86-shared/Assembler-x86-shared.cpp',
            'js/src/jit/x86-shared/AssemblerBuffer-x86-shared.cpp',
            'js/src/jit/x86-shared/BaselineCompiler-x86-shared.cpp',
            'js/src/jit/x86-shared/BaselineIC-x86-shared.cpp',
            'js/src/jit/x86-shared/CodeGenerator-x86-shared.cpp',
            'js/src/jit/x86-shared/Disassembler-x86-shared.cpp',
            'js/src/jit/x86-shared/Lowering-x86-shared.cpp',
            'js/src/jit/x86-shared/MacroAssembler-x86-shared.cpp',
            'js/src/jit/x86-shared/MoveEmitter-x86-shared.cpp',
          ],
        }],
        ['target_arch == "arm"', {
          'sources': [
            'js/src/jit/arm/Architecture-arm.cpp',
            'js/src/jit/arm/Architecture-arm.h',
            'js/src/jit/arm/Assembler-arm.cpp',
            'js/src/jit/arm/Assembler-arm.h',
            'js/src/jit/arm/AtomicOperations-arm.h',
            'js/src/jit/arm/Bailouts-arm.cpp',
            'js/src/jit/arm/BaselineCompiler-arm.cpp',
            'js/src/jit/arm/BaselineCompiler-arm.h',
            'js/src/jit/arm/BaselineIC-arm.cpp',
            'js/src/jit/arm/CodeGenerator-arm.cpp',
            'js/src/jit/arm/CodeGenerator-arm.h',
            'js/src/jit/arm/disasm/Constants-arm.cpp',
            'js/src/jit/arm/disasm/Constants-arm.h',
            'js/src/jit/arm/disasm/Disasm-arm.cpp',
            'js/src/jit/arm/disasm/Disasm-arm.h',
            'js/src/jit/arm/LIR-arm.h',
            'js/src/jit/arm/LOpcodes-arm.h',
            'js/src/jit/arm/Lowering-arm.cpp',
            'js/src/jit/arm/Lowering-arm.h',
            'js/src/jit/arm/MacroAssembler-arm-inl.h',
            'js/src/jit/arm/MacroAssembler-arm.cpp',
            'js/src/jit/arm/MacroAssembler-arm.h',
            'js/src/jit/arm/MoveEmitter-arm.cpp',
            'js/src/jit/arm/MoveEmitter-arm.h',
            'js/src/jit/arm/SharedIC-arm.cpp',
            'js/src/jit/arm/SharedICHelpers-arm.h',
            'js/src/jit/arm/SharedICRegisters-arm.h',
            'js/src/jit/arm/Trampoline-arm.cpp',
          ],
        }],
        ['target_arch == "arm64"', {
          'sources': [
            'js/src/jit/arm64/Architecture-arm64.cpp',
            'js/src/jit/arm64/Architecture-arm64.h',
            'js/src/jit/arm64/Assembler-arm64.cpp',
            'js/src/jit/arm64/Assembler-arm64.h',
            'js/src/jit/arm64/AtomicOperations-arm64.h',
            'js/src/jit/arm64/Bailouts-arm64.cpp',
            'js/src/jit/arm64/BaselineCompiler-arm64.h',
            'js/src/jit/arm64/BaselineIC-arm64.cpp',
            'js/src/jit/arm64/CodeGenerator-arm64.cpp',
            'js/src/jit/arm64/CodeGenerator-arm64.h',
            'js/src/jit/arm64/DoubleEntryTable.tbl',
            'js/src/jit/arm64/gen-double-encoder-table.py',
            'js/src/jit/arm64/LIR-arm64.h',
            'js/src/jit/arm64/LOpcodes-arm64.h',
            'js/src/jit/arm64/Lowering-arm64.cpp',
            'js/src/jit/arm64/Lowering-arm64.h',
            'js/src/jit/arm64/MacroAssembler-arm64-inl.h',
            'js/src/jit/arm64/MacroAssembler-arm64.cpp',
            'js/src/jit/arm64/MacroAssembler-arm64.h',
            'js/src/jit/arm64/MoveEmitter-arm64.cpp',
            'js/src/jit/arm64/MoveEmitter-arm64.h',
            'js/src/jit/arm64/SharedIC-arm64.cpp',
            'js/src/jit/arm64/SharedICHelpers-arm64.h',
            'js/src/jit/arm64/SharedICRegisters-arm64.h',
            'js/src/jit/arm64/Trampoline-arm64.cpp',
            'js/src/jit/arm64/vixl/Assembler-vixl.cpp',
            'js/src/jit/arm64/vixl/Cpu-vixl.cpp',
            'js/src/jit/arm64/vixl/Debugger-vixl.cpp',
            'js/src/jit/arm64/vixl/Decoder-vixl.cpp',
            'js/src/jit/arm64/vixl/Disasm-vixl.cpp',
            'js/src/jit/arm64/vixl/Instructions-vixl.cpp',
            'js/src/jit/arm64/vixl/Instrument-vixl.cpp',
            'js/src/jit/arm64/vixl/Logic-vixl.cpp',
            'js/src/jit/arm64/vixl/MacroAssembler-vixl.cpp',
            'js/src/jit/arm64/vixl/MozAssembler-vixl.cpp',
            'js/src/jit/arm64/vixl/MozInstructions-vixl.cpp',
            'js/src/jit/arm64/vixl/MozSimulator-vixl.cpp',
            'js/src/jit/arm64/vixl/Simulator-vixl.cpp',
            'js/src/jit/arm64/vixl/Utils-vixl.cpp',
          ],
        }],
        ['target_arch == "mips"', {
          'sources': [
            'js/src/jit/mips-shared/Architecture-mips-shared.cpp',
            'js/src/jit/mips-shared/Assembler-mips-shared.cpp',
            'js/src/jit/mips-shared/Bailouts-mips-shared.cpp',
            'js/src/jit/mips-shared/BaselineCompiler-mips-shared.cpp',
            'js/src/jit/mips-shared/BaselineIC-mips-shared.cpp',
            'js/src/jit/mips-shared/CodeGenerator-mips-shared.cpp',
            'js/src/jit/mips-shared/Lowering-mips-shared.cpp',
            'js/src/jit/mips-shared/MacroAssembler-mips-shared.cpp',
            'js/src/jit/mips-shared/MoveEmitter-mips-shared.cpp',
            'js/src/jit/mips32/Architecture-mips32.cpp',
            'js/src/jit/mips32/Assembler-mips32.cpp',
            'js/src/jit/mips32/Bailouts-mips32.cpp',
            'js/src/jit/mips32/BaselineCompiler-mips32.cpp',
            'js/src/jit/mips32/BaselineIC-mips32.cpp',
            'js/src/jit/mips32/CodeGenerator-mips32.cpp',
            'js/src/jit/mips32/Lowering-mips32.cpp',
            'js/src/jit/mips32/MacroAssembler-mips32.cpp',
            'js/src/jit/mips32/MoveEmitter-mips32.cpp',
            'js/src/jit/mips32/SharedIC-mips32.cpp',
            'js/src/jit/mips32/Trampoline-mips32.cpp',
          ],
        }],
        ['target_arch == "mips64"', {
          'sources': [
            'js/src/jit/mips-shared/Architecture-mips-shared.cpp',
            'js/src/jit/mips-shared/Assembler-mips-shared.cpp',
            'js/src/jit/mips-shared/Bailouts-mips-shared.cpp',
            'js/src/jit/mips-shared/BaselineCompiler-mips-shared.cpp',
            'js/src/jit/mips-shared/BaselineIC-mips-shared.cpp',
            'js/src/jit/mips-shared/CodeGenerator-mips-shared.cpp',
            'js/src/jit/mips-shared/Lowering-mips-shared.cpp',
            'js/src/jit/mips-shared/MacroAssembler-mips-shared.cpp',
            'js/src/jit/mips-shared/MoveEmitter-mips-shared.cpp',
            'js/src/jit/mips64/Architecture-mips64.cpp',
            'js/src/jit/mips64/Assembler-mips64.cpp',
            'js/src/jit/mips64/Bailouts-mips64.cpp',
            'js/src/jit/mips64/BaselineCompiler-mips64.cpp',
            'js/src/jit/mips64/BaselineIC-mips64.cpp',
            'js/src/jit/mips64/CodeGenerator-mips64.cpp',
            'js/src/jit/mips64/Lowering-mips64.cpp',
            'js/src/jit/mips64/MacroAssembler-mips64.cpp',
            'js/src/jit/mips64/MoveEmitter-mips64.cpp',
            'js/src/jit/mips64/SharedIC-mips64.cpp',
            'js/src/jit/mips64/Trampoline-mips64.cpp',
          ],
        }],
      ],
    },

    {
      'target_name': 'mozjs-45_shell',
      'type': '<(final_executable_type)',
      'sources': [
        'js/src/shell/js.cpp',
        'js/src/shell/jsoptparse.cpp',
        'js/src/shell/OSObject.cpp',
      ],
      'dependencies': [
        'mozjs-45_lib',
      ],
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
            'memory/fallible/fallible.h',
            'memory/mozalloc/mozalloc.h',
            'memory/mozalloc/mozalloc_abort.h',
            'memory/mozalloc/mozalloc_oom.h',
            'memory/mozalloc/msvc_raise_wrappers.h',
            'memory/mozalloc/throw_gcc.h',
            'memory/mozalloc/throw_msvc.h',
            'mfbt/Alignment.h',
            'mfbt/AllocPolicy.h',
            'mfbt/AlreadyAddRefed.h',
            'mfbt/Array.h',
            'mfbt/ArrayUtils.h',
            'mfbt/Assertions.h',
            'mfbt/Atomics.h',
            'mfbt/Attributes.h',
            'mfbt/BinarySearch.h',
            'mfbt/BloomFilter.h',
            'mfbt/Casting.h',
            'mfbt/ChaosMode.h',
            'mfbt/Char16.h',
            'mfbt/CheckedInt.h',
            'mfbt/Compiler.h',
            'mfbt/Compression.h',
            'mfbt/DebugOnly.h',
            'mfbt/decimal/Decimal.h',
            'mfbt/decimal/moz-decimal-utils.h',
            'mfbt/double-conversion/bignum-dtoa.h',
            'mfbt/double-conversion/bignum.h',
            'mfbt/double-conversion/cached-powers.h',
            'mfbt/double-conversion/diy-fp.h',
            'mfbt/double-conversion/double-conversion.h',
            'mfbt/double-conversion/fast-dtoa.h',
            'mfbt/double-conversion/fixed-dtoa.h',
            'mfbt/double-conversion/ieee.h',
            'mfbt/double-conversion/strtod.h',
            'mfbt/double-conversion/utils.h',
            'mfbt/Endian.h',
            'mfbt/EnumeratedArray.h',
            'mfbt/EnumeratedRange.h',
            'mfbt/EnumSet.h',
            'mfbt/FastBernoulliTrial.h',
            'mfbt/FloatingPoint.h',
            'mfbt/Function.h',
            'mfbt/GuardObjects.h',
            'mfbt/HashFunctions.h',
            'mfbt/IndexSequence.h',
            'mfbt/IntegerPrintfMacros.h',
            'mfbt/IntegerRange.h',
            'mfbt/IntegerTypeTraits.h',
            'mfbt/JSONWriter.h',
            'mfbt/Likely.h',
            'mfbt/LinkedList.h',
            'mfbt/LinuxSignal.h',
            'mfbt/lz4.h',
            'mfbt/MacroArgs.h',
            'mfbt/MacroForEach.h',
            'mfbt/MathAlgorithms.h',
            'mfbt/Maybe.h',
            'mfbt/MaybeOneOf.h',
            'mfbt/MemoryChecking.h',
            'mfbt/MemoryReporting.h',
            'mfbt/Move.h',
            'mfbt/NullPtr.h',
            'mfbt/NumericLimits.h',
            'mfbt/Opaque.h',
            'mfbt/Pair.h',
            'mfbt/PodOperations.h',
            'mfbt/Poison.h',
            'mfbt/Range.h',
            'mfbt/RangedArray.h',
            'mfbt/RangedPtr.h',
            'mfbt/ReentrancyGuard.h',
            'mfbt/RefCounted.h',
            'mfbt/RefCountType.h',
            'mfbt/RefPtr.h',
            'mfbt/ReverseIterator.h',
            'mfbt/RollingMean.h',
            'mfbt/Scoped.h',
            'mfbt/ScopeExit.h',
            'mfbt/SegmentedVector.h',
            'mfbt/SHA1.h',
            'mfbt/SizePrintfMacros.h',
            'mfbt/Snprintf.h',
            'mfbt/SplayTree.h',
            'mfbt/TaggedAnonymousMemory.h',
            'mfbt/TemplateLib.h',
            'mfbt/ThreadLocal.h',
            'mfbt/ToString.h',
            'mfbt/Tuple.h',
            'mfbt/TypedEnumBits.h',
            'mfbt/Types.h',
            'mfbt/TypeTraits.h',
            'mfbt/UniquePtr.h',
            'mfbt/UniquePtrExtensions.h',
            'mfbt/unused.h',
            'mfbt/Variant.h',
            'mfbt/Vector.h',
            'mfbt/WeakPtr.h',
            'mfbt/WindowsVersion.h',
            'mfbt/XorShift128PlusRNG.h',
            'mozglue/misc/StackWalk.h',
            'mozglue/misc/TimeStamp.h',
            'mozglue/misc/TimeStamp_windows.h',
          ],
        },
        {
          'destination': '<(generated_include_directory)/js',
          'files': [
            'js/public/CallArgs.h',
            'js/public/CallNonGenericMethod.h',
            'js/public/CharacterEncoding.h',
            'js/public/Class.h',
            'js/public/Conversions.h',
            'js/public/Date.h',
            'js/public/Debug.h',
            'js/public/GCAPI.h',
            'js/public/GCHashTable.h',
            'js/public/HashTable.h',
            'js/public/HeapAPI.h',
            'js/public/Id.h',
            'js/public/Initialization.h',
            'js/public/LegacyIntTypes.h',
            'js/public/MemoryMetrics.h',
            'js/public/Principals.h',
            'js/public/ProfilingFrameIterator.h',
            'js/public/ProfilingStack.h',
            'js/public/Proxy.h',
            'js/public/RequiredDefines.h',
            'js/public/RootingAPI.h',
            'js/public/SliceBudget.h',
            'js/public/StructuredClone.h',
            'js/public/TraceableVector.h',
            'js/public/TraceKind.h',
            'js/public/TracingAPI.h',
            'js/public/TrackedOptimizationInfo.h',
            'js/public/TypeDecls.h',
            'js/public/UbiNode.h',
            'js/public/UbiNodeBreadthFirst.h',
            'js/public/UbiNodeCensus.h',
            'js/public/UbiNodeDominatorTree.h',
            'js/public/UbiNodePostOrder.h',
            'js/public/Utility.h',
            'js/public/Value.h',
            'js/public/Vector.h',
            'js/public/WeakMapPtr.h',
          ],
        },
        {
          'destination': '<(generated_include_directory)',
          'files': [
            'fake_generated/dist/include/js-config.h',
            'fake_generated/js/src/jsautokw.h',
            'fake_generated/js/src/selfhosted.out.h',
            'fake_generated/js/src/shell/shellmoduleloader.out.h',
          ],
        },
      ],
      'hard_dependency': 1,
    },
  ],
}
