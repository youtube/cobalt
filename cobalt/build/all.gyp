# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

# This file contains all targets that should be created by gyp_cobalt by
# default.

{
  'targets': [
    {
      'target_name': 'Default',
      'default_project': 1,
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/cobalt/browser/cobalt.gyp:cobalt',
      ],
    },

    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_unittests_deploy',
        '<(DEPTH)/cobalt/audio/audio.gyp:*',
        '<(DEPTH)/cobalt/audio/audio_test.gyp:*',
        '<(DEPTH)/cobalt/base/base.gyp:*',
        '<(DEPTH)/cobalt/bindings/testing/testing.gyp:*',
        '<(DEPTH)/cobalt/browser/browser.gyp:*',
        '<(DEPTH)/cobalt/browser/cobalt.gyp:*',
        '<(DEPTH)/cobalt/csp/csp.gyp:*',
        '<(DEPTH)/cobalt/css_parser/css_parser.gyp:*',
        '<(DEPTH)/cobalt/cssom/cssom.gyp:*',
        '<(DEPTH)/cobalt/cssom/cssom_test.gyp:*',
        '<(DEPTH)/cobalt/dom/dom.gyp:*',
        '<(DEPTH)/cobalt/dom/dom_test.gyp:*',
        '<(DEPTH)/cobalt/dom/testing/dom_testing.gyp:*',
        '<(DEPTH)/cobalt/dom_parser/dom_parser.gyp:*',
        '<(DEPTH)/cobalt/dom_parser/dom_parser_test.gyp:*',
        '<(DEPTH)/cobalt/encoding/encoding.gyp:*',
        '<(DEPTH)/cobalt/encoding/encoding_test.gyp:*',
        '<(DEPTH)/cobalt/extension/extension.gyp:*',
        '<(DEPTH)/cobalt/h5vcc/h5vcc.gyp:*',
        '<(DEPTH)/cobalt/input/input.gyp:*',
        '<(DEPTH)/cobalt/layout/layout.gyp:*',
        '<(DEPTH)/cobalt/layout_tests/layout_tests.gyp:*',
        '<(DEPTH)/cobalt/loader/image/sandbox/sandbox.gyp:*',
        '<(DEPTH)/cobalt/loader/loader.gyp:*',
        '<(DEPTH)/cobalt/loader/origin.gyp:*',
        '<(DEPTH)/cobalt/math/math.gyp:*',
        '<(DEPTH)/cobalt/media/media.gyp:*',
        '<(DEPTH)/cobalt/media/sandbox/sandbox.gyp:*',
        '<(DEPTH)/cobalt/media_capture/media_capture.gyp:*',
        '<(DEPTH)/cobalt/media_capture/media_capture_test.gyp:*',
        '<(DEPTH)/cobalt/media_session/media_session.gyp:*',
        '<(DEPTH)/cobalt/media_session/media_session_test.gyp:*',
        '<(DEPTH)/cobalt/media_stream/media_stream.gyp:*',
        '<(DEPTH)/cobalt/media_stream/media_stream_test.gyp:*',
        '<(DEPTH)/cobalt/network/network.gyp:*',
        '<(DEPTH)/cobalt/overlay_info/overlay_info.gyp:*',
        '<(DEPTH)/cobalt/render_tree/render_tree.gyp:*',
        '<(DEPTH)/cobalt/renderer/backend/backend.gyp:graphics_system_test_deploy',
        '<(DEPTH)/cobalt/renderer/renderer.gyp:*',
        '<(DEPTH)/cobalt/renderer/sandbox/sandbox.gyp:*',
        '<(DEPTH)/cobalt/samples/simple_example/simple_example.gyp:*',
        '<(DEPTH)/cobalt/script/engine.gyp:engine_shell',
        '<(DEPTH)/cobalt/script/script.gyp:*',
        '<(DEPTH)/cobalt/speech/sandbox/sandbox.gyp:*',
        '<(DEPTH)/cobalt/speech/speech.gyp:*',
        '<(DEPTH)/cobalt/storage/storage.gyp:*',
        '<(DEPTH)/cobalt/storage/store/store.gyp:*',
        '<(DEPTH)/cobalt/storage/store_upgrade/upgrade.gyp:*',
        '<(DEPTH)/cobalt/storage/store_upgrade/upgrade_tool.gyp:*',
        '<(DEPTH)/cobalt/trace_event/trace_event.gyp:*',
        '<(DEPTH)/cobalt/web_animations/web_animations.gyp:*',
        '<(DEPTH)/cobalt/webdriver/webdriver.gyp:*',
        '<(DEPTH)/cobalt/webdriver/webdriver_test.gyp:*',
        '<(DEPTH)/cobalt/websocket/websocket.gyp:*',
        '<(DEPTH)/cobalt/xhr/xhr.gyp:*',
        '<(DEPTH)/crypto/crypto.gyp:crypto_unittests_deploy',
        '<(DEPTH)/third_party/boringssl/boringssl_tool.gyp:*',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zip_unittests_deploy',
        '<(DEPTH)/net/net.gyp:net_unittests_deploy',
        '<(DEPTH)/sql/sql.gyp:sql_unittests_deploy',
        '<(DEPTH)/starboard/client_porting/cwrappers/cwrappers_test.gyp:cwrappers_test_deploy',
        '<(DEPTH)/starboard/common/common_test.gyp:common_test_deploy',
        '<(DEPTH)/starboard/elf_loader/elf_loader.gyp:elf_loader_test_deploy',
        '<(DEPTH)/starboard/loader_app/loader_app.gyp:loader_app_tests_deploy',
        '<(DEPTH)/starboard/nplb/nplb_evergreen_compat_tests/nplb_evergreen_compat_tests.gyp:nplb_evergreen_compat_tests_deploy',
      ],
      'conditions': [
        ['sb_evergreen != 1', {
          'dependencies': [
            '<!@pymod_do_main(starboard.optional.get_optional_tests -g -d <(DEPTH))',
          ],
        }],
        ['OS=="starboard"', {
          'dependencies': [
            '<(DEPTH)/nb/nb_test.gyp:nb_test_deploy',
            '<(DEPTH)/nb/nb_test.gyp:reuse_allocator_benchmark',
            '<(DEPTH)/starboard/starboard_all.gyp:starboard_all',
          ],
        }],
        ['sb_evergreen==1', {
          'dependencies': [
            '<(DEPTH)/cobalt/updater/one_app_only_sandbox.gyp:*',
            '<(DEPTH)/components/update_client/update_client.gyp:cobalt_slot_management_test_deploy',
            '<(DEPTH)/third_party/musl/musl.gyp:musl_unittests',
            '<(DEPTH)/starboard/loader_app/installation_manager.gyp:*',
          ],
        }],
        ['sb_evergreen_compatible==1', {
          'dependencies': [
            '<(DEPTH)/third_party/crashpad/handler/handler.gyp:crashpad_handler',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="starboard"', {
      'targets': [
        {
          # Subset of targets for a platform whose port is in progress.
          'target_name': 'in_progress_targets',
          'type': 'none',
          'dependencies': [
            '<(DEPTH)/cobalt/browser/cobalt.gyp:cobalt',
          ],
        },
      ],
    }],
  ],
}
