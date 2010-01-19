# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'xcode_create_dependents_test_runner': 1,
      'dependencies': [
        '../app/app.gyp:*',
        '../base/base.gyp:*',
        '../chrome/chrome.gyp:*',
        '../ipc/ipc.gyp:*',
        '../media/media.gyp:*',
        '../net/net.gyp:*',
        '../printing/printing.gyp:*',
        '../sdch/sdch.gyp:*',
        '../skia/skia.gyp:*',
        '../testing/gmock.gyp:*',
        '../testing/gtest.gyp:*',
        '../third_party/bzip2/bzip2.gyp:*',
        '../third_party/cld/cld.gyp:*',
        '../third_party/codesighs/codesighs.gyp:*',
        '../third_party/ffmpeg/ffmpeg.gyp:*',
        '../third_party/icu/icu.gyp:*',
        '../third_party/libjpeg/libjpeg.gyp:*',
        '../third_party/libpng/libpng.gyp:*',
        '../third_party/libxml/libxml.gyp:*',
        '../third_party/libxslt/libxslt.gyp:*',
        '../third_party/lzma_sdk/lzma_sdk.gyp:*',
        '../third_party/modp_b64/modp_b64.gyp:*',
        '../third_party/npapi/npapi.gyp:*',
        '../third_party/ots/ots.gyp:*',
        '../third_party/sqlite/sqlite.gyp:*',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:*',
        '../third_party/zlib/zlib.gyp:*',
        '../webkit/tools/pepper_test_plugin/pepper_test_plugin.gyp:*',
        '../webkit/tools/test_shell/test_shell.gyp:*',
        '../webkit/webkit.gyp:*',
        'util/build_util.gyp:*',
        'temp_gyp/googleurl.gyp:*',
      ],
      'conditions': [
        ['javascript_engine=="v8"', {
          'dependencies': [
            '../v8/tools/gyp/v8.gyp:*',
          ],
        }],
        ['OS=="mac" or OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../third_party/yasm/yasm.gyp:*#host',
           ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../third_party/ocmock/ocmock.gyp:*',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:*',
            '../courgette/courgette.gyp:*',
            '../sandbox/sandbox.gyp:*',
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'dependencies': [
                '../chrome/installer/installer.gyp:linux_packages_<(channel)',
              ],
            }],
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../tools/gtk_clipboard_dump/gtk_clipboard_dump.gyp:*',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:*',
            '../net/third_party/nss/nss.gyp:*',
          ],
        }],
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:*',
              ],
            }],
          ],
          'dependencies': [
            '../breakpad/breakpad.gyp:*',
            '../chrome/app/locales/locales.gyp:*',
            '../chrome_frame/chrome_frame.gyp:*',
            '../courgette/courgette.gyp:*',
            '../gears/gears.gyp:*',
            '../gpu/demos/demos.gyp:*',
            '../gpu/gpu.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../third_party/bsdiff/bsdiff.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../third_party/gles2_book/gles2_book.gyp:*',
            '../tools/memory_watcher/memory_watcher.gyp:*',
          ],
        }, {
          'dependencies': [
            '../third_party/libevent/libevent.gyp:*',
          ],
        }],
        ['OS=="win" or (OS=="linux" and toolkit_views==1)', {
          'dependencies': [
            '../views/views.gyp:*',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../third_party/chromeos_text_input/text_input.gyp:*',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          # Target to build everything plus the dmg.  We don't put the dmg
          # in the All target because developers really don't need it.
          'target_name': 'all_and_dmg',
          'type': 'none',
          'dependencies': [
            'All',
            '../chrome/chrome.gyp:build_app_dmg',
          ],
        },
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder_dbg',
          'type': 'none',
          'dependencies': [
            '../app/app.gyp:app_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_rel',
          'type': 'none',
          'dependencies': [
            '../app/app.gyp:app_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:memory_test',
            '../chrome/chrome.gyp:page_cycler_tests',
            '../chrome/chrome.gyp:startup_tests',
            '../chrome/chrome.gyp:tab_switching_test',
            '../chrome/chrome.gyp:ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome/chrome.gyp:url_fetch_test',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_mac',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
            '../net/net.gyp:net_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
          ],
        },
      ],  # targets
    }], # OS="mac"
    ['OS=="win"', {
      'targets': [
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder',
          'type': 'none',
          'dependencies': [
            '../app/app.gyp:app_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:memory_test',
            '../chrome/chrome.gyp:nacl_ui_tests',
            '../chrome/chrome.gyp:page_cycler_tests',
            '../chrome/chrome.gyp:plugin_tests',
            '../chrome/chrome.gyp:startup_tests',
            '../chrome/chrome.gyp:sync_unit_tests',
            '../chrome/chrome.gyp:tab_switching_test',
            '../chrome/chrome.gyp:ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome/chrome.gyp:url_fetch_test',
            '../chrome/installer/installer.gyp:installer_util_unittests',
            '../chrome/installer/installer.gyp:mini_installer_test',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome/installer/mini_installer.gyp:chrome_frame_mini_installer',
            '../courgette/courgette.gyp:courgette_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../webkit/tools/test_shell/test_shell.gyp:npapi_layout_test_plugin',
            # TODO(nsylvain) ui_tests.exe depends on test_shell_common.
            # This should:
            # 1) not be the case. OR.
            # 2) be expressed in the ui tests dependencies.
            '../webkit/tools/test_shell/test_shell.gyp:test_shell_common',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_perftests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
          ],
        },
        {
          # TODO(robertshield): Remove this target once the last independent
          # CF builder is retired.
          'target_name': 'chrome_frame_builder',
          'type': 'none',
          'dependencies': [
            '../chrome/installer/installer.gyp:installer_util_unittests',
            '../chrome/installer/installer.gyp:mini_installer_test',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome/installer/mini_installer.gyp:chrome_frame_mini_installer',
            '../courgette/courgette.gyp:courgette_unittests',
            '../chrome/chrome.gyp:chrome',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_perftests',
          ],
        },
      ],  # targets
    }], # OS="win"
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
