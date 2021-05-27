# Copyright 2012 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'V8_ROOT': '<(DEPTH)/third_party/v8',
    'v8_code': 1,
    'v8_random_seed%': 314159265,
    'v8_vector_stores%': 0,
    'v8_embed_script%': "",
    'mksnapshot_exec': '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)mksnapshot<(host_executable_suffix)',
    'v8_os_page_size%': 0,
    # Cobalt added variables

    'v8_use_snapshot': 1,
    'v8_optimized_debug': 0,
    'v8_use_external_startup_data': 0,
    'v8_enable_i18n_support': 0,
    'v8_enable_snapshot_code_comments': 0,
    # Enable native counters from the snapshot (impacts performance, sets
    # -dV8_SNAPSHOT_NATIVE_CODE_COUNTERS).
    # This option will generate extra code in the snapshot to increment counters,
    # as per the --native-code-counters flag.
    'v8_enable_snapshot_native_code_counters%': 0,
    # Enable code-generation-time checking of types in the CodeStubAssembler.
    'v8_enable_verify_csa%': 0,
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

    # New addition after 8.7 rebase
    'v8_enable_third_party_heap': 0,
    'v8_control_flow_integrity': 0,  #  qualcomm's pointer authentication
    # End of Cobalt variables

    'generate_bytecode_output_root': '<(SHARED_INTERMEDIATE_DIR)/generate-bytecode-output-root',
    'generate_bytecode_builtins_list_output': '<(generate_bytecode_output_root)/builtins-generated/bytecodes-builtins-list.h',
    'torque_files_v8_root_relative': [
      "src/builtins/aggregate-error.tq",
      "src/builtins/array-copywithin.tq",
      "src/builtins/array-every.tq",
      "src/builtins/array-filter.tq",
      "src/builtins/array-find.tq",
      "src/builtins/array-findindex.tq",
      "src/builtins/array-foreach.tq",
      "src/builtins/array-from.tq",
      "src/builtins/array-isarray.tq",
      "src/builtins/array-join.tq",
      "src/builtins/array-lastindexof.tq",
      "src/builtins/array-map.tq",
      "src/builtins/array-of.tq",
      "src/builtins/array-reduce-right.tq",
      "src/builtins/array-reduce.tq",
      "src/builtins/array-reverse.tq",
      "src/builtins/array-shift.tq",
      "src/builtins/array-slice.tq",
      "src/builtins/array-some.tq",
      "src/builtins/array-splice.tq",
      "src/builtins/array-unshift.tq",
      "src/builtins/array.tq",
      "src/builtins/arraybuffer.tq",
      "src/builtins/base.tq",
      "src/builtins/boolean.tq",
      "src/builtins/builtins-bigint.tq",
      "src/builtins/builtins-string.tq",
      "src/builtins/cast.tq",
      "src/builtins/collections.tq",
      "src/builtins/constructor.tq",
      "src/builtins/conversion.tq",
      "src/builtins/convert.tq",
      "src/builtins/console.tq",
      "src/builtins/data-view.tq",
      "src/builtins/finalization-registry.tq",
      "src/builtins/frames.tq",
      "src/builtins/frame-arguments.tq",
      "src/builtins/function.tq",
      "src/builtins/growable-fixed-array.tq",
      "src/builtins/ic-callable.tq",
      "src/builtins/ic-dynamic-map-checks.tq",
      "src/builtins/ic.tq",
      "src/builtins/internal-coverage.tq",
      "src/builtins/internal.tq",
      "src/builtins/iterator.tq",
      "src/builtins/math.tq",
      "src/builtins/number.tq",
      "src/builtins/object-fromentries.tq",
      "src/builtins/object.tq",
      "src/builtins/promise-abstract-operations.tq",
      "src/builtins/promise-all.tq",
      "src/builtins/promise-all-element-closure.tq",
      "src/builtins/promise-any.tq",
      "src/builtins/promise-constructor.tq",
      "src/builtins/promise-finally.tq",
      "src/builtins/promise-misc.tq",
      "src/builtins/promise-race.tq",
      "src/builtins/promise-reaction-job.tq",
      "src/builtins/promise-resolve.tq",
      "src/builtins/promise-then.tq",
      "src/builtins/promise-jobs.tq",
      "src/builtins/proxy-constructor.tq",
      "src/builtins/proxy-delete-property.tq",
      "src/builtins/proxy-get-property.tq",
      "src/builtins/proxy-get-prototype-of.tq",
      "src/builtins/proxy-has-property.tq",
      "src/builtins/proxy-is-extensible.tq",
      "src/builtins/proxy-prevent-extensions.tq",
      "src/builtins/proxy-revocable.tq",
      "src/builtins/proxy-revoke.tq",
      "src/builtins/proxy-set-property.tq",
      "src/builtins/proxy-set-prototype-of.tq",
      "src/builtins/proxy.tq",
      "src/builtins/reflect.tq",
      "src/builtins/regexp-exec.tq",
      "src/builtins/regexp-match-all.tq",
      "src/builtins/regexp-match.tq",
      "src/builtins/regexp-replace.tq",
      "src/builtins/regexp-search.tq",
      "src/builtins/regexp-source.tq",
      "src/builtins/regexp-split.tq",
      "src/builtins/regexp-test.tq",
      "src/builtins/regexp.tq",
      "src/builtins/string-endswith.tq",
      "src/builtins/string-html.tq",
      "src/builtins/string-iterator.tq",
      "src/builtins/string-pad.tq",
      "src/builtins/string-repeat.tq",
      "src/builtins/string-replaceall.tq",
      "src/builtins/string-slice.tq",
      "src/builtins/string-startswith.tq",
      "src/builtins/string-substr.tq",
      "src/builtins/string-substring.tq",
      "src/builtins/string-trim.tq",
      "src/builtins/symbol.tq",
      "src/builtins/torque-internal.tq",
      "src/builtins/typed-array-createtypedarray.tq",
      "src/builtins/typed-array-every.tq",
      "src/builtins/typed-array-entries.tq",
      "src/builtins/typed-array-filter.tq",
      "src/builtins/typed-array-find.tq",
      "src/builtins/typed-array-findindex.tq",
      "src/builtins/typed-array-foreach.tq",
      "src/builtins/typed-array-from.tq",
      "src/builtins/typed-array-keys.tq",
      "src/builtins/typed-array-of.tq",
      "src/builtins/typed-array-reduce.tq",
      "src/builtins/typed-array-reduceright.tq",
      "src/builtins/typed-array-set.tq",
      "src/builtins/typed-array-slice.tq",
      "src/builtins/typed-array-some.tq",
      "src/builtins/typed-array-sort.tq",
      "src/builtins/typed-array-subarray.tq",
      "src/builtins/typed-array-values.tq",
      "src/builtins/typed-array.tq",
      "src/builtins/wasm.tq",
      "src/builtins/weak-ref.tq",
      "src/ic/handler-configuration.tq",
      "src/objects/allocation-site.tq",
      "src/objects/api-callbacks.tq",
      "src/objects/arguments.tq",
      "src/objects/bigint.tq",
      "src/objects/cell.tq",
      "src/objects/code.tq",
      "src/objects/contexts.tq",
      "src/objects/data-handler.tq",
      "src/objects/debug-objects.tq",
      "src/objects/descriptor-array.tq",
      "src/objects/embedder-data-array.tq",
      "src/objects/feedback-cell.tq",
      "src/objects/feedback-vector.tq",
      "src/objects/fixed-array.tq",
      "src/objects/foreign.tq",
      "src/objects/free-space.tq",
      "src/objects/heap-number.tq",
      "src/objects/heap-object.tq",
      "src/objects/js-array-buffer.tq",
      "src/objects/js-array.tq",
      "src/objects/js-collection-iterator.tq",
      "src/objects/js-collection.tq",
      "src/objects/js-function.tq",
      "src/objects/js-generator.tq",
      "src/objects/js-objects.tq",
      "src/objects/js-promise.tq",
      "src/objects/js-proxy.tq",
      "src/objects/js-regexp-string-iterator.tq",
      "src/objects/js-regexp.tq",
      "src/objects/js-weak-refs.tq",
      "src/objects/literal-objects.tq",
      "src/objects/map.tq",
      "src/objects/microtask.tq",
      "src/objects/module.tq",
      "src/objects/name.tq",
      "src/objects/oddball.tq",
      "src/objects/ordered-hash-table.tq",
      "src/objects/primitive-heap-object.tq",
      "src/objects/promise.tq",
      "src/objects/property-array.tq",
      "src/objects/property-cell.tq",
      "src/objects/property-descriptor-object.tq",
      "src/objects/prototype-info.tq",
      "src/objects/regexp-match-info.tq",
      "src/objects/scope-info.tq",
      "src/objects/script.tq",
      "src/objects/shared-function-info.tq",
      "src/objects/source-text-module.tq",
      "src/objects/stack-frame-info.tq",
      "src/objects/string.tq",
      "src/objects/struct.tq",
      "src/objects/synthetic-module.tq",
      "src/objects/template-objects.tq",
      "src/objects/templates.tq",
      "src/objects/torque-defined-classes.tq",
      "src/wasm/wasm-objects.tq",
      "test/torque/test-torque.tq",
      "third_party/v8/builtins/array-sort.tq",
    ],
    'torque_files': ['<!@pymod_do_main(third_party.v8.gypfiles.ForEachFormat "<(V8_ROOT)/%s" <@(torque_files_v8_root_relative))'],
    'torque_output_root': '<(SHARED_INTERMEDIATE_DIR)/torque-output-root',
    'torque_files_replaced': ['<!@pymod_do_main(third_party.v8.gypfiles.ForEachReplace ".tq" "-tq-csa" <@(torque_files_v8_root_relative))'],
    'torque_outputs': ['<!@pymod_do_main(third_party.v8.gypfiles.ForEachFormat "<(torque_output_root)/torque-generated/%s.cc" <@(torque_files_replaced))'],
    'torque_outputs+': ['<!@pymod_do_main(third_party.v8.gypfiles.ForEachFormat "<(torque_output_root)/torque-generated/%s.h" <@(torque_files_replaced))'],
    'v8_compiler_sources': ['<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "v8_compiler_sources = ")'],

    'conditions': [
      ['(target_arch=="arm64" or target_arch=="x64") and disable_v8_pointer_compression==0', {
         'v8_enable_pointer_compression': 1,
      }, {
         'v8_enable_pointer_compression': 0,
      }],
      ['v8_enable_i18n_support', {
        'torque_files_v8_root_relative': [
          "src/objects/intl-objects.tq",
          "src/objects/js-break-iterator.tq",
          "src/objects/js-collator.tq",
          "src/objects/js-date-time-format.tq",
          "src/objects/js-display-names.tq",
          "src/objects/js-list-format.tq",
          "src/objects/js-locale.tq",
          "src/objects/js-number-format.tq",
          "src/objects/js-plural-rules.tq",
          "src/objects/js-relative-time-format.tq",
          "src/objects/js-segment-iterator.tq",
          "src/objects/js-segmenter.tq",
          "src/objects/js-segments.tq",
        ]
      }]
    ],
  },
  'includes': ['toolchain.gypi', 'features.gypi'],
  'target_defaults': {
    'msvs_settings': {
      'VCCLCompilerTool': {
        'AdditionalOptions': ['/utf-8']
      }
    },
  },
  'targets': [
    {
      'target_name': 'fake_torque_output',
      'hard_dependency': 1,
      'type': 'none',
      'actions': [
        {
          'action_name': 'no-op',
          # For some reason, if we put these outputs under target run_torque, they
          # will be rebuilt unconditionally even if their template files are not
          # modified. This could be a bug in our gyp runner.
          # As a solution, we put the outputs here in its own target so that ninja
          # will know that some target does provide these generated files and ninja
          # does not rebuild them all the time.
          'outputs': [
            '<(torque_output_root)/torque-generated/bit-fields.h',
            '<(torque_output_root)/torque-generated/builtin-definitions.h',
            '<(torque_output_root)/torque-generated/interface-descriptors.inc',
            '<(torque_output_root)/torque-generated/factory.cc',
            '<(torque_output_root)/torque-generated/factory.inc',
            '<(torque_output_root)/torque-generated/field-offsets.h',
            '<(torque_output_root)/torque-generated/class-verifiers.cc',
            '<(torque_output_root)/torque-generated/class-verifiers.h',
            '<(torque_output_root)/torque-generated/enum-verifiers.cc',
            '<(torque_output_root)/torque-generated/objects-printer.cc',
            '<(torque_output_root)/torque-generated/objects-body-descriptors-inl.inc',
            '<(torque_output_root)/torque-generated/class-definitions.cc',
            '<(torque_output_root)/torque-generated/class-definitions-inl.h',
            '<(torque_output_root)/torque-generated/class-definitions.h',
            '<(torque_output_root)/torque-generated/class-debug-readers.cc',
            '<(torque_output_root)/torque-generated/class-debug-readers.h',
            '<(torque_output_root)/torque-generated/exported-macros-assembler.cc',
            '<(torque_output_root)/torque-generated/exported-macros-assembler.h',
            '<(torque_output_root)/torque-generated/csa-types.h',
            '<(torque_output_root)/torque-generated/instance-types.h',
            '<(torque_output_root)/torque-generated/runtime-macros.cc',
            '<(torque_output_root)/torque-generated/runtime-macros.h',
            '<(torque_output_root)/torque-generated/internal-class-definitions.h',
            '<(torque_output_root)/torque-generated/internal-class-definitions-inl.h',
            '<(torque_output_root)/torque-generated/exported-class-definitions.h',
            '<(torque_output_root)/torque-generated/exported-class-definitions-inl.h',
          ],
          'inputs': [],
          'action': [
            # Windows gyp runner does not like empty actions.
            'echo',
          ],
        },
      ],
    },
    {
      'target_name': 'run_torque',
      'type': 'none',
      'hard_dependency': 1,
      'dependent_settings': {
        'direct_include_dirs': [
          '<(torque_output_root)',
        ],
      },
      'dependencies': [
        'fake_torque_output',
      ],
      'actions': [
        {
          'action_name': 'run_torque_action',
          'inputs': [  # Order matters.
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)torque<(host_executable_suffix)',
            '<@(torque_files_v8_root_relative)',
          ],
          'outputs': [
            '<@(torque_outputs)',
          ],
          'action': [
            '<@(_inputs)',
            '-o', '<(torque_output_root)/torque-generated',
            '-v8-root', '<(V8_ROOT)'
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
      'msvs_disabled_warnings': [4267, 4312, 4351, 4355, 4800, 4838, 4715, 4309],
      'dependencies': [
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_maybe_icu',
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(torque_output_root)/torque-generated/csa-types.h',
          '<(torque_output_root)/torque-generated/enum-verifiers.cc',
          '<(torque_output_root)/torque-generated/exported-macros-assembler.cc',
          '<(torque_output_root)/torque-generated/exported-macros-assembler.h',
          '<(V8_ROOT)/src/torque/runtime-support.h',
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
          '<(torque_output_root)/torque-generated/class-verifiers.cc',
          '<(torque_output_root)/torque-generated/class-verifiers.h',
          '<(torque_output_root)/torque-generated/factory.cc',
          '<(torque_output_root)/torque-generated/objects-printer.cc',
          '<(torque_output_root)/torque-generated/runtime-macros.cc',
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
            'python2',
            '<(V8_ROOT)/tools/run.py',
            '<@(_inputs)',
            '<@(_outputs)',
          ],
        },
      ],
    },  # generate_bytecode_builtins_list#target
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
        '<(V8_ROOT)/src/init/setup-isolate-full.cc',

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
        '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_initializers.*?sources = ")',

        '<@(torque_outputs)',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_target_arch=="x86"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/ia32/builtins-ia32.cc',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/x64/builtins-x64.cc',
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/arm/builtins-arm.cc',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/arm64/builtins-arm64.cc',
          ],
        }],
        ['v8_target_arch=="mips" or v8_target_arch=="mipsel"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/mips/builtins-mips.cc',
          ],
        }],
        ['v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/mips64/builtins-mips64.cc',
          ],
        }],
        ['v8_target_arch=="ppc"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/ppc/builtins-ppc.cc',
          ],
        }],
        ['v8_target_arch=="ppc64"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/ppc/builtins-ppc.cc',
          ],
        }],
        ['v8_target_arch=="s390x"', {
          'sources': [
            '<(V8_ROOT)/src/builtins/s390/builtins-s390.cc',
          ],
        }],
        ['v8_enable_i18n_support==1', {
          'dependencies': [
            '<(icu_gyp_path):icui18n',
            '<(icu_gyp_path):icuuc',
          ],
        }, {
           'sources!': [
             '<(V8_ROOT)/src/builtins/builtins-intl-gen.cc',
           ],
         }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.cc',
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
          'conditions': [
            ['v8_target_arch=="arm64"', {
              'msvs_enable_marmasm': 1,
            }]
          ],
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

        ['cobalt_v8_emit_builtins_as_inline_asm==1', {
          'actions': [
            {
             'action_name': 'convert_asm_to_inline',
             'inputs': ['<(INTERMEDIATE_DIR)/embedded.S'],
             'outputs': ['<(INTERMEDIATE_DIR)/embedded.cc'],
             'action': [
               'python2',
               'tools/snapshot/asm_to_inline_asm.py',
               '>@(_inputs)',
               '>@(_outputs)',
             ],
            },
          ],
          'sources': ['<(INTERMEDIATE_DIR)/embedded.cc'],
        }, {
          'sources': ['<(INTERMEDIATE_DIR)/embedded.S'],
        }],
      ],
      'sources': [
        '<(V8_ROOT)/src/init/setup-isolate-deserialize.cc',
        '<(INTERMEDIATE_DIR)/snapshot.cc',
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
              '--no-partial-constant-pool',
              # In cross builds, the snapshot may be generated for both the host and
              # target toolchains.  The same host binary is used to generate both, so
              # mksnapshot needs to know which target OS to use at runtime.  It's weird,
              # but the target OS is really <(OS).
              '--target_os=<(OS)',
              '--target_arch=<(v8_target_arch)',
              '--startup_src', '<(INTERMEDIATE_DIR)/snapshot.cc',
              '--embedded_variant', 'Default',
              '--embedded_src', '<(INTERMEDIATE_DIR)/embedded.S',
            ],
          },
          'inputs': [
            '<(mksnapshot_exec)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/snapshot.cc',
            '<(INTERMEDIATE_DIR)/embedded.S',
          ],
          'sources': [
            '<(INTERMEDIATE_DIR)/snapshot.cc',
          ],
          'conditions': [
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
      'target_name': 'v8_version',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'direct_dependent_settings': {
        'sources': [
          '<(V8_ROOT)/include/v8-value-serializer-version.h',
          '<(V8_ROOT)/include/v8-version-string.h',
          '<(V8_ROOT)/include/v8-version.h',
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
          '<(V8_ROOT)/include/v8-cppgc.h',
          '<(V8_ROOT)/include/v8-fast-api-calls.h',
          '<(V8_ROOT)/include/v8-internal.h',
          '<(V8_ROOT)/include/v8.h',
          '<(V8_ROOT)/include/v8config.h',

          # The following headers cannot be platform-specific. The include validation
          # of `gn gen $dir --check` requires all header files to be available on all
          # platforms.
          '<(V8_ROOT)/include/v8-wasm-trap-handler-posix.h',
          '<(V8_ROOT)/include/v8-wasm-trap-handler-win.h',
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
          '<(V8_ROOT)/src/common/globals.h',
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
        'v8_base_without_compiler',
      ],
    'msvs_disabled_warnings': [4267, 4312, 4351, 4355, 4800, 4838, 4715],
      'sources': ['<@(v8_compiler_sources)'],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.cc',
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
        'v8_base_without_compiler',
      ],
      'msvs_disabled_warnings': [4267, 4312, 4351, 4355, 4800, 4838, 4715, 4309],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'AdditionalOptions': [
            '/EHsc',
            '/std:c++14',
          ],
        },
      },
      'sources': ['<@(v8_compiler_sources)'],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.cc',
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
        '<(V8_ROOT)/src/inspector/inspector.gyp:protocol_generated_sources#target',
        # Code generators that only need to be build for the host.
        'cppgc_base',
        'torque_generated_definitions',
        'v8_headers',
        'v8_libbase',
        'v8_libsampler',
        'v8_shared_internal_headers',
        'v8_version',
        # BUILD.gn public_deps
        'generate_bytecode_builtins_list#target',
        'run_torque#target',
        'v8_cppgc_shared',
        'v8_maybe_icu',
        'v8_zlib',
      ],
      'includes': ['src/inspector/inspector.gypi'],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(generate_bytecode_output_root)',
          '<(torque_output_root)',
        ],
      },
      'cflags_cc': ['-fno-exceptions'],
      'cflags_cc_host': ['-fno-exceptions'],
      'msvs_disabled_warnings': [4267, 4312, 4351, 4355, 4800, 4838, 4715, 4309],
      'sources': [
        # "//base/trace_event/common/trace_event_common.h",

        ### gcmole(all) ###
        '<(generate_bytecode_builtins_list_output)',

        '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?sources = ")',
        'src/wasm/baseline/liftoff-compiler-starboard.cc',

        '<@(inspector_all_sources)',
      ],
      'conditions': [
        ['v8_enable_third_party_heap==1', {
          # TODO(targos): add values from v8_third_party_heap_files to sources
        }, {
          'sources': [
            '<(V8_ROOT)/src/heap/third-party/heap-api-stub.cc',
          ],
        }],
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['v8_control_flow_integrity==1', {
          'sources': [
            '<(V8_ROOT)/src/execution/arm64/pointer-authentication-arm64.h',
          ],
        }, {
          'sources': [
            '<(V8_ROOT)/src/execution/pointer-authentication-dummy.h',
          ],
        }],
        ['v8_target_arch=="x86"', {
          'sources': [  ### gcmole(arch:ia32) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"x86.*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="x64"', {
          'sources': [  ### gcmole(arch:x64) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"x64.*?sources \+= ")',
          ],
          'conditions': [
            # iOS Xcode simulator builds run on an x64 target. iOS and macOS are both
            # based on Darwin and thus POSIX-compliant to a similar degree.
            ['OS=="linux" or OS=="mac" or OS=="ios" or OS=="freebsd"', {
              'sources': [
                '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?is_linux.*?sources \+= ")',
              ],
            }],
            ['OS=="win"', {
              'sources': [
                '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?is_win.*?sources \+= ")',
              ],
            }],
          ],
        }],
        ['v8_target_arch=="arm"', {
          'sources': [  ### gcmole(arch:arm) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"arm\\".*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="arm64"', {
          'sources': [  ### gcmole(arch:arm64) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"arm64\\".*?sources \+= ")',
          ],
          'conditions': [
            ['OS=="win"', {
              'sources': [
                "<(V8_ROOT)/src/diagnostics/unwinding-info-win64.cc",
                "<(V8_ROOT)/src/diagnostics/unwinding-info-win64.h"
              ],
            }],
          ],
        }],
        ['v8_target_arch=="mips" or v8_target_arch=="mipsel"', {
          'sources': [  ### gcmole(arch:mipsel) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"mips\\".*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="mips64" or v8_target_arch=="mips64el"', {
          'sources': [  ### gcmole(arch:mips64el) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"mips64\\".*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="ppc"', {
          'sources': [  ### gcmole(arch:ppc) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"ppc\\".*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="ppc64"', {
          'sources': [  ### gcmole(arch:ppc64) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"ppc64\\".*?sources \+= ")',
          ],
        }],
        ['v8_target_arch=="s390x"', {
          'sources': [  ### gcmole(arch:s390) ###
            '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_current_cpu == \\"s390\\".*?sources \+= ")',
          ],
        }],
        ['OS=="win"', {
          'msvs_precompiled_header': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.h',
          'msvs_precompiled_source': '<(V8_ROOT)/../../tools/msvs/pch/v8_pch.cc',
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
          'sources': ['src/deoptimizer/deoptimizer-cfi-empty.cc'],
          'target_conditions': [
            ['_toolset=="host"', {
              # if _toolset="host", we use the native host build which does not
              # involve Starboard at all.
              'conditions': [
                ['host_os=="win"', {
                  'sources': [
                    'src/diagnostics/unwinding-info-win64.cc',
                    'src/diagnostics/unwinding-info-win64.h',
                    'src/trap-handler/handler-inside-win.cc',
                    'src/trap-handler/handler-inside-win.h',
                    'src/trap-handler/handler-outside-win.cc',
                    'src/heap/base/asm/x64/push_registers_masm.S',
                  ],
                  'sources/': [
                    ['exclude','src/heap/base/asm/x64/push_registers_asm',]
                  ],
                }],
                ['host_os=="linux" or host_os=="mac"', {
                  'conditions': [
                    ['v8_target_arch=="x64"', {
                      'sources': [
                        "src/trap-handler/handler-inside-posix.cc",
                        "src/trap-handler/handler-inside-posix.h",
                        "src/trap-handler/handler-outside-posix.cc",
                      ],
                    }],
                  ],
                }],
              ],
            }, {
              'conditions': [
                ['cobalt_compiled_by_msvc', {
                  'sources': [
                    'src/heap/base/asm/x64/push_registers_masm.S',
                  ],
                  'sources/': [
                    ['exclude','src/heap/base/asm/x64/push_registers_asm',]
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
            'run_gen-regexp-special-case',
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
             '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_base_without_compiler.*?v8_enable_i18n_support.*?sources -= ")',
           ],
         }],
        ['v8_postmortem_support', {
          'dependencies': ['postmortem-metadata#target'],
        }],
        ['v8_enable_third_party_heap', {
          # TODO(targos): add values from v8_third_party_heap_libs to link_settings.libraries
        }],
        # Platforms that don't have Compare-And-Swap (CAS) support need to link atomic library
        # to implement atomic memory access
        ['v8_current_cpu in ["mips", "mipsel", "mips64", "mips64el", "ppc", "arm"]', {
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
        '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"torque_base.*?sources = ")',
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
        '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"torque_ls_base.*?sources = ")',
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
        '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"v8_libbase.*?sources = ")',
      ],

      'dependencies': [
        'v8_headers',
      ],
      'msvs_disabled_warnings': [4267, 4312, 4351, 4355, 4800, 4838, 4715, 4309],

      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['is_component_build', {
          'defines': ["BUILDING_V8_BASE_SHARED"],
        }],
        ['is_posix or is_fuchsia', {
          'sources': [
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
          ],
          'conditions': [
            ['OS != "aix" and OS != "solaris"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
                '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
              ],
            }],
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-linux.cc',
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
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-aix.cc',
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
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
          ],
          'link_settings': {
            'target_conditions': [
              ['_toolset=="host" and host_os=="linux"', {
                'libraries': [
                  '-ldl',
                  '-lrt'
                ],
              }],
            ],
          },
          'target_conditions': [
            ['_toolset=="host"', {
              'sources': [
                '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
                '<(V8_ROOT)/src/base/platform/platform-linux.cc',
              ],
            }, {
              'sources': [
                '<(V8_ROOT)/src/base/debug/stack_trace_android.cc',
                '<(V8_ROOT)/src/base/platform/platform-linux.cc',
              ],
            }],
          ],
        }],
        ['is_fuchsia', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_fuchsia.cc',
            '<(V8_ROOT)/src/base/platform/platform-fuchsia.cc',
          ]
        }],
        ['OS == "mac" or OS == "ios"', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-macos.cc',
          ]
        }],
        ['is_win', {
          'sources': [
            '<(V8_ROOT)/src/base/debug/stack_trace_win.cc',
            '<(V8_ROOT)/src/base/platform/platform-win32.cc',
            '<(V8_ROOT)/src/base/win32-headers.h',
          ],
          'conditions': [['target_arch == "arm64"', {
            'defines': ['_WIN32_WINNT=0x0602'], # For GetCurrentThreadStackLimits on Windows on Arm
          }]],
          'defines': ['_CRT_RAND_S'], # for rand_s()
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
          #   '<(V8_ROOT)/tools/mips_toolchain/sysroot/usr/lib/',
          #   '<(V8_ROOT)/tools/mips_toolchain/sysroot/usr/lib/',
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
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-solaris.cc',
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
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
            '<(V8_ROOT)/src/base/qnx-math.h'
          ],
          'target_conditions': [
            ['_toolset=="host" and host_os=="linux"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-linux.cc'
              ],
            }],
            ['_toolset=="host" and host_os=="mac"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-macos.cc'
              ],
            }],
            ['_toolset=="target"', {
              'sources': [
                '<(V8_ROOT)/src/base/platform/platform-qnx.cc'
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
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-freebsd.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
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
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-openbsd.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
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
            '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-openbsd.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix.h',
            '<(V8_ROOT)/src/base/platform/platform-posix.cc',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
            '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
          ],
        }
         ],
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
                    '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
                    '<(V8_ROOT)/src/base/platform/platform-linux.cc',
                    '<(V8_ROOT)/src/base/platform/platform-posix.h',
                    '<(V8_ROOT)/src/base/platform/platform-posix.cc',
                    '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
                    '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
                  ],
                }],
                ['host_os=="win"', {
                  # Most of the following codes are copied from 'OS="win"'
                  # section.
                  'defines': [
                    '_CRT_RAND_S'  # for rand_s()
                  ],
                  'variables': {
                    'gyp_generators': '<!pymod_do_main(starboard.build.gyp_functions getenv GYP_GENERATORS)',
                  },
                  'sources': [
                    '<(V8_ROOT)/src/base/debug/stack_trace_win.cc',
                    '<(V8_ROOT)/src/base/platform/platform-win32.cc',
                    '<(V8_ROOT)/src/base/win32-headers.h',
                  ],
                  'include_dirs': [
                    '<(torque_output_root)',
                  ],
                  'msvs_disabled_warnings': [4351, 4355, 4800],
                }],
                ['host_os == "mac"', {
                  'sources': [
                    '<(V8_ROOT)/src/base/debug/stack_trace_posix.cc',
                    '<(V8_ROOT)/src/base/platform/platform-macos.cc',
                    '<(V8_ROOT)/src/base/platform/platform-posix.h',
                    '<(V8_ROOT)/src/base/platform/platform-posix.cc',
                    '<(V8_ROOT)/src/base/platform/platform-posix-time.h',
                    '<(V8_ROOT)/src/base/platform/platform-posix-time.cc',
                  ],
                }],
              ],
            }, {
              # If _toolset=="target", build with target platform's Starboard.
              'sources': [
                '<(V8_ROOT)/src/base/debug/stack_trace_starboard.cc',
                '<(V8_ROOT)/src/base/platform/platform-starboard.cc',
              ],
            }],
          ],
        }],
      ],
    },  # v8_libbase
    {
      'target_name': 'v8_libplatform',
      'type': 'static_library',
      'dependencies': [
        'v8_libbase',
      ],
      'sources': [
        '<(V8_ROOT)/base/trace_event/common/trace_event_common.h',
        '<(V8_ROOT)/include/libplatform/libplatform-export.h',
        '<(V8_ROOT)/include/libplatform/libplatform.h',
        '<(V8_ROOT)/include/libplatform/v8-tracing.h',
        '<(V8_ROOT)/src/libplatform/default-foreground-task-runner.cc',
        '<(V8_ROOT)/src/libplatform/default-foreground-task-runner.h',
        '<(V8_ROOT)/src/libplatform/default-job.cc',
        '<(V8_ROOT)/src/libplatform/default-job.h',
        '<(V8_ROOT)/src/libplatform/default-platform.cc',
        '<(V8_ROOT)/src/libplatform/default-platform.h',
        '<(V8_ROOT)/src/libplatform/default-worker-threads-task-runner.cc',
        '<(V8_ROOT)/src/libplatform/default-worker-threads-task-runner.h',
        '<(V8_ROOT)/src/libplatform/delayed-task-queue.cc',
        '<(V8_ROOT)/src/libplatform/delayed-task-queue.h',
        '<(V8_ROOT)/src/libplatform/task-queue.cc',
        '<(V8_ROOT)/src/libplatform/task-queue.h',
        '<(V8_ROOT)/src/libplatform/tracing/trace-buffer.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-buffer.h',
        '<(V8_ROOT)/src/libplatform/tracing/trace-config.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-object.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-writer.cc',
        '<(V8_ROOT)/src/libplatform/tracing/trace-writer.h',
        '<(V8_ROOT)/src/libplatform/tracing/tracing-controller.cc',
        '<(V8_ROOT)/src/libplatform/worker-thread.cc',
        '<(V8_ROOT)/src/libplatform/worker-thread.h',
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
            '<(V8_ROOT)/src/libplatform/tracing/json-trace-event-listener.cc',
            '<(V8_ROOT)/src/libplatform/tracing/json-trace-event-listener.h',
            '<(V8_ROOT)/src/libplatform/tracing/trace-event-listener.cc',
            '<(V8_ROOT)/src/libplatform/tracing/trace-event-listener.h',
          ],
          'dependencies': [
            '<(V8_ROOT)/third_party/perfetto:libperfetto',
            '<(V8_ROOT)/third_party/perfetto/protos/perfetto/trace:lite',
          ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(V8_ROOT)/include',
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
        '<(V8_ROOT)/src/libsampler/sampler.cc',
        '<(V8_ROOT)/src/libsampler/sampler.h'
      ],
    },  # v8_libsampler
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
        "<(V8_ROOT)/src/builtins/generate-bytecodes-builtins-list.cc",
        "<(V8_ROOT)/src/interpreter/bytecode-operands.cc",
        "<(V8_ROOT)/src/interpreter/bytecode-operands.h",
        "<(V8_ROOT)/src/interpreter/bytecodes.cc",
        "<(V8_ROOT)/src/interpreter/bytecodes.h",
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
        'v8_maybe_icu',
        # "build/win:default_exe_manifest",
      ],

      'sources': [
        '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "\\"mksnapshot.*?sources = ")',
      ],
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host'],
        }],
        ['clang==1 and host_os!="mac"', {
          'ldflags_host': [
            '-fuse-ld=lld',
          ],
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
        "<(V8_ROOT)/src/torque/torque.cc",
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
        "<(V8_ROOT)/src/torque/ls/torque-language-server.cc",
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
        "<(V8_ROOT)/src/regexp/gen-regexp-special-case.cc",
      ],
    },  # gen-regexp-special-case
#    {
#      'target_name': 'run_gen-regexp-special-case',
#      'type': 'none',
#      'conditions': [
#        ['want_separate_host_toolset', {
#          'dependencies': ['gen-regexp-special-case#host'],
#          'toolsets': ['host', 'target'],
#        }, {
#          'dependencies': ['gen-regexp-special-case'],
#        }],
#      ],
#      'actions': [
#        {
#          'action_name': 'run_gen-regexp-special-case_action',
#          'inputs': [
#            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)gen-regexp-special-case<(EXECUTABLE_SUFFIX)',
#          ],
#          'outputs': [
#            '<(SHARED_INTERMEDIATE_DIR)/src/regexp/special-case.cc',
#          ],
#          'action': [
#            'python2',
#            '<(V8_ROOT)/tools/run.py',
#            '<@(_inputs)',
#            '<@(_outputs)',
#          ],
#        },
#      ],
#    },  # run_gen-regexp-special-case
    {
      'target_name': 'cppgc_base',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
      ],
      'direct_dependent_settings': {
        'sources': [
          '<!@pymod_do_main(third_party.v8.gypfiles.GN-scraper "<(V8_ROOT)/BUILD.gn"  "v8_source_set.\\"cppgc_base.*?sources = ")',
        ],
      },
    },  # cppgc_base
    {
      'target_name': 'v8_cppgc_shared',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'direct_dependent_settings': {
        'sources': [
          '<(V8_ROOT)/src/heap/base/stack.cc',
          '<(V8_ROOT)/src/heap/base/stack.h',
          '<(V8_ROOT)/src/heap/base/worklist.cc',
          '<(V8_ROOT)/src/heap/base/worklist.h',
        ],
        'conditions': [
          ['clang or OS!="win"', {
            'conditions': [
              ['_toolset == "host" and host_arch == "x64" or _toolset == "target" and target_arch=="x64"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/x64/push_registers_asm.cc',
                ],
              }],
              ['_toolset == "host" and target_arch == "arm" or _toolset == "host" and target_arch=="x86"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/ia32/push_registers_asm.cc',
                ],
                # x86 target requires x86 host build.
                'sources/': [
                  ['exclude','src/heap/base/asm/x64/push_registers_asm',]
                ],
              }],
              ['_toolset == "host" and host_arch == "arm" or _toolset == "target" and target_arch=="arm"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/arm/push_registers_asm.cc',
                ],
              }],
              ['_toolset == "host" and host_arch == "arm64" or _toolset == "target" and target_arch=="arm64"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/arm64/push_registers_asm.cc',
                ],
              }],
              ['_toolset == "host" and host_arch == "ppc64" or _toolset == "target" and target_arch=="ppc64"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/ppc/push_registers_asm.cc',
                ],
              }],
              ['_toolset == "host" and host_arch == "s390x" or _toolset == "target" and target_arch=="s390x"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/s390/push_registers_asm.cc',
                ],
              }],
              ['_toolset == "host" and host_arch == "mips" or _toolset == "target" and target_arch=="mips" or _toolset == "host" and host_arch == "mipsel" or _toolset == "target" and target_arch=="mipsel"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/mips/push_registers_asm.cc',
                ],
              }],
              ['_toolset == "host" and host_arch == "mips64" or _toolset == "target" and target_arch=="mips64" or _toolset == "host" and host_arch == "mips64el" or _toolset == "target" and target_arch=="mips64el"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/mips64/push_registers_asm.cc',
                ],
              }],
            ]
          }],
          ['OS=="win"', {
            'conditions': [
              ['_toolset == "host" and host_arch == "x64" or _toolset == "target" and target_arch=="x64"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/x64/push_registers_masm.S',
                ],
              }],
              ['_toolset == "host" and host_arch == "x86" or _toolset == "target" and target_arch=="x86"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/ia32/push_registers_masm.S',
                ],
              }],
              ['_toolset == "host" and host_arch == "arm64" or _toolset == "target" and target_arch=="arm64"', {
                'sources': [
                  '<(V8_ROOT)/src/heap/base/asm/arm64/push_registers_masm.S',
                ],
              }],
            ],
          }],
        ],
      },
    },  # v8_cppgc_shared

    ###############################################################################
    # Public targets
    #

    {
      'target_name': 'v8',
      'hard_dependency': 1,
      'toolsets': ['target'],
      'dependencies': [
        'v8_snapshot',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'type': '<(component)',
          'sources': [
            # Note: on non-Windows we still build this file so that gyp
            # has some sources to link into the component.
            '<(V8_ROOT)/src/utils/v8dll-main.cc',
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
          '<(V8_ROOT)/include',
        ],
      },
      'actions': [
        {
          'action_name': 'v8_dump_build_config',
          'inputs': [
            '<(V8_ROOT)/tools/testrunner/utils/dump_build_config_gyp.py',
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
              'v8_use_siphash=<(v8_use_siphash)',
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
            'python2', '<(V8_ROOT)/tools/testrunner/utils/dump_build_config_gyp.py',
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
      'dependencies': ['run_torque#target'],
      'variables': {
        'heapobject_files': [
          '<(torque_output_root)/torque-generated/instance-types.h',
          '<(V8_ROOT)/src/objects/allocation-site.h',
          '<(V8_ROOT)/src/objects/allocation-site-inl.h',
          '<(V8_ROOT)/src/objects/cell.h',
          '<(V8_ROOT)/src/objects/cell-inl.h',
          '<(V8_ROOT)/src/objects/code.h',
          '<(V8_ROOT)/src/objects/code-inl.h',
          '<(V8_ROOT)/src/objects/data-handler.h',
          '<(V8_ROOT)/src/objects/data-handler-inl.h',
          '<(V8_ROOT)/src/objects/descriptor-array.h',
          '<(V8_ROOT)/src/objects/descriptor-array-inl.h',
          '<(V8_ROOT)/src/objects/feedback-cell.h',
          '<(V8_ROOT)/src/objects/feedback-cell-inl.h',
          '<(V8_ROOT)/src/objects/fixed-array.h',
          '<(V8_ROOT)/src/objects/fixed-array-inl.h',
          '<(V8_ROOT)/src/objects/heap-number.h',
          '<(V8_ROOT)/src/objects/heap-number-inl.h',
          '<(V8_ROOT)/src/objects/heap-object.h',
          '<(V8_ROOT)/src/objects/heap-object-inl.h',
          '<(V8_ROOT)/src/objects/instance-type.h',
          '<(V8_ROOT)/src/objects/js-array-buffer.h',
          '<(V8_ROOT)/src/objects/js-array-buffer-inl.h',
          '<(V8_ROOT)/src/objects/js-array.h',
          '<(V8_ROOT)/src/objects/js-array-inl.h',
          '<(V8_ROOT)/src/objects/js-function-inl.h',
          '<(V8_ROOT)/src/objects/js-function.cc',
          '<(V8_ROOT)/src/objects/js-function.h',
          '<(V8_ROOT)/src/objects/js-objects.cc',
          '<(V8_ROOT)/src/objects/js-objects.h',
          '<(V8_ROOT)/src/objects/js-objects-inl.h',
          '<(V8_ROOT)/src/objects/js-promise.h',
          '<(V8_ROOT)/src/objects/js-promise-inl.h',
          '<(V8_ROOT)/src/objects/js-regexp.cc',
          '<(V8_ROOT)/src/objects/js-regexp.h',
          '<(V8_ROOT)/src/objects/js-regexp-inl.h',
          '<(V8_ROOT)/src/objects/js-regexp-string-iterator.h',
          '<(V8_ROOT)/src/objects/js-regexp-string-iterator-inl.h',
          '<(V8_ROOT)/src/objects/map.cc',
          '<(V8_ROOT)/src/objects/map.h',
          '<(V8_ROOT)/src/objects/map-inl.h',
          '<(V8_ROOT)/src/objects/name.h',
          '<(V8_ROOT)/src/objects/name-inl.h',
          '<(V8_ROOT)/src/objects/objects.h',
          '<(V8_ROOT)/src/objects/objects-inl.h',
          '<(V8_ROOT)/src/objects/oddball.h',
          '<(V8_ROOT)/src/objects/oddball-inl.h',
          '<(V8_ROOT)/src/objects/primitive-heap-object.h',
          '<(V8_ROOT)/src/objects/primitive-heap-object-inl.h',
          '<(V8_ROOT)/src/objects/scope-info.h',
          '<(V8_ROOT)/src/objects/script.h',
          '<(V8_ROOT)/src/objects/script-inl.h',
          '<(V8_ROOT)/src/objects/shared-function-info.cc',
          '<(V8_ROOT)/src/objects/shared-function-info.h',
          '<(V8_ROOT)/src/objects/shared-function-info-inl.h',
          '<(V8_ROOT)/src/objects/string.cc',
          '<(V8_ROOT)/src/objects/string-comparator.cc',
          '<(V8_ROOT)/src/objects/string-comparator.h',
          '<(V8_ROOT)/src/objects/string.h',
          '<(V8_ROOT)/src/objects/string-inl.h',
          '<(V8_ROOT)/src/objects/struct.h',
          '<(V8_ROOT)/src/objects/struct-inl.h',
        ],
      },
      'actions': [
        {
          'action_name': 'gen-postmortem-metadata',
          'inputs': [
            '<(V8_ROOT)/tools/gen-postmortem-metadata.py',
            '<@(heapobject_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/debug-support.cc',
          ],
          'action': [
            'python2',
            '<(V8_ROOT)/tools/gen-postmortem-metadata.py',
            '<@(_outputs)',
            '<@(heapobject_files)'
          ],
        },
      ],
      'direct_dependent_settings': {
        'sources': ['<(SHARED_INTERMEDIATE_DIR)/debug-support.cc', ],
      },
    },  # postmortem-metadata

    {
      'target_name': 'v8_zlib',
      'type': 'static_library',
      'conditions': [
        ['want_separate_host_toolset', {
          'toolsets': ['host', 'target'],
        }],
        ['OS=="win"', {
          'conditions': [
            ['"<(target_arch)"=="arm64" and _toolset=="target"', {
              'defines': ['CPU_NO_SIMD']
            }, {
              'defines': ['X86_WINDOWS']
            }]
          ]
        }],
      ],

      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/zlib',
          '<(DEPTH)/third_party/zlib/google',
        ],
      },
      'dependencies': [
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib#target',
      ],
      'target_conditions': [
        # cobalt/src/third_party/zlib/zlib.gyp does not provide a host biuld,
        # We can use the following simple zlib build rule from nodejs but for
        # target builds we need to use the starboardized gyp rule.
        ['_toolset == "host"', {
          'defines': [ 'ZLIB_IMPLEMENTATION' ],
          'include_dirs': [
            '<(DEPTH)/third_party/zlib',
            '<(DEPTH)/third_party/zlib/google',
          ],
          'sources': [
            '<(DEPTH)/third_party/zlib/adler32.c',
            '<(DEPTH)/third_party/zlib/chromeconf.h',
            '<(DEPTH)/third_party/zlib/compress.c',
            '<(DEPTH)/third_party/zlib/contrib/optimizations/insert_string.h',
            '<(DEPTH)/third_party/zlib/contrib/optimizations/insert_string.h',
            #'<(DEPTH)/third_party/zlib/cpu_features.c',
            #'<(DEPTH)/third_party/zlib/cpu_features.h',
            '<(DEPTH)/third_party/zlib/crc32.c',
            '<(DEPTH)/third_party/zlib/crc32.h',
            '<(DEPTH)/third_party/zlib/deflate.c',
            '<(DEPTH)/third_party/zlib/deflate.h',
            '<(DEPTH)/third_party/zlib/gzclose.c',
            '<(DEPTH)/third_party/zlib/gzguts.h',
            '<(DEPTH)/third_party/zlib/gzlib.c',
            '<(DEPTH)/third_party/zlib/gzread.c',
            '<(DEPTH)/third_party/zlib/gzwrite.c',
            '<(DEPTH)/third_party/zlib/infback.c',
            '<(DEPTH)/third_party/zlib/inffast.c',
            '<(DEPTH)/third_party/zlib/inffast.h',
            '<(DEPTH)/third_party/zlib/inffixed.h',
            '<(DEPTH)/third_party/zlib/inflate.c',
            '<(DEPTH)/third_party/zlib/inflate.h',
            '<(DEPTH)/third_party/zlib/inftrees.c',
            '<(DEPTH)/third_party/zlib/inftrees.h',
            '<(DEPTH)/third_party/zlib/trees.c',
            '<(DEPTH)/third_party/zlib/trees.h',
            '<(DEPTH)/third_party/zlib/uncompr.c',
            '<(DEPTH)/third_party/zlib/zconf.h',
            '<(DEPTH)/third_party/zlib/zlib.h',
            '<(DEPTH)/third_party/zlib/zutil.c',
            '<(DEPTH)/third_party/zlib/zutil.h',
            '<(DEPTH)/third_party/zlib/google/compression_utils_portable.cc',
            '<(DEPTH)/third_party/zlib/google/compression_utils_portable.h',
          ],
        }],
      ],
    },  # v8_zlib
  ],
}

