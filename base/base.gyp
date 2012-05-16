# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/win_precompile.gypi',
    'base.gypi',
  ],
  'targets': [
    {
      'target_name': 'base_i18n',
      'type': '<(component)',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'dependencies': [
        'base',
        'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            # i18n/rtl.cc uses gtk
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
      'export_dependent_settings': [
        'base',
      ],
      'defines': [
        'BASE_I18N_IMPLEMENTATION',
      ],
      'sources': [
        'i18n/base_i18n_export.h',
        'i18n/bidi_line_iterator.cc',
        'i18n/bidi_line_iterator.h',
        'i18n/break_iterator.cc',
        'i18n/break_iterator.h',
        'i18n/char_iterator.cc',
        'i18n/char_iterator.h',
        'i18n/case_conversion.cc',
        'i18n/case_conversion.h',
        'i18n/file_util_icu.cc',
        'i18n/file_util_icu.h',
        'i18n/icu_encoding_detection.cc',
        'i18n/icu_encoding_detection.h',
        'i18n/icu_string_conversions.cc',
        'i18n/icu_string_conversions.h',
        'i18n/icu_util.cc',
        'i18n/icu_util.h',
        'i18n/number_formatting.cc',
        'i18n/number_formatting.h',
        'i18n/rtl.cc',
        'i18n/rtl.h',
        'i18n/string_search.cc',
        'i18n/string_search.h',
        'i18n/time_formatting.cc',
        'i18n/time_formatting.h',
      ],
    },
    {
      # This is the subset of files from base that should not be used with a
      # dynamic library. Note that this library cannot depend on base because
      # base depends on base_static.
      'target_name': 'base_static',
      'type': 'static_library',
      'variables': {
        'enable_wexit_time_destructors': 1,
        'optimize': 'max',
      },
      'toolsets': ['host', 'target'],
      'sources': [
        'base_switches.cc',
        'base_switches.h',
        'win/pe_image.cc',
        'win/pe_image.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      # TODO(rvargas): Remove this when gyp finally supports a clean model.
      # See bug 36232.
      'target_name': 'base_static_win64',
      'type': 'static_library',
      'sources': [
        'base_switches.cc',
        'base_switches.h',
        'win/pe_image.cc',
        'win/pe_image.h',
      ],
      'include_dirs': [
        '..',
      ],
      'configurations': {
        'Common_Base': {
          'msvs_target_platform': 'x64',
        },
      },
      'defines': [
        'NACL_WIN64',
      ],
      # TODO(rvargas): Bug 78117. Remove this.
      'msvs_disabled_warnings': [
        4244,
      ],
    },
    # Include this target for a main() function that simply instantiates
    # and runs a base::TestSuite.
    {
      'target_name': 'run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        'test_support_base',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
    },
    {
      'target_name': 'base_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        # Tests.
        'android/jni_android_unittest.cc',
        'android/path_utils_unittest.cc',
        'android/scoped_java_ref_unittest.cc',
        'at_exit_unittest.cc',
        'atomicops_unittest.cc',
        'base64_unittest.cc',
        'bind_helpers_unittest.cc',
        'bind_unittest.cc',
        'bind_unittest.nc',
        'bits_unittest.cc',
        'build_time_unittest.cc',
        'callback_unittest.cc',
        'callback_unittest.nc',
        'cancelable_callback_unittest.cc',
        'command_line_unittest.cc',
        'cpu_unittest.cc',
        'debug/leak_tracker_unittest.cc',
        'debug/stack_trace_unittest.cc',
        'debug/trace_event_unittest.cc',
        'debug/trace_event_win_unittest.cc',
        'dir_reader_posix_unittest.cc',
        'environment_unittest.cc',
        'file_descriptor_shuffle_unittest.cc',
        'file_path_unittest.cc',
        'file_util_proxy_unittest.cc',
        'file_util_unittest.cc',
        'file_version_info_unittest.cc',
        'gmock_unittest.cc',
        'id_map_unittest.cc',
        'i18n/break_iterator_unittest.cc',
        'i18n/char_iterator_unittest.cc',
        'i18n/case_conversion_unittest.cc',
        'i18n/file_util_icu_unittest.cc',
        'i18n/icu_string_conversions_unittest.cc',
        'i18n/number_formatting_unittest.cc',
        'i18n/rtl_unittest.cc',
        'i18n/string_search_unittest.cc',
        'i18n/time_formatting_unittest.cc',
        'json/json_parser_unittest.cc',
        'json/json_reader_unittest.cc',
        'json/json_value_converter_unittest.cc',
        'json/json_value_serializer_unittest.cc',
        'json/json_writer_unittest.cc',
        'json/string_escape_unittest.cc',
        'lazy_instance_unittest.cc',
        'linked_list_unittest.cc',
        'logging_unittest.cc',
        'mac/closure_blocks_leopard_compat_unittest.cc',
        'mac/foundation_util_unittest.mm',
        'mac/mac_util_unittest.mm',
        'mac/objc_property_releaser_unittest.mm',
        'mac/scoped_sending_event_unittest.mm',
        'md5_unittest.cc',
        'memory/aligned_memory_unittest.cc',
        'memory/linked_ptr_unittest.cc',
        'memory/mru_cache_unittest.cc',
        'memory/ref_counted_memory_unittest.cc',
        'memory/ref_counted_unittest.cc',
        'memory/scoped_nsobject_unittest.mm',
        'memory/scoped_ptr_unittest.cc',
        'memory/scoped_ptr_unittest.nc',
        'memory/scoped_vector_unittest.cc',
        'memory/singleton_unittest.cc',
        'memory/weak_ptr_unittest.cc',
        'message_loop_proxy_impl_unittest.cc',
        'message_loop_proxy_unittest.cc',
        'message_loop_unittest.cc',
        'message_pump_glib_unittest.cc',
        'message_pump_libevent_unittest.cc',
        'metrics/field_trial_unittest.cc',
        'metrics/histogram_unittest.cc',
        'metrics/stats_table_unittest.cc',
        'observer_list_unittest.cc',
        'path_service_unittest.cc',
        'pickle_unittest.cc',
        'platform_file_unittest.cc',
        'pr_time_unittest.cc',
        'process_util_unittest.cc',
        'process_util_unittest_mac.h',
        'process_util_unittest_mac.mm',
        'profiler/tracked_time_unittest.cc',
        'property_bag_unittest.cc',
        'rand_util_unittest.cc',
        'scoped_native_library_unittest.cc',
        'scoped_temp_dir_unittest.cc',
        'sha1_unittest.cc',
        'shared_memory_unittest.cc',
        'stack_container_unittest.cc',
        'string16_unittest.cc',
        'string_number_conversions_unittest.cc',
        'string_piece_unittest.cc',
        'string_split_unittest.cc',
        'string_tokenizer_unittest.cc',
        'string_util_unittest.cc',
        'stringize_macros_unittest.cc',
        'stringprintf_unittest.cc',
        'synchronization/cancellation_flag_unittest.cc',
        'synchronization/condition_variable_unittest.cc',
        'synchronization/lock_unittest.cc',
        'synchronization/waitable_event_unittest.cc',
        'synchronization/waitable_event_watcher_unittest.cc',
        'sys_info_unittest.cc',
        'sys_string_conversions_mac_unittest.mm',
        'sys_string_conversions_unittest.cc',
        'system_monitor/system_monitor_unittest.cc',
        'task_runner_util_unittest.cc',
        'template_util_unittest.cc',
        'test/sequenced_worker_pool_owner.cc',
        'test/sequenced_worker_pool_owner.h',
        'test/trace_event_analyzer_unittest.cc',
        'threading/non_thread_safe_unittest.cc',
        'threading/platform_thread_unittest.cc',
        'threading/sequenced_worker_pool_unittest.cc',
        'threading/simple_thread_unittest.cc',
        'threading/thread_checker_unittest.cc',
        'threading/thread_collision_warner_unittest.cc',
        'threading/thread_local_storage_unittest.cc',
        'threading/thread_local_unittest.cc',
        'threading/thread_unittest.cc',
        'threading/watchdog_unittest.cc',
        'threading/worker_pool_posix_unittest.cc',
        'threading/worker_pool_unittest.cc',
        'time_unittest.cc',
        'time_win_unittest.cc',
        'timer_unittest.cc',
        'tools_sanity_unittest.cc',
        'tracked_objects_unittest.cc',
        'tuple_unittest.cc',
        'utf_offset_string_conversions_unittest.cc',
        'utf_string_conversions_unittest.cc',
        'values_unittest.cc',
        'version_unittest.cc',
        'vlog_unittest.cc',
        'win/dllmain.cc',
        'win/enum_variant_unittest.cc',
        'win/event_trace_consumer_unittest.cc',
        'win/event_trace_controller_unittest.cc',
        'win/event_trace_provider_unittest.cc',
        'win/i18n_unittest.cc',
        'win/iunknown_impl_unittest.cc',
        'win/object_watcher_unittest.cc',
        'win/pe_image_unittest.cc',
        'win/registry_unittest.cc',
        'win/sampling_profiler_unittest.cc',
        'win/scoped_bstr_unittest.cc',
        'win/scoped_comptr_unittest.cc',
        'win/scoped_process_information_unittest.cc',
        'win/scoped_variant_unittest.cc',
        'win/win_util_unittest.cc',
        'win/wrapped_window_proc_unittest.cc',
      ],
      'dependencies': [
        'base',
        'base_i18n',
        'base_static',
        'run_all_unittests',
        'test_support_base',
        'third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'includes': ['../build/nocompile.gypi'],
      'variables': {
         # TODO(ajwong): Is there a way to autodetect this?
        'module_dir': 'base'
      },
      'conditions': [
        ['OS == "android"', {
          'sources!': [
            # TODO(michaelbai): Removed the below once the fix upstreamed.
            'debug/stack_trace_unittest.cc',
            'memory/mru_cache_unittest.cc',
            'process_util_unittest.cc',
            'synchronization/cancellation_flag_unittest.cc',
          ],
          'dependencies': [
            'android/jni_generator/jni_generator.gyp:jni_generator_tests',
          ],
          'conditions': [
            ['"<(gtest_target_type)"=="shared_library"', {
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ],
            }, { # gtest_target_type != shared_library
              'sources!': [
                # The below files are excluded because of the missing JNI.
                'android/jni_android_unittest.cc',
                'android/path_utils_unittest.cc',
                'android/scoped_java_ref_unittest.cc',
              ],
            }],
          ],
        }],
        ['use_glib==1', {
          'sources!': [
            'file_version_info_unittest.cc',
          ],
          'conditions': [
            [ 'linux_use_tcmalloc==1', {
                'dependencies': [
                  'allocator/allocator.gyp:allocator',
                ],
              },
            ],
            [ 'toolkit_uses_gtk==1', {
              'sources': [
                'nix/xdg_util_unittest.cc',
              ],
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ]
            }],
          ],
          'dependencies': [
            '../build/linux/system.gyp:glib',
            '../build/linux/system.gyp:ssl',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }, {  # use_glib!=1
          'sources!': [
            'message_pump_glib_unittest.cc',
          ]
        }],
        # This is needed to trigger the dll copy step on windows.
        # TODO(mark): This should not be necessary.
        ['OS == "win"', {
          'dependencies': [
            '../third_party/icu/icu.gyp:icudata',
          ],
          'sources!': [
            'dir_reader_posix_unittest.cc',
            'file_descriptor_shuffle_unittest.cc',
            'threading/worker_pool_posix_unittest.cc',
            'message_pump_libevent_unittest.cc',
          ],
        }, {  # OS != "win"
          'dependencies': [
            '../third_party/libevent/libevent.gyp:libevent'
          ],
          'sources/': [
            ['exclude', '^win/'],
          ],
          'sources!': [
            'debug/trace_event_win_unittest.cc',
            'time_win_unittest.cc',
            'win/win_util_unittest.cc',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'closure_blocks_leopard_compat',
          ],
        }],
      ],
    },
    {
      'target_name': 'check_example',
      'type': 'executable',
      'sources': [
        'check_example.cc',
      ],
      'dependencies': [
        'base',
      ],
    },
    {
      'target_name': 'test_support_base',
      'type': 'static_library',
      'dependencies': [
        'base',
        'base_static',
        'base_i18n',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'base',
      ],
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            # test_suite initializes GTK.
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['os_posix==0', {
          'sources!': [
            'test/scoped_locale.cc',
            'test/scoped_locale.h',
          ],
        }],
        ['os_bsd==1', {
          'sources!': [
            'test/test_file_util_linux.cc',
          ],
        }],
      ],
      'sources': [
        'perftimer.cc',
        'test/mock_chrome_application_mac.h',
        'test/mock_chrome_application_mac.mm',
        'test/mock_devices_changed_observer.cc',
        'test/mock_devices_changed_observer.h',
        'test/mock_time_provider.cc',
        'test/mock_time_provider.h',
        'test/multiprocess_test.cc',
        'test/multiprocess_test.h',
        'test/perf_test_suite.cc',
        'test/perf_test_suite.h',
        'test/scoped_locale.cc',
        'test/scoped_locale.h',
        'test/sequenced_task_runner_test_template.cc',
        'test/sequenced_task_runner_test_template.h',
        'test/task_runner_test_template.cc',
        'test/task_runner_test_template.h',
        'test/test_file_util.h',
        'test/test_file_util_linux.cc',
        'test/test_file_util_mac.cc',
        'test/test_file_util_posix.cc',
        'test/test_file_util_win.cc',
        'test/test_reg_util_win.cc',
        'test/test_reg_util_win.h',
        'test/test_suite.cc',
        'test/test_suite.h',
        'test/test_stub_android.cc',
        'test/test_switches.cc',
        'test/test_switches.h',
        'test/test_timeouts.cc',
        'test/test_timeouts.h',
        'test/thread_test_helper.cc',
        'test/thread_test_helper.h',
        'test/trace_event_analyzer.cc',
        'test/trace_event_analyzer.h',
        'test/values_test_util.cc',
        'test/values_test_util.h',
      ],
    },
    {
      'target_name': 'base_unittests_run',
      'type': 'none',
      'dependencies': [
        'base_unittests',
      ],
      'includes': [
        'base_unittests.isolate',
      ],
      'actions': [
        {
          'action_name': 'isolate',
          'inputs': [
            'base_unittests.isolate',
            '<@(isolate_dependency_tracked)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/base_unittests.results',
          ],
          'action': [
            'python',
            '../tools/isolate/isolate.py',
            '--mode', '<(test_isolation_mode)',
            '--outdir', '<(test_isolation_outdir)',
            '--variable', 'PRODUCT_DIR', '<(PRODUCT_DIR)',
            '--variable', 'OS', '<(OS)',
            '--result', '<@(_outputs)',
            'base_unittests.isolate',
          ],
        },
      ],
    },
    {
      'target_name': 'test_support_perf',
      'type': 'static_library',
      'dependencies': [
        'base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'perftimer.cc',
        'test/run_all_perftests.cc',
      ],
      'direct_dependent_settings': {
        'defines': [
          'PERF_TEST',
        ],
      },
      'conditions': [
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            # Needed to handle the #include chain:
            #   base/test/perf_test_suite.h
            #   base/test/test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'base_jni_headers',
          'type': 'none',
          'variables': {
            'java_sources': [
              'android/java/org/chromium/base/BuildInfo.java',
              'android/java/org/chromium/base/LocaleUtils.java',
              'android/java/org/chromium/base/PathUtils.java',
              'android/java/org/chromium/base/SystemMessageHandler.java',
            ],
            'jni_headers': [
              '<(SHARED_INTERMEDIATE_DIR)/base/jni/build_info_jni.h',
              '<(SHARED_INTERMEDIATE_DIR)/base/jni/locale_utils_jni.h',
              '<(SHARED_INTERMEDIATE_DIR)/base/jni/path_utils_jni.h',
              '<(SHARED_INTERMEDIATE_DIR)/base/jni/system_message_handler_jni.h',
            ],
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'base_java',
          'type': 'none',
          'variables': {
            'package_name': 'base',
            'java_in_dir': 'android/java',
          },
          'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
    ['OS == "win"', {
      'targets': [
        {
          'target_name': 'debug_message',
          'type': 'executable',
          'sources': [
            'debug_message.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
        },
      ],
    }],
    ['OS=="mac"', {
     'targets': [
       {
         'target_name': 'closure_blocks_leopard_compat',
         'sources': [
           'mac/closure_blocks_leopard_compat.h',
         ],
         'conditions': [
           ['mac_sdk == "10.5"', {
             'type': 'shared_library',
             'product_name': 'closure_blocks_leopard_compat_stub',
             'variables': {
               # This target controls stripping directly. See below.
               'mac_strip': 0,
             },
             'sources': [
               'mac/closure_blocks_leopard_compat.S',
             ],
             'xcode_settings': {
               # These values are taken from libSystem.dylib in the 10.5
               # SDK. Setting LD_DYLIB_INSTALL_NAME causes anything linked
               # against this stub library to look for the symbols it
               # provides in the real libSystem at runtime. When using ld
               # from Xcode 4 or later (ld64-123.2 and up), giving two
               # libraries with the same "install name" to the linker will
               # cause it to print "ld: warning: dylibs with same install
               # name". This is harmless, and ld will behave as intended
               # here.
               #
               # The real library's compatibility version is used, and the
               # value of the current version from the SDK is used to make
               # it appear as though anything linked against this stub was
               # linked against the real thing.
               'LD_DYLIB_INSTALL_NAME': '/usr/lib/libSystem.B.dylib',
               'DYLIB_COMPATIBILITY_VERSION': '1.0.0',
               'DYLIB_CURRENT_VERSION': '111.1.4',

               # Turn on stripping (yes, even in debug mode), and add the -c
               # flag. This is what produces a stub library (MH_DYLIB_STUB)
               # as opposed to a dylib (MH_DYLIB). MH_DYLIB_STUB files
               # contain symbol tables and everything else needed for
               # linking, but are stripped of section contents. This is the
               # same way that the stub libraries in Mac OS X SDKs are
               # created. dyld will refuse to load a stub library, so this
               # provides some insurance in case anyone tries to load the
               # stub at runtime.
               'DEPLOYMENT_POSTPROCESSING': 'YES',
               'STRIP_STYLE': 'non-global',
               'STRIPFLAGS': '-c',
             },
           }, {  # else: mac_sdk != "10.5"
             # When using the 10.6 SDK or newer, the necessary definitions
             # are already present in libSystem.dylib. There is no need to
             # build a stub dylib to provide these symbols at link time.
             # This target is still useful to cause those symbols to be
             # treated as weak imports in dependents, who still must
             # #include closure_blocks_leopard_compat.h to get weak imports.
             'type': 'none',
           }],
         ],
       },
     ],
   }],
    # Special target to wrap a <(gtest_target_type)==shared_library
    # base_unittests into an android apk for execution.
    # TODO(jrg): lib.target comes from _InstallableTargetInstallPath()
    # in the gyp make generator.  What is the correct way to extract
    # this path from gyp and into 'raw' for input to antfiles?
    # Hard-coding in the gypfile seems a poor choice.
    # TODO(jrg): there has to be a shorter way to do all this.  Try
    # and convert this entire target cluster into ~5 lines that can be
    # trivially copied to other projects with only a deps change, such
    # as with a new gtest_target_type called
    # 'shared_library_apk_wrapper' that does a lot of this magically.
    ['OS=="android" and "<(gtest_target_type)"=="shared_library"', {
      'targets': [
        {
          'target_name': 'base_unittests_apk',
          'type': 'none',
          'dependencies': [
            'base',  # So that android/java/java.gyp:base_java is built
            'base_unittests',
          ],
          'actions': [
            {
              # Generate apk files (including source and antfile) from
              # a template, and builds them.
              'action_name': 'generate_and_build',
              'inputs': [
                '../testing/android/AndroidManifest.xml',
                '../testing/android/generate_native_test.py',
                '<(PRODUCT_DIR)/lib.target/libbase_unittests.so',
                '<(PRODUCT_DIR)/lib.java/chromium_base.jar'
              ],
              'outputs': [
                '<(PRODUCT_DIR)/base_unittests_apk/base_unittests-debug.apk',
              ],
              'action': [ 
                '../testing/android/generate_native_test.py',
                '--native_library',
                '<(PRODUCT_DIR)/lib.target/libbase_unittests.so',
                '--jar',
                '<(PRODUCT_DIR)/lib.java/chromium_base.jar',
                '--output',
                '<(PRODUCT_DIR)/base_unittests_apk',
                '--ant-args', 
                '-DPRODUCT_DIR=<(PRODUCT_DIR)',
                '--ant-compile'
              ],
            },
          ]
        }],
    }],
  ],
}
