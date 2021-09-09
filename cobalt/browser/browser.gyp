# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
    'sb_pedantic_warnings': 1,
  },
  'targets': [
    {
      'target_name': 'browser',
      'type': 'static_library',
      'sources': [
        'application.cc',
        'application.h',
        'browser_module.cc',
        'browser_module.h',
        'device_authentication.cc',
        'device_authentication.h',
        'lifecycle_observer.h',
        'memory_settings/auto_mem.cc',
        'memory_settings/auto_mem.h',
        'memory_settings/auto_mem_settings.cc',
        'memory_settings/auto_mem_settings.h',
        'memory_settings/calculations.cc',
        'memory_settings/calculations.h',
        'memory_settings/checker.cc',
        'memory_settings/checker.h',
        'memory_settings/constants.h',
        'memory_settings/scaling_function.cc',
        'memory_settings/scaling_function.h',
        'memory_settings/memory_settings.cc',
        'memory_settings/memory_settings.h',
        'memory_settings/pretty_print.cc',
        'memory_settings/pretty_print.h',
        'memory_settings/table_printer.cc',
        'memory_settings/table_printer.h',
        'memory_settings/texture_dimensions.h',
        'memory_tracker/tool.cc',
        'memory_tracker/tool.h',
        'memory_tracker/tool/buffered_file_writer.cc',
        'memory_tracker/tool/buffered_file_writer.h',
        'memory_tracker/tool/compressed_time_series_tool.cc',
        'memory_tracker/tool/compressed_time_series_tool.h',
        'memory_tracker/tool/histogram_table_csv_base.h',
        'memory_tracker/tool/internal_fragmentation_tool.cc',
        'memory_tracker/tool/internal_fragmentation_tool.h',
        'memory_tracker/tool/leak_finder_tool.cc',
        'memory_tracker/tool/leak_finder_tool.h',
        'memory_tracker/tool/log_writer_tool.cc',
        'memory_tracker/tool/log_writer_tool.h',
        'memory_tracker/tool/malloc_stats_tool.cc',
        'memory_tracker/tool/malloc_stats_tool.h',
        'memory_tracker/tool/malloc_logger_tool.cc',
        'memory_tracker/tool/malloc_logger_tool.h',
        'memory_tracker/tool/memory_size_binner_tool.cc',
        'memory_tracker/tool/memory_size_binner_tool.h',
        'memory_tracker/tool/params.cc',
        'memory_tracker/tool/params.h',
        'memory_tracker/tool/print_csv_tool.cc',
        'memory_tracker/tool/print_csv_tool.h',
        'memory_tracker/tool/print_tool.cc',
        'memory_tracker/tool/print_tool.h',
        'memory_tracker/tool/tool_impl.cc',
        'memory_tracker/tool/tool_impl.h',
        'memory_tracker/tool/tool_thread.cc',
        'memory_tracker/tool/tool_thread.h',
        'memory_tracker/tool/util.cc',
        'memory_tracker/tool/util.h',
        'on_screen_keyboard_starboard_bridge.cc',
        'on_screen_keyboard_starboard_bridge.h',
        'render_tree_combiner.cc',
        'render_tree_combiner.h',
        'screen_shot_writer.cc',
        'screen_shot_writer.h',
        'splash_screen.cc',
        'splash_screen.h',
        'splash_screen_cache.cc',
        'splash_screen_cache.h',
        'storage_upgrade_handler.cc',
        'storage_upgrade_handler.h',
        'suspend_fuzzer.cc',
        'suspend_fuzzer.h',
        'switches.cc',
        'switches.h',
        'system_platform_error_handler.cc',
        'system_platform_error_handler.h',
        'url_handler.cc',
        'url_handler.h',
        'user_agent_platform_info.cc',
        'user_agent_platform_info.h',
        'user_agent_string.cc',
        'user_agent_string.h',
        'web_module.cc',
        'web_module.h',
        'web_module_stat_tracker.cc',
        'web_module_stat_tracker.h',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/account/account.gyp:account',
        '<(DEPTH)/cobalt/audio/audio.gyp:audio',
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/configuration/configuration.gyp:configuration',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:css_parser',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:dom_parser',
        '<(DEPTH)/cobalt/encoding/encoding.gyp:text_encoding',
        '<(DEPTH)/cobalt/fetch/fetch.gyp:fetch',
        '<(DEPTH)/cobalt/h5vcc/h5vcc.gyp:h5vcc',
        '<(DEPTH)/cobalt/input/input.gyp:input',
        '<(DEPTH)/cobalt/layout/layout.gyp:layout',
        '<(DEPTH)/cobalt/math/math.gyp:math',
        '<(DEPTH)/cobalt/media/media.gyp:media',
        '<(DEPTH)/cobalt/media_capture/media_capture.gyp:*',
        '<(DEPTH)/cobalt/media_session/media_session.gyp:media_session',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/cobalt/overlay_info/overlay_info.gyp:overlay_info',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:renderer',
        '<(DEPTH)/cobalt/renderer/test/png_utils/png_utils.gyp:png_utils',
        '<(DEPTH)/cobalt/script/engine.gyp:engine',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/sso/sso.gyp:sso',
        '<(DEPTH)/cobalt/subtlecrypto/subtlecrypto.gyp:subtlecrypto',
        '<(DEPTH)/cobalt/system_window/system_window.gyp:system_window',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:trace_event',
        '<(DEPTH)/cobalt/ui_navigation/ui_navigation.gyp:ui_navigation',
        '<(DEPTH)/cobalt/webdriver/webdriver.gyp:webdriver',
        '<(DEPTH)/cobalt/websocket/websocket.gyp:websocket',
        '<(DEPTH)/cobalt/xhr/xhr.gyp:xhr',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/nb/nb.gyp:nb',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/url/url.gyp:url',
        'browser_bindings.gyp:bindings',
        '<(cobalt_webapi_extension_gyp_target)',
      ],
      # This target doesn't generate any headers, but it exposes generated
      # header files (for idl dictionaries) through this module's public header
      # files. So mark this target as a hard dependency to ensure that any
      # dependent targets wait until this target (and its hard dependencies) are
      # built.
      #'hard_dependency': 1,
      'export_dependent_settings': [
        # Additionally, ensure that the include directories for generated
        # headers are put on the include directories for targets that depend
        # on this one.
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
      ],
      'include_dirs': [
        # For cobalt_build_id.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'conditions': [
        ['max_cobalt_cpu_usage != -1', {
          'defines': [ 'COBALT_MAX_CPU_USAGE_IN_BYTES' ],
        }],
        ['max_cobalt_gpu_usage != -1', {
          'defines': [ 'COBALT_MAX_GPU_USAGE_IN_BYTES' ],
        }],
        ['enable_about_scheme == 1', {
          'defines': [ 'ENABLE_ABOUT_SCHEME' ],
        }],
        ['enable_debugger == 1', {
          'sources': [
            'debug_console.cc',
            'debug_console.h',
            'lifecycle_console_commands.cc',
            'lifecycle_console_commands.h',
          ],
          'dependencies': [
            '<(DEPTH)/cobalt/debug/debug.gyp:debug',
          ],
        }],
        ['sb_evergreen == 1', {
          'dependencies': [
            '<(DEPTH)/cobalt/updater/updater.gyp:updater',
          ],
        }, {
          'dependencies': [
            '<@(cobalt_platform_dependencies)',
          ],
        }],
        ['"<(cobalt_webapi_extension_source_idl_files)"!="" or "<(cobalt_webapi_extension_generated_header_idl_files)"!=""', {
          'defines': [
            'COBALT_WEBAPI_EXTENSION_DEFINED',
          ],
        }],
      ],
    },

    {
      'target_name': 'browser_test',
      'type': '<(gtest_target_type)',
      'sources': [
        'device_authentication_test.cc',
        'storage_upgrade_handler_test.cc',
        'memory_settings/auto_mem_test.cc',
        'memory_settings/auto_mem_settings_test.cc',
        'memory_settings/calculations_test.cc',
        'memory_settings/memory_settings_test.cc',
        'memory_settings/pretty_print_test.cc',
        'memory_settings/table_printer_test.cc',
        'memory_settings/test_common.h',
        'memory_tracker/tool/tool_impl_test.cc',
        'memory_tracker/tool/util_test.cc',
        'user_agent_string_test.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cobalt/base/base.gyp:base',
        '<(DEPTH)/cobalt/dom/dom.gyp:dom',
        '<(DEPTH)/cobalt/network/network.gyp:network',
        '<(DEPTH)/cobalt/speech/speech.gyp:speech',
        '<(DEPTH)/cobalt/storage/storage.gyp:storage',
        '<(DEPTH)/cobalt/storage/storage.gyp:storage_upgrade_copy_test_data',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        'browser',
      ],
      'includes': [ '<(DEPTH)/cobalt/test/test.gypi' ],
    },

    {
      'target_name': 'browser_test_deploy',
      'type': 'none',
      'dependencies': [
        'browser_test',
      ],
      'variables': {
        'executable_name': 'browser_test',
      },
      'includes': [ '<(DEPTH)/starboard/build/deploy.gypi' ],
    },

  ],
}
