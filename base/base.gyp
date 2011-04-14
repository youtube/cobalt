# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'base.gypi',
  ],
  'targets': [
    {
      'target_name': 'base_i18n',
      'type': '<(library)',
      'msvs_guid': '968F3222-9798-4D21-BE08-15ECB5EF2994',
      'dependencies': [
        'base',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            # i18n/rtl.cc uses gtk
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
      'export_dependent_settings': [
        'base',
      ],
      'sources': [
        'i18n/bidi_line_iterator.cc',
        'i18n/bidi_line_iterator.h',
        'i18n/break_iterator.cc',
        'i18n/break_iterator.h',
        'i18n/char_iterator.cc',
        'i18n/char_iterator.h',
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
        'i18n/time_formatting.cc',
        'i18n/time_formatting.h',
      ],
    },
    {
      # This is the subset of files from base that should not be used with a
      # dynamic library.
      'target_name': 'base_static',
      'type': '<(library)',
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
      'type': '<(library)',
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
    {
      'target_name': 'base_unittests',
      'type': 'executable',
      'msvs_guid': '27A30967-4BBA-48D1-8522-CDE95F7B1CEC',
      'sources': [
        # Infrastructure files.
        'test/run_all_unittests.cc',

        # Tests.
        'at_exit_unittest.cc',
        'atomicops_unittest.cc',
        'base64_unittest.cc',
        'bind_unittest.cc',
        'bits_unittest.cc',
        'callback_unittest.cc',
        'command_line_unittest.cc',
        'cpu_unittest.cc',
        'debug/leak_tracker_unittest.cc',
        'debug/stack_trace_unittest.cc',
        'debug/trace_event_win_unittest.cc',
        'dir_reader_posix_unittest.cc',
        'environment_unittest.cc',
        'file_descriptor_shuffle_unittest.cc',
        'file_path_unittest.cc',
        'file_util_unittest.cc',
        'file_version_info_unittest.cc',
        'gmock_unittest.cc',
        'id_map_unittest.cc',
        'i18n/break_iterator_unittest.cc',
        'i18n/char_iterator_unittest.cc',
        'i18n/file_util_icu_unittest.cc',
        'i18n/icu_string_conversions_unittest.cc',
        'i18n/rtl_unittest.cc',
        'json/json_reader_unittest.cc',
        'json/json_writer_unittest.cc',
        'json/string_escape_unittest.cc',
        'lazy_instance_unittest.cc',
        'linked_list_unittest.cc',
        'logging_unittest.cc',
        'mac/mac_util_unittest.mm',
        'memory/linked_ptr_unittest.cc',
        'memory/ref_counted_unittest.cc',
        'memory/scoped_native_library_unittest.cc',
        'memory/scoped_ptr_unittest.cc',
        'memory/scoped_temp_dir_unittest.cc',
        'memory/scoped_vector_unittest.cc',
        'memory/singleton_unittest.cc',
        'memory/weak_ptr_unittest.cc',
        'message_loop_proxy_impl_unittest.cc',
        'message_loop_unittest.cc',
        'message_pump_glib_unittest.cc',
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
        'rand_util_unittest.cc',
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
        'task_queue_unittest.cc',
        'task_unittest.cc',
        'template_util_unittest.cc',
        'threading/non_thread_safe_unittest.cc',
        'threading/platform_thread_unittest.cc',
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
        'win/event_trace_consumer_unittest.cc',
        'win/event_trace_controller_unittest.cc',
        'win/event_trace_provider_unittest.cc',
        'win/i18n_unittest.cc',
        'win/object_watcher_unittest.cc',
        'win/pe_image_unittest.cc',
        'win/registry_unittest.cc',
        'win/scoped_bstr_unittest.cc',
        'win/scoped_comptr_unittest.cc',
        'win/scoped_variant_unittest.cc',
        'win/win_util_unittest.cc',
        'win/wrapped_window_proc_unittest.cc',
      ],
      'dependencies': [
        'base',
        'base_i18n',
        'base_static',
        'test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'conditions': [
        ['OS == "linux" or OS == "freebsd" or OS == "openbsd" or OS == "solaris"', {
          'sources!': [
            'file_version_info_unittest.cc',
          ],
          'sources': [
            'nix/xdg_util_unittest.cc',
          ],
          'conditions': [
            [ 'linux_use_tcmalloc==1', {
                'dependencies': [
                  'allocator/allocator.gyp:allocator',
                ],
              },
            ],
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }, {  # OS != "linux" and OS != "freebsd" and OS != "openbsd" and OS != "solaris"
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
          ],
        }, {  # OS != "win"
          'sources/': [
            ['exclude', '^win/'],
          ],
          'sources!': [
            'system_monitor_unittest.cc',
            'time_win_unittest.cc',
            'trace_event_win_unittest.cc',
            'win_util_unittest.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_base',
      'type': '<(library)',
      'dependencies': [
        'base',
        'base_i18n',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            # test_suite initializes GTK.
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
      'sources': [
        'perftimer.cc',
        'test/mock_chrome_application_mac.h',
        'test/mock_chrome_application_mac.mm',
        'test/multiprocess_test.cc',
        'test/multiprocess_test.h',
        'test/perf_test_suite.cc',
        'test/perf_test_suite.h',
        'test/test_file_util.h',
        'test/test_file_util_linux.cc',
        'test/test_file_util_mac.cc',
        'test/test_file_util_posix.cc',
        'test/test_file_util_win.cc',
        'test/test_suite.cc',
        'test/test_suite.h',
        'test/test_switches.cc',
        'test/test_switches.h',
        'test/test_timeouts.cc',
        'test/test_timeouts.h',
      ],
    },
    {
      'target_name': 'test_support_perf',
      'type': '<(library)',
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
        ['OS == "linux" or OS == "freebsd" or OS == "openbsd" or OS == "solaris"', {
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
    [ 'OS == "win"', {
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
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
