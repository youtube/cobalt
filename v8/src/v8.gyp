# Copyright 2012 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'include_dirs': [
      '..',
      '<(DEPTH)/v8/include',
    ],
    'defines': [
      'DISABLE_GRAPHS_STARBOARD',
      'DISABLE_UNWIND_STARBOARD',
      'DISABLE_WASM_STARBOARD',
      'DISABLE_WASM_COMPILER_ISSUE_STARBOARD',
      'NO_ARRAY_MOVE_STARBOARD',
      # Disable mitigations for executing untrusted code.
      # Disabled by default on x86 due to conflicting requirements with embedded
      # builtins. 
      # Chrome enabled this by default only on Android since it doesn't support
      # site-isolation and on simulator builds which test code generation
      # on these platforms.
      # Cobalt only has one isolate instance when the app is fully functioning in
      # production so we will just disable this feature by default everywhere.
      'DISABLE_UNTRUSTED_CODE_MITIGATIONS',
    ],
    'msvs_disabled_warnings': [4267, 4312, 4351, 4355, 4800, 4838, 4715],
    'conditions': [
      ['cobalt_config == "debug"', {
        'defines': [
          # 'DEBUG=1',
          # 'OBJECT_PRINT=1',
          'V8_ENABLE_ALLOCATION_TIMEOUT=1',
        ],
      }],
      ['host_os=="win"', {
        'compiler_flags_host': ['/wd4267', '/wd4312', '/wd4351', '/wd4355', '/wd4800', '/wd4838', '/wd4715', '/EHsc'],
      }],
      ['host_os=="mac"', {
        'cflags': ['-std=c++11',],
        'cflags_host': ['-std=c++11',],
      }],
      ['v8_enable_embedded_builtins==1', {
        'defines': [
          'V8_EMBEDDED_BUILTINS',
        ],
        'direct_dependent_settings': {
          'defines': [
            'V8_EMBEDDED_BUILTINS',
          ],
        },
      }],
    ],
  },
  'variables': {
    'v8_code': 1,
    'v8_random_seed%': 314159265,
    'v8_vector_stores%': 0,
    'v8_embed_script%': "",
    'host_executable_suffix%': '<(EXECUTABLE_SUFFIX)',
    'v8_os_page_size%': 0,
    'generate_bytecode_output_root': '<(SHARED_INTERMEDIATE_DIR)/generate-bytecode-output-root',
    'generate_bytecode_builtins_list_output': '<(generate_bytecode_output_root)/builtins-generated/bytecodes-builtins-list.h',

    'v8_use_snapshot': 1,
    'v8_optimized_debug': 0,
    'v8_use_external_startup_data': 0,
    'v8_enable_i18n_support': 0,
    # Embedded builtins allow V8 to share built-in codes across isolates.
    'v8_enable_embedded_builtins': '<(cobalt_v8_enable_embedded_builtins)',
    'v8_enable_snapshot_code_comments': 0,
    # Enable native counters from the snapshot (impacts performance, sets
    # -dV8_SNAPSHOT_NATIVE_CODE_COUNTERS).
    # This option will generate extra code in the snapshot to increment counters,
    # as per the --native-code-counters flag.
    'v8_enable_snapshot_native_code_counters%': 0,
    # Enable code-generation-time checking of types in the CodeStubAssembler.
    'v8_enable_verify_csa%': 0,
    # Enable pointer compression (sets -dV8_COMPRESS_POINTERS).
    'v8_enable_pointer_compression%': 0,
    'v8_enable_31bit_smis_on_64bit_arch%': 0,
    # Sets -dOBJECT_PRINT.
    'v8_enable_object_print%': 0,
    # Sets -dV8_TRACE_MAPS.
    'v8_enable_trace_maps%': 0,
    # Sets -dV8_ENABLE_CHECKS.
    'v8_enable_v8_checks%': 0,
    # Sets -dV8_TRACE_IGNITION.
    'v8_enable_trace_ignition%': 0,
    # Sets -dV8_TRACE_FEEDBACK_UPDATES.
    'v8_enable_trace_feedback_updates%': 0,
    # Sets -dV8_CONCURRENT_MARKING
    'v8_enable_concurrent_marking%': 1,
    # Enables various testing features.
    'v8_enable_test_features%': 0,
    'is_component_build': 0,
    'v8_current_cpu': '<(v8_target_arch)',
    'is_posix': 0,
    'is_fuchsia': 0,
    'is_android': 0,
    'is_win': 0,
    'is_linux': 0,
    'is_starboard': 1,
    'is_asan': 0,
    'v8_use_perfetto': 0,
    'v8_use_siphash': 0,
    'v8_enable_lite_mode': 0,
    'want_separate_host_toolset': 1,

    'torque_files': [
      "<(DEPTH)/v8/src/builtins/arguments.tq",
      "<(DEPTH)/v8/src/builtins/array-copywithin.tq",
      "<(DEPTH)/v8/src/builtins/array-every.tq",
      "<(DEPTH)/v8/src/builtins/array-filter.tq",
      "<(DEPTH)/v8/src/builtins/array-find.tq",
      "<(DEPTH)/v8/src/builtins/array-findindex.tq",
      "<(DEPTH)/v8/src/builtins/array-foreach.tq",
      "<(DEPTH)/v8/src/builtins/array-join.tq",
      "<(DEPTH)/v8/src/builtins/array-lastindexof.tq",
      "<(DEPTH)/v8/src/builtins/array-map.tq",
      "<(DEPTH)/v8/src/builtins/array-of.tq",
      "<(DEPTH)/v8/src/builtins/array-reduce-right.tq",
      "<(DEPTH)/v8/src/builtins/array-reduce.tq",
      "<(DEPTH)/v8/src/builtins/array-reverse.tq",
      "<(DEPTH)/v8/src/builtins/array-shift.tq",
      "<(DEPTH)/v8/src/builtins/array-slice.tq",
      "<(DEPTH)/v8/src/builtins/array-some.tq",
      "<(DEPTH)/v8/src/builtins/array-splice.tq",
      "<(DEPTH)/v8/src/builtins/array-unshift.tq",
      "<(DEPTH)/v8/src/builtins/array.tq",
      "<(DEPTH)/v8/src/builtins/base.tq",
      "<(DEPTH)/v8/src/builtins/bigint.tq",
      "<(DEPTH)/v8/src/builtins/boolean.tq",
      "<(DEPTH)/v8/src/builtins/collections.tq",
      "<(DEPTH)/v8/src/builtins/data-view.tq",
      "<(DEPTH)/v8/src/builtins/extras-utils.tq",
      "<(DEPTH)/v8/src/builtins/frames.tq",
      "<(DEPTH)/v8/src/builtins/growable-fixed-array.tq",
      "<(DEPTH)/v8/src/builtins/internal-coverage.tq",
      "<(DEPTH)/v8/src/builtins/iterator.tq",
      "<(DEPTH)/v8/src/builtins/math.tq",
      "<(DEPTH)/v8/src/builtins/object-fromentries.tq",
      "<(DEPTH)/v8/src/builtins/object.tq",
      "<(DEPTH)/v8/src/builtins/proxy-constructor.tq",
      "<(DEPTH)/v8/src/builtins/proxy-delete-property.tq",
      "<(DEPTH)/v8/src/builtins/proxy-get-property.tq",
      "<(DEPTH)/v8/src/builtins/proxy-get-prototype-of.tq",
      "<(DEPTH)/v8/src/builtins/proxy-has-property.tq",
      "<(DEPTH)/v8/src/builtins/proxy-is-extensible.tq",
      "<(DEPTH)/v8/src/builtins/proxy-prevent-extensions.tq",
      "<(DEPTH)/v8/src/builtins/proxy-revocable.tq",
      "<(DEPTH)/v8/src/builtins/proxy-revoke.tq",
      "<(DEPTH)/v8/src/builtins/proxy-set-property.tq",
      "<(DEPTH)/v8/src/builtins/proxy-set-prototype-of.tq",
      "<(DEPTH)/v8/src/builtins/proxy.tq",
      "<(DEPTH)/v8/src/builtins/reflect.tq",
      "<(DEPTH)/v8/src/builtins/regexp-replace.tq",
      "<(DEPTH)/v8/src/builtins/regexp.tq",
      "<(DEPTH)/v8/src/builtins/string.tq",
      "<(DEPTH)/v8/src/builtins/string-endswith.tq",
      "<(DEPTH)/v8/src/builtins/string-html.tq",
      "<(DEPTH)/v8/src/builtins/string-iterator.tq",
      "<(DEPTH)/v8/src/builtins/string-repeat.tq",
      "<(DEPTH)/v8/src/builtins/string-slice.tq",
      "<(DEPTH)/v8/src/builtins/string-startswith.tq",
      "<(DEPTH)/v8/src/builtins/string-substring.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-createtypedarray.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-every.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-filter.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-find.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-findindex.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-foreach.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-reduce.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-reduceright.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-slice.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-some.tq",
      "<(DEPTH)/v8/src/builtins/typed-array-subarray.tq",
      "<(DEPTH)/v8/src/builtins/typed-array.tq",
      "<(DEPTH)/v8/third_party/v8/builtins/array-sort.tq",
      "<(DEPTH)/v8/test/torque/test-torque.tq",
    ],
    'torque_output_root': '<(SHARED_INTERMEDIATE_DIR)/torque-output-root',
    'torque_files_replaced': ['<!@pymod_do_main(v8.gypfiles.ForEachReplace ".tq" "-tq-csa" <@(torque_files))'],
    'torque_outputs': ['<!@pymod_do_main(v8.gypfiles.ForEachFormat "<(torque_output_root)/torque-generated/%s.cc" <@(torque_files_replaced))'],
    'torque_outputs+': ['<!@pymod_do_main(v8.gypfiles.ForEachFormat "<(torque_output_root)/torque-generated/%s.h" <@(torque_files_replaced))'],
    'v8_compiler_sources': ['<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "v8_compiler_sources = ")'],

    'conditions': [
      ['v8_enable_i18n_support', {
        'torque_files': [
          "<(DEPTH)/v8/src/objects/intl-objects.tq",
        ]
      }],
    ],
  },
  'includes': ["../gypfiles/toolchain.gypi", "../gypfiles/features.gypi", "../gypfiles/v8_external_snapshot.gypi"],
  'targets': [
    {
      'target_name': 'run_torque',
      'type': 'none',
      'toolsets': ['target'],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(torque_output_root)',
        ],
      },
      'actions': [
        {
          'action_name': 'run_torque_action',
          'inputs': [  # Order matters.
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)torque<(host_executable_suffix)',
            '<@(torque_files)',
          ],
          'outputs': [
            '<(torque_output_root)/torque-generated/builtin-definitions-tq.h',
            '<(torque_output_root)/torque-generated/field-offsets-tq.h',
            '<(torque_output_root)/torque-generated/class-verifiers-tq.cc',
            '<(torque_output_root)/torque-generated/class-verifiers-tq.h',
            '<(torque_output_root)/torque-generated/objects-printer-tq.cc',
            '<(torque_output_root)/torque-generated/class-definitions-tq.cc',
            '<(torque_output_root)/torque-generated/class-definitions-tq-inl.h',
            '<(torque_output_root)/torque-generated/class-definitions-tq.h',
            '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.cc',
            '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.h',
            '<(torque_output_root)/torque-generated/csa-types-tq.h',
            '<(torque_output_root)/torque-generated/instance-types-tq.h',
            '<@(torque_outputs)',
          ],
          'action': [
            '<@(_inputs)',
            '-o', '<(torque_output_root)/torque-generated',
            '-v8-root', '<(DEPTH)/v8'
          ],
        },
      ],
    },  # run_torque
    {
      'target_name': 'v8_maybe_icu',
      'type': 'none',
      'hard_dependency': 1,
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_enable_i18n_support', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
          'export_dependent_settings': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
        }],
      ],
    },  # v8_maybe_icu
    {
      'target_name': 'torque_generated_initializers',
      'type': 'none',
      'hard_dependency': 1,
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.cc',
          '<(torque_output_root)/torque-generated/exported-macros-assembler-tq.h',
          '<(torque_output_root)/torque-generated/csa-types-tq.h',
          '<@(torque_outputs)',
        ],
      }
    },  # torque_generated_initializers
    {
      'target_name': 'torque_generated_definitions',
      'type': 'none',
      'hard_dependency': 1,
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(torque_output_root)/torque-generated/class-definitions-tq.cc',
          '<(torque_output_root)/torque-generated/class-verifiers-tq.cc',
          '<(torque_output_root)/torque-generated/class-verifiers-tq.h',
          '<(torque_output_root)/torque-generated/objects-printer-tq.cc',
        ],
        'include_dirs': [
          '<(torque_output_root)',
        ],
      },
    },  # torque_generated_definitions
    {
      'target_name': 'generate_bytecode_builtins_list',
      'type': 'none',
      'hard_dependency': 1,
      # 'conditions': [
      #   ['want_separate_host_toolset', {
      #     'dependencies': ['bytecode_builtins_list_generator#host'],
      #     'toolsets': ['host', 'target'],
      #   }, {
      #     'dependencies': ['bytecode_builtins_list_generator'],
      #   }],
      # ],
      'direct_dependent_settings': {
        'sources': [
          '<(generate_bytecode_builtins_list_output)',
        ],
        'include_dirs': [
          '<(generate_bytecode_output_root)',
          '<(torque_output_root)',
        ],
      },
      'actions': [
        {
          'action_name': 'generate_bytecode_builtins_list_action',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)bytecode_builtins_list_generator<(host_executable_suffix)',
          ],
          'outputs': [
            '<(generate_bytecode_builtins_list_output)',
          ],
          'action': [
            'python',
            '<(DEPTH)/v8/tools/run.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },  # generate_bytecode_builtins_list

    {
      # This rule delegates to either v8_snapshot, v8_nosnapshot, or
      # v8_external_snapshot, depending on the current variables.
      # The intention is to make the 'calling' rules a bit simpler.
      'target_name': 'v8_maybe_snapshot',
      'type': 'none',
      'toolsets': ['target'],
      'hard_dependency': 1,
      'conditions': [
        ['v8_use_snapshot!=1', {
          # The dependency on v8_base should come from a transitive
          # dependency however the Android toolchain requires libv8_base.a
          # to appear before libv8_snapshot.a so it's listed explicitly.
          'dependencies': ['v8_base', 'v8_init', 'v8_nosnapshot'],
        }],
        ['v8_use_snapshot==1 and v8_use_external_startup_data==0', {
          # The dependency on v8_base should come from a transitive
          # dependency however the Android toolchain requires libv8_base.a
          # to appear before libv8_snapshot.a so it's listed explicitly.
          'dependencies': ['v8_base', 'v8_snapshot'],
        }],
        ['v8_use_snapshot==1 and v8_use_external_startup_data==1 and want_separate_host_toolset==0', {
          'dependencies': ['v8_base', 'v8_external_snapshot'],
        }],
        ['v8_use_snapshot==1 and v8_use_external_startup_data==1 and want_separate_host_toolset==1', {
          'dependencies': ['v8_base', 'v8_external_snapshot'],
        }],
      ]
    },  # v8_maybe_snapshot
    {
      'target_name': 'v8_init',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_initializers',
        'v8_maybe_icu',
      ],
      'sources': [
        ### gcmole(all) ###
        '<(DEPTH)/v8/src/init/setup-isolate-full.cc',

        # '<(generate_bytecode_builtins_list_output)',
      ],
    },  # v8_init
    {
      'target_name': 'v8_initializers',
      'type': 'static_library',
      'dependencies': [
        'torque_generated_initializers',
      ],
      'include_dirs': [
        '<(torque_output_root)',
        '<(generate_bytecode_output_root)',
      ],
      'sources': [
        '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_initializers.*?sources = ")',

        '<@(torque_outputs)',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_target_arch=="x86"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/ia32/builtins-ia32.cc',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/x64/builtins-x64.cc',
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/arm/builtins-arm.cc',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/arm64/builtins-arm64.cc',
          ],
        }],
        ['v8_target_arch=="mips" or v8_target_arch=="mipsel"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/mips/builtins-mips.cc',
          ],
        }],
        ['v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/mips64/builtins-mips64.cc',
          ],
        }],
        ['v8_target_arch=="ppc" or v8_target_arch=="ppc64"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/ppc/builtins-ppc.cc',
          ],
        }],
        ['v8_target_arch=="s390x"', {
          'sources': [
            '<(DEPTH)/v8/src/builtins/s390/builtins-s390.cc',
          ],
        }],
        ['v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
        }, {
           'sources!': [
             '<(DEPTH)/v8/src/builtins/builtins-intl-gen.cc',
           ],
         }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
        }],
      ],
    },  # v8_initializers
    {
      'target_name': 'v8_snapshot',
      'type': 'static_library',
      'toolsets': ['target'],
      'conditions': [
        ['want_separate_host_toolset', {
          'dependencies': [
            'generate_bytecode_builtins_list#target',
            'run_torque#target',
            'mksnapshot#host',
            'v8_maybe_icu',
            # [GYP] added explicitly, instead of inheriting from the other deps
            'v8_base_without_compiler',
            'v8_compiler_for_mksnapshot',
            'v8_initializers',
            'v8_libplatform',
          ]
        }, {
          'dependencies': [
            'generate_bytecode_builtins_list#target',
            'run_torque#target',
            'mksnapshot',
            'v8_maybe_icu',
            # [GYP] added explicitly, instead of inheriting from the other deps
            'v8_base_without_compiler',
            'v8_compiler_for_mksnapshot',
            'v8_initializers',
            'v8_libplatform',
          ]
        }],
        ['v8_enable_embedded_builtins==1', {
          'actions': [
            {
             'action_name': 'convert_asm_to_inline',
             'inputs': ['<(INTERMEDIATE_DIR)/embedded.S'],
             'outputs': ['<(INTERMEDIATE_DIR)/embedded.cc'],
             'action': [
               'python',
               '../tools/snapshot/asm_to_inline_asm.py',
               '>@(_inputs)',
               '>@(_outputs)',
             ],
            },
          ],
          'conditions': [
            # Xcode toolchain needs raw asm or otherwise will crash at runtime.
            ['cobalt_v8_emit_builtins_as_inline_asm==1', {
              'sources': ['<(INTERMEDIATE_DIR)/embedded.S'],
            },{
              'sources': ['<(INTERMEDIATE_DIR)/embedded.cc'],
            }]
          ],
        }],
        ['v8_use_external_startup_data', {
          'sources': ['<(INTERMEDIATE_DIR)/snapshot_blob.bin'],
        },{
          'sources': ['<(INTERMEDIATE_DIR)/snapshot.cc'],
        }],
      ],
      'sources': [
        '<(DEPTH)/v8/src/init/setup-isolate-deserialize.cc',
        '<(DEPTH)/v8/gypfiles/extras-libraries.cc',
      ],
      'xcode_settings': {
        # V8 7.4 over macOS10.11 compatibility
        # Refs: https://github.com/nodejs/node/pull/26685
        'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
      },
      'actions': [
        {
          'action_name': 'run_mksnapshot',
          'message': 'generating: >@(_outputs)',
          'variables': {
            'mksnapshot_flags': [
              '--turbo_instruction_scheduling',
              # In cross builds, the snapshot may be generated for both the host and
              # target toolchains.  The same host binary is used to generate both, so
              # mksnapshot needs to know which target OS to use at runtime.  It's weird,
              # but the target OS is really <(OS).
              '--target_os=<(OS)',
            ],
          },
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(host_executable_suffix)',
          ],
          'conditions': [
            ['v8_target_arch=="x86"', {
              'variables': {
                'mksnapshot_flags': ['--target_arch=ia32'],
              },
            }, {
              'variables': {
                'mksnapshot_flags': ['--target_arch=<(v8_target_arch)'],
              },
            }],
            ['v8_enable_embedded_builtins==1', {
              # In this case we use `embedded_variant "Default"`
              # and `suffix = ''` for the template `embedded${suffix}.S`.
              'outputs': ['<(INTERMEDIATE_DIR)/embedded.S'],
              'variables': {
                'mksnapshot_flags': [
                  '--embedded_variant', 'Default',
                  '--embedded_src', '<(INTERMEDIATE_DIR)/embedded.S',
                ],
              },
            }, {
               #'outputs': ['<(DEPTH)/v8/src/snapshot/embedded/embedded-empty.cc']
            }],
            ['v8_random_seed', {
              'variables': {
                'mksnapshot_flags': ['--random-seed', '<(v8_random_seed)'],
              },
            }],
            ['v8_os_page_size', {
              'variables': {
                'mksnapshot_flags': ['--v8_os_page_size', '<(v8_os_page_size)'],
              },
            }],
            ['v8_use_external_startup_data', {
              'outputs': ['<(INTERMEDIATE_DIR)/snapshot_blob.bin', ],
              'variables': {
                'mksnapshot_flags': ['--startup_blob', '<(INTERMEDIATE_DIR)/snapshot_blob.bin', ],
              },
            }, {
               'outputs': ["<(INTERMEDIATE_DIR)/snapshot.cc"],
               'variables': {
                 'mksnapshot_flags': ['--startup_src', '<(INTERMEDIATE_DIR)/snapshot.cc', ],
               },
             }],
            ['v8_embed_script != ""', {
              'inputs': ['<(v8_embed_script)'],
              'variables': {
                'mksnapshot_flags': ['<(v8_embed_script)'],
              },
            }],
            ['v8_enable_snapshot_code_comments', {
              'variables': {
                'mksnapshot_flags': ['--code-comments'],
              },
            }],
            ['v8_enable_snapshot_native_code_counters', {
              'variables': {
                'mksnapshot_flags': ['--native-code-counters'],
              },
            }, {
               # --native-code-counters is the default in debug mode so make sure we can
               # unset it.
               'variables': {
                 'mksnapshot_flags': ['--no-native-code-counters'],
               },
             }],
          ],
          'action': [
            '>@(_inputs)',
            '>@(mksnapshot_flags)',
          ],
        },
      ],
    },  # v8_snapshot
    {
      'target_name': 'v8_nosnapshot',
      'type': 'static_library',
      'dependencies': [
        # 'js2c_extras',  # Disabled for Node.js
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'sources': [
        '<(DEPTH)/v8/gypfiles/extras-libraries.cc',
        '<(DEPTH)/v8/src/snapshot/embedded/embedded-empty.cc',
        '<(DEPTH)/v8/src/snapshot/snapshot-empty.cc',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['component=="shared_library"', {
          'defines': [
            'BUILDING_V8_SHARED',
          ],
        }],
      ]
    },  # v8_nosnapshot
    {
      'target_name': 'v8_version',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(DEPTH)/v8/include/v8-value-serializer-version.h',
          '<(DEPTH)/v8/include/v8-version-string.h',
          '<(DEPTH)/v8/include/v8-version.h',
        ],
      },
    },  # v8_version
    {
      'target_name': 'v8_headers',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_version',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(DEPTH)/v8/include/v8-internal.h',
          '<(DEPTH)/v8/include/v8.h',
          '<(DEPTH)/v8/include/v8config.h',

          # The following headers cannot be platform-specific. The include validation
          # of `gn gen $dir --check` requires all header files to be available on all
          # platforms.
          '<(DEPTH)/v8/include/v8-wasm-trap-handler-posix.h',
          '<(DEPTH)/v8/include/v8-wasm-trap-handler-win.h',
        ],
      },
    },  # v8_headers
    {
      'target_name': 'v8_shared_internal_headers',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_headers',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(DEPTH)/v8/src/common/globals.h',
        ],
      },
    },  # v8_shared_internal_headers
    {
      'target_name': 'v8_compiler_opt',
      'type': 'static_library',
      'dependencies': [
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'sources': ['<@(v8_compiler_sources)'],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
        }],
      ],
    },  # v8_compiler_opt
    {
      'target_name': 'v8_compiler',
      'type': 'static_library',
      'dependencies': [
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'sources': ['<@(v8_compiler_sources)'],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
        }],
      ],
    },  # v8_compiler
    {
      'target_name': 'v8_compiler_for_mksnapshot',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['is_component_build and not v8_optimized_debug and v8_enable_fast_mksnapshot', {
          'dependencies': [
            'v8_compiler_opt',
          ],
          'export_dependent_settings': [
            'v8_compiler_opt',
          ],
        }, {
           'dependencies': [
             'v8_compiler',
           ],
           'export_dependent_settings': [
             'v8_compiler',
           ],
         }],
      ],
    },  # v8_compiler_for_mksnapshot
    {
      'target_name': 'v8_base_without_compiler',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/v8/src/inspector/inspector.gyp:protocol_generated_sources#target',
        # Code generators that only need to be build for the host.
        'torque_generated_definitions#target',
        'v8_headers',
        'v8_libbase',
        'v8_libsampler',
        'v8_shared_internal_headers',
        'v8_version',
        # BUILD.gn public_deps
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'includes': ['<(DEPTH)/v8/src/inspector/inspector.gypi'],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(generate_bytecode_output_root)',
          '<(torque_output_root)',
        ],
      },
      'sources': [
        # "//base/trace_event/common/trace_event_common.h",

        ### gcmole(all) ###
        '<(generate_bytecode_builtins_list_output)',

        '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?sources = ")',

        '<@(inspector_all_sources)',
      ],
      'conditions': [
        ['v8_enable_embedded_builtins!=1', {
          'sources': ['<(DEPTH)/v8/src/snapshot/embedded/embedded-empty.cc'],
        }],
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_target_arch=="x86"', {
          'sources': [  ### gcmole(arch:ia32) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"x86.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [  ### gcmole(arch:x64) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"x64.*?sources \+= ")',
          ],
          'conditions': [
            # iOS Xcode simulator builds run on an x64 target. iOS and macOS are both
            # based on Darwin and thus POSIX-compliant to a similar degree.
            ['OS=="linux" or OS=="mac" or OS=="ios"', {
              'sources': [
                '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?is_linux.*?sources \+= ")',
              ],
            }],
            ['OS=="win"', {
              'sources': [
                '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?is_win.*?sources \+= ")',
              ],
            }],
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [  ### gcmole(arch:arm) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"arm.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [  ### gcmole(arch:arm64) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"arm64.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="mips" or v8_target_arch=="mipsel"', {
          'sources': [  ### gcmole(arch:mipsel) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"mips.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
          'sources': [  ### gcmole(arch:mips64el) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"mips64.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="ppc" or v8_target_arch=="ppc64"', {
          'sources': [  ### gcmole(arch:ppc) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"ppc.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="s390x"', {
          'sources': [  ### gcmole(arch:s390) ###
            '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"s390.*?sources \+= ")',
          ],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(DEPTH)/v8/../../tools/msvs/pch/v8_pch.cc',
          'sources': [
            '<(_msvs_precompiled_header)',
            '<(_msvs_precompiled_source)',
          ],
          # This will prevent V8's .cc files conflicting with the inspector's
          # .cpp files in the same shard.
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ObjectFile': '$(IntDir)%(Extension)\\',
            },
          },
        }],
        ['is_starboard', {
          'target_conditions': [
            ['_toolset=="host"', {
              # if _toolset="host", we use the native host build which does not
              # involve Starboard at all.
              'conditions': [
                ['host_os=="win"', {
                  'sources': [
                    'diagnostics/unwinding-info-win64.cc',
                    'diagnostics/unwinding-info-win64.h',
                    'trap-handler/handler-inside-win.cc',
                    'trap-handler/handler-inside-win.h',
                    'trap-handler/handler-outside-win.cc',
                  ],
                }],
                ['host_os=="linux" or host_os=="mac"', {
                  'conditions': [
                    ['v8_target_arch=="x64"', {
                      'sources': [
                        "trap-handler/handler-inside-posix.cc",
                        "trap-handler/handler-inside-posix.h",
                        "trap-handler/handler-outside-posix.cc",
                      ],
                    }],
                  ],
                }],
              ],
            }],
          ],
        }],
        ['component=="shared_library"', {
          'defines': [
            'BUILDING_V8_SHARED',
          ],
        }],
        ['v8_enable_i18n_support', {
          'dependencies': [
            'run_gen-regexp-special-case#target',
          ],
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/src/regexp/special-case.cc',
          ],
          'conditions': [
            ['icu_use_data_file_flag', {
              'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_FILE'],
            }, {
               'conditions': [
                 ['OS=="win"', {
                   'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_SHARED'],
                 }, {
                    'defines': ['ICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC'],
                  }],
               ],
             }],
            ['OS=="win"', {
              'dependencies': [
                '<(icu_gyp_path):icudata#target',
              ],
            }],
          ],
        }, {  # v8_enable_i18n_support==0
           'sources!': [
             '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_enable_i18n_support.*?sources -= ")',
           ],
         }],
        ['v8_postmortem_support', {
          'dependencies': ['postmortem-metadata#target'],
        }],
        # Platforms that don't have Compare-And-Swap (CAS) support need to link atomic library
        # to implement atomic memory access
        ['v8_current_cpu in ["mips", "mipsel", "mips64", "mips64el", "ppc", "ppc64", "s390x"]', {
          'link_settings': {
            'libraries': ['-latomic', ],
          },
        }],
      ],
    },  # v8_base_without_compiler
    {
      'target_name': 'v8_base',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_base_without_compiler',
        'v8_compiler',
      ],
    },  # v8_base
    {
      'target_name': 'torque_base',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'sources': [
        '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"torque_base.*?sources = ")',
      ],
      'dependencies': [
        'v8_shared_internal_headers',
        'v8_libbase',
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'cflags_cc!': ['-fno-exceptions'],
      'cflags_cc': ['-fexceptions'],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',  # -fexceptions
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
      },
    },  # torque_base
    {
      'target_name': 'torque_ls_base',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'sources': [
        '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"torque_ls_base.*?sources = ")',
      ],
      'dependencies': [
        'torque_base',
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'cflags_cc!': ['-fno-exceptions'],
      'cflags_cc': ['-fexceptions'],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',  # -fexceptions
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
      },
    },  # torque_ls_base
    {
      'target_name': 'v8_libbase',
      'type': 'static_library',
      'sources': [
        '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"v8_libbase.*?sources = ")',
      ],

      'dependencies': [
        'v8_headers',
      ],

      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['is_component_build', {
          'defines': ["BUILDING_V8_BASE_SHARED"],
        }],
        ['is_posix or is_fuchsia', {
          'sources': [
            '<(DEPTH)/v8/src/base/platform/platform-posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix.h',
          ],
          'conditions': [
            ['OS != "aix" and OS != "solaris"', {
              'sources': [
                '<(DEPTH)/v8/src/base/platform/platform-posix-time.cc',
                '<(DEPTH)/v8/src/base/platform/platform-posix-time.h',
              ],
            }],
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-linux.cc',
          ],
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lrt'
            ],
          },
        }],
        ['OS=="aix"', {
          'variables': {
            # Used to differentiate `AIX` and `OS400`(IBM i).
            'aix_variant_name': '<!(uname -s)',
          },
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-aix.cc',
          ],
          'conditions': [
            [ '"<(aix_variant_name)"=="AIX"', { # It is `AIX`
              'link_settings': {
                'libraries': [
                  '-ldl',
                  '-lrt'
                ],
              },
            }],
          ],
        }],
        ['is_android', {
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_android.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.h',
          ],
          'conditions': [
            ['_toolset=="host"', {
              'link_settings': {
                'libraries': [
                  '-ldl',
                  '-lrt'
                ]
              },
              'conditions': [
                ['_toolset=="host"', {
                  'conditions': [
                    ['host_os == "mac"', {
                      'sources': [
                        '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc'
                        '<(DEPTH)/v8/src/base/platform/platform-macos.cc'
                      ]
                    }, {
                       'sources': [
                         '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc'
                         '<(DEPTH)/v8/src/base/platform/platform-linux.cc'
                       ]
                     }],
                  ],
                }, {
                   'sources': [
                     '<(DEPTH)/v8/src/base/debug/stack_trace_android.cc'
                     '<(DEPTH)/v8/src/base/platform/platform-linux.cc'
                   ]
                 }],
              ],
            }],
          ],
        }],
        ['is_starboard', {
          'target_conditions': [
            ['_toolset=="host"', {
              # if _toolset="host", we use the native host build which does not
              # involve Starboard at all.
              'target_conditions': [
                ['host_os=="linux"', {
                  'ldflags_host': [
                    '-ldl',
                    '-lrt'
                  ],
                  'sources': [
                    'base/debug/stack_trace_posix.cc',
                    'base/platform/platform-linux.cc',
                    'base/platform/platform-posix.h',
                    'base/platform/platform-posix.cc',
                    'base/platform/platform-posix-time.h',
                    'base/platform/platform-posix-time.cc',
                  ],
                }],
                ['host_os=="win"', {
                  # Most of the following codes are copied from 'OS="win"'
                  # section.
                  'defines': [
                    '_CRT_RAND_S'  # for rand_s()
                  ],
                  'variables': {
                    'gyp_generators': '<!(echo $GYP_GENERATORS)',
                  },
                  'sources': [
                    'base/debug/stack_trace_win.cc',
                    'base/platform/platform-win32.cc',
                    'base/win32-headers.h',
                  ],
                  'include_dirs': [
                    '<(torque_output_root)',
                  ],
                  'msvs_disabled_warnings': [4351, 4355, 4800],
                }],
                ['host_os == "mac"', {
                  'sources': [
                    'base/debug/stack_trace_posix.cc',
                    'base/platform/platform-macos.cc',
                    'base/platform/platform-posix.h',
                    'base/platform/platform-posix.cc',
                    'base/platform/platform-posix-time.h',
                    'base/platform/platform-posix-time.cc',
                  ],
                }],
              ],
            }, {
              # If _toolset=="target", build with target platform's Starboard.
              'sources': [
                'base/debug/stack_trace_starboard.cc',
                'base/platform/platform-starboard.cc',
              ],
            }],
          ],
        }],
        ['is_fuchsia', {
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_fuchsia.cc',
            '<(DEPTH)/v8/src/base/platform/platform-fuchsia.cc',
          ]
        }],
        ['OS == "mac" or OS == "ios"', {
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-macos.cc',
          ]
        }],
        ['is_win', {
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_win.cc',
            '<(DEPTH)/v8/src/base/platform/platform-win32.cc',
            '<(DEPTH)/v8/src/base/win32-headers.h',
          ],

          'defines': ['_CRT_RAND_S'],  # for rand_s()
          'direct_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalDependencies': [
                  'dbghelp.lib',
                  'winmm.lib',
                  'ws2_32.lib'
                ]
              }
            },
          },
        }],
        ['target_arch == "mips" or OS == "mips64"', {
          # here just for 'BUILD.gn' sync
          # 'data': [
          #   '<(DEPTH)/v8/tools/mips_toolchain/sysroot/usr/lib/',
          #   '<(DEPTH)/v8/tools/mips_toolchain/sysroot/usr/lib/',
          # ],
        }],
        # end of conditions from 'BUILD.gn'

        # Node.js validated
        ['OS=="solaris"', {
          'link_settings': {
            'libraries': [
              '-lnsl',
              '-lrt',
            ]
          },
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-solaris.cc',
          ],
        }],

        # YMMV with the following conditions
        ['OS=="qnx"', {
          'link_settings': {
            'target_conditions': [
              ['_toolset=="host" and host_os=="linux"', {
                'libraries': [
                  '-lrt'
                ],
              }],
              ['_toolset=="target"', {
                'libraries': [
                  '-lbacktrace'
                ],
              }],
            ],
          },
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.cc',
            '<(DEPTH)/v8/src/base/qnx-math.h'
          ],
          'target_conditions': [
            ['_toolset=="host" and host_os=="linux"', {
              'sources': [
                '<(DEPTH)/v8/src/base/platform/platform-linux.cc'
              ],
            }],
            ['_toolset=="host" and host_os=="mac"', {
              'sources': [
                '<(DEPTH)/v8/src/base/platform/platform-macos.cc'
              ],
            }],
            ['_toolset=="target"', {
              'sources': [
                '<(DEPTH)/v8/src/base/platform/platform-qnx.cc'
              ],
            }],
          ],
        },
         ],
        ['OS=="freebsd"', {
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
            ]
          },
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-freebsd.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.cc',
          ],
        }
         ],
        ['OS=="openbsd"', {
          'link_settings': {
            'libraries': [
              '-L/usr/local/lib -lexecinfo',
            ]
          },
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-openbsd.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.cc',
          ],
        }
         ],
        ['OS=="netbsd"', {
          'link_settings': {
            'libraries': [
              '-L/usr/pkg/lib -Wl,-R/usr/pkg/lib -lexecinfo',
            ]
          },
          'sources': [
            '<(DEPTH)/v8/src/base/debug/stack_trace_posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-openbsd.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix.cc',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.h',
            '<(DEPTH)/v8/src/base/platform/platform-posix-time.cc',
          ],
        }
         ],
      ],
    },  # v8_libbase
    {
      'target_name': 'v8_libplatform',
      'type': 'static_library',
      'dependencies': [
        'v8_libbase',
      ],
      'sources': [
        '<(DEPTH)/v8/base/trace_event/common/trace_event_common.h',
        '<(DEPTH)/v8/include/libplatform/libplatform-export.h',
        '<(DEPTH)/v8/include/libplatform/libplatform.h',
        '<(DEPTH)/v8/include/libplatform/v8-tracing.h',
        '<(DEPTH)/v8/src/libplatform/default-foreground-task-runner.cc',
        '<(DEPTH)/v8/src/libplatform/default-foreground-task-runner.h',
        '<(DEPTH)/v8/src/libplatform/default-platform.cc',
        '<(DEPTH)/v8/src/libplatform/default-platform.h',
        '<(DEPTH)/v8/src/libplatform/default-worker-threads-task-runner.cc',
        '<(DEPTH)/v8/src/libplatform/default-worker-threads-task-runner.h',
        '<(DEPTH)/v8/src/libplatform/delayed-task-queue.cc',
        '<(DEPTH)/v8/src/libplatform/delayed-task-queue.h',
        '<(DEPTH)/v8/src/libplatform/task-queue.cc',
        '<(DEPTH)/v8/src/libplatform/task-queue.h',
        '<(DEPTH)/v8/src/libplatform/tracing/trace-buffer.cc',
        '<(DEPTH)/v8/src/libplatform/tracing/trace-buffer.h',
        '<(DEPTH)/v8/src/libplatform/tracing/trace-config.cc',
        '<(DEPTH)/v8/src/libplatform/tracing/trace-object.cc',
        '<(DEPTH)/v8/src/libplatform/tracing/trace-writer.cc',
        '<(DEPTH)/v8/src/libplatform/tracing/trace-writer.h',
        '<(DEPTH)/v8/src/libplatform/tracing/tracing-controller.cc',
        '<(DEPTH)/v8/src/libplatform/worker-thread.cc',
        '<(DEPTH)/v8/src/libplatform/worker-thread.h',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['component=="shared_library"', {
          'direct_dependent_settings': {
            'defines': ['USING_V8_PLATFORM_SHARED'],
          },
          'defines': ['BUILDING_V8_PLATFORM_SHARED'],
        }],
        ['v8_use_perfetto', {
          'sources': [
            '<(DEPTH)/v8/src/libplatform/tracing/json-trace-event-listener.cc',
            '<(DEPTH)/v8/src/libplatform/tracing/json-trace-event-listener.h',
            '<(DEPTH)/v8/src/libplatform/tracing/trace-event-listener.cc',
            '<(DEPTH)/v8/src/libplatform/tracing/trace-event-listener.h',
          ],
          'dependencies': [
            '<(DEPTH)/v8/third_party/perfetto:libperfetto',
            '<(DEPTH)/v8/third_party/perfetto/protos/perfetto/trace:lite',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/v8/include',
        ],
      },
    },  # v8_libplatform
    {
      'target_name': 'v8_libsampler',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'dependencies': [
        'v8_libbase',
      ],
      'sources': [
        '<(DEPTH)/v8/src/libsampler/sampler.cc',
        '<(DEPTH)/v8/src/libsampler/sampler.h'
      ],
    },  # v8_libsampler

    # {
    #   'target_name': 'fuzzer_support',
    #   'type': 'static_library',
    #   'conditions': [
    #     ['want_separate_host_toolset', {
    #       'toolsets': ['host', 'target'],
    #     }],
    #   ],
    #   'dependencies': [
    #     'v8',
    #     'v8_libbase',
    #     'v8_libplatform',
    #     'v8_maybe_icu',
    #   ],
    #   'sources': [
    #     "<(DEPTH)/v8/test/fuzzer/fuzzer-support.cc",
    #     "<(DEPTH)/v8/test/fuzzer/fuzzer-support.h",
    #   ],
    # },  # fuzzer_support

    # {
    #   'target_name': 'wee8',
    #   'type': 'static_library',
    #   'dependencies': [
    #     'v8_base',
    #     'v8_libbase',
    #     'v8_libplatform',
    #     'v8_libsampler',
    #     'v8_maybe_snapshot',
    #     # 'build/win:default_exe_manifest',
    #   ],
    #   'sources': [
    #     "<(DEPTH)/v8/src/wasm/c-api.cc",
    #     "<(DEPTH)/v8/third_party/wasm-api/wasm.h",
    #     "<(DEPTH)/v8/third_party/wasm-api/wasm.hh",
    #   ],
    # }, # wee8

    # ###############################################################################
    # # Executablesicu_path
    # #

    {
      'target_name': 'bytecode_builtins_list_generator',
      'type': 'executable',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'defines!': [
        'BUILDING_V8_SHARED=1',
      ],
      'dependencies': [
        "v8_libbase",
        # "build/win:default_exe_manifest",
      ],
      'sources': [
        "<(DEPTH)/v8/src/builtins/generate-bytecodes-builtins-list.cc",
        "<(DEPTH)/v8/src/interpreter/bytecode-operands.cc",
        "<(DEPTH)/v8/src/interpreter/bytecode-operands.h",
        "<(DEPTH)/v8/src/interpreter/bytecodes.cc",
        "<(DEPTH)/v8/src/interpreter/bytecodes.h",
      ],
    },  # bytecode_builtins_list_generator
    {
      'target_name': 'mksnapshot',
      'type': 'executable',
      'dependencies': [
        'v8_base_without_compiler',
        'v8_compiler_for_mksnapshot',
        'v8_init',
        'v8_libbase',
        'v8_libplatform',
        'v8_nosnapshot',
        # "build/win:default_exe_manifest",
        'v8_maybe_icu',
      ],
      'sources': [
        '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"mksnapshot.*?sources = ")',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
    },  # mksnapshot
    {
      'target_name': 'torque',
      'type': 'executable',
      'dependencies': [
        'torque_base',
        # "build/win:default_exe_manifest",
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'cflags_cc!': ['-fno-exceptions'],
      'cflags_cc': ['-fexceptions'],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',  # -fexceptions
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'dbghelp.lib',
            'winmm.lib',
            'ws2_32.lib'
          ]
        }
      },
      'sources': [
        "<(DEPTH)/v8/src/torque/torque.cc",
      ],
    },  # torque
    {
      'target_name': 'torque-language-server',
      'type': 'executable',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'dependencies': [
        'torque_base',
        'torque_ls_base',
        # "build/win:default_exe_manifest",
      ],
      'defines!': [
        '_HAS_EXCEPTIONS=0',
        'BUILDING_V8_SHARED=1',
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'RuntimeTypeInfo': 'true',
          'ExceptionHandling': 1,
        },
      },
      'sources': [
        "<(DEPTH)/v8/src/torque/ls/torque-language-server.cc",
      ],
    },  # torque-language-server
    {
      'target_name': 'gen-regexp-special-case',
      'type': 'executable',
      'dependencies': [
        'v8_libbase',
        # "build/win:default_exe_manifest",
        'v8_maybe_icu',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
      ],
      'sources': [
        "<(DEPTH)/v8/src/regexp/gen-regexp-special-case.cc",
      ],
    },  # gen-regexp-special-case
    {
      'target_name': 'run_gen-regexp-special-case',
      'type': 'none',
      # 'conditions': [
      #   ['want_separate_host_toolset', {
      #     'dependencies': ['gen-regexp-special-case#host'],
      #     'toolsets': ['host', 'target'],
      #   }, {
      #     'dependencies': ['gen-regexp-special-case'],
      #   }],
      # ],
      'actions': [
        {
          'action_name': 'run_gen-regexp-special-case_action',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)gen-regexp-special-case<(host_executable_suffix)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/src/regexp/special-case.cc',
          ],
          'action': [
            'python',
            '<(DEPTH)/v8/tools/run.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },  # run_gen-regexp-special-case

    ###############################################################################
    # Public targets
    #

    {
      'target_name': 'v8',
      'hard_dependency': 1,
      'toolsets': ['target'],
      'dependencies': [
        'v8_maybe_snapshot'
      ],
      'conditions': [
        ['component=="shared_library"', {
          'type': '<(component)',
          'sources': [
            # Note: on non-Windows we still build this file so that gyp
            # has some sources to link into the component.
            '<(DEPTH)/v8/src/utils/v8dll-main.cc',
          ],
          'defines': [
            'BUILDING_V8_SHARED',
          ],
          'direct_dependent_settings': {
            'defines': [
              'USING_V8_SHARED',
            ],
          },
          'conditions': [
            ['OS=="mac"', {
              'xcode_settings': {
                'OTHER_LDFLAGS': ['-dynamiclib', '-all_load']
              },
            }],
            ['soname_version!=""', {
              'product_extension': 'so.<(soname_version)',
            }],
          ],
        },
         {
           'type': 'none',
         }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/v8/include',
        ],
      },
      'actions': [
        {
          'action_name': 'v8_dump_build_config',
          'inputs': [
            '<(DEPTH)/v8/tools/testrunner/utils/dump_build_config_gyp.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/v8_build_config.json',
          ],
          'variables': {
            'v8_dump_build_config_args': [
              '<(PRODUCT_DIR)/v8_build_config.json',
              'dcheck_always_on=<(dcheck_always_on)',
              'is_android=<(is_android)',
              'is_asan=<(asan)',
              'is_cfi=<(cfi_vptr)',
              'is_clang=<(clang)',
              'is_component_build=<(component)',
              'is_debug=<(CONFIGURATION_NAME)',
              # Not available in gyp.
              'is_gcov_coverage=0',
              'is_msan=<(msan)',
              'is_tsan=<(tsan)',
              # Not available in gyp.
              'is_ubsan_vptr=0',
              'target_cpu=<(target_arch)',
              'v8_enable_i18n_support=<(v8_enable_i18n_support)',
              'v8_enable_verify_predictable=<(v8_enable_verify_predictable)',
              'v8_target_cpu=<(v8_target_arch)',
              'v8_use_snapshot=<(v8_use_snapshot)',
              'v8_use_siphash=<(v8_use_siphash)',
              'v8_enable_embedded_builtins=<(v8_enable_embedded_builtins)',
              'v8_enable_verify_csa=<(v8_enable_verify_csa)',
              'v8_enable_lite_mode=<(v8_enable_lite_mode)',
              'v8_enable_pointer_compression=<(v8_enable_pointer_compression)',
            ]
          },
          'conditions': [
            ['v8_target_arch=="mips" or v8_target_arch=="mipsel" \
              or v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
              'v8_dump_build_config_args': [
                'mips_arch_variant=<(mips_arch_variant)',
                'mips_use_msa=<(mips_use_msa)',
              ],
            }],
          ],
          'action': [
            'python', '<(DEPTH)/v8/tools/testrunner/utils/dump_build_config_gyp.py',
            '<@(v8_dump_build_config_args)',
          ],
        },
      ],
    },  # v8
    # missing a bunch of fuzzer targets

    ###############################################################################
    # Protobuf targets, used only when building outside of chromium.
    #

    {
      'target_name': 'postmortem-metadata',
      'type': 'none',
      # 'conditions': [
      #   ['want_separate_host_toolset', {
      #     'toolsets': ['host', 'target'],
      #   }],
      # ],
      'variables': {
        'heapobject_files': [
          '<!@pymod_do_main(v8.gypfiles.GN-scraper "<(DEPTH)/v8/BUILD.gn"  "\\"postmortem-metadata.*?sources = ")',
        ],
      },
      'actions': [
        {
          'action_name': 'gen-postmortem-metadata',
          'inputs': [
            '<(DEPTH)/v8/tools/gen-postmortem-metadata.py',
            '<@(heapobject_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/debug-support.cc',
          ],
          'action': [
            'python',
            '<(DEPTH)/v8/tools/gen-postmortem-metadata.py',
            '<@(_outputs)',
            '<@(heapobject_files)'
          ],
        },
      ],
      'direct_dependent_settings': {
        'sources': ['<(SHARED_INTERMEDIATE_DIR)/debug-support.cc', ],
      },
    },  # postmortem-metadata
  ],
}
