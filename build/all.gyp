# Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
        '../chrome/browser/sync/tools/sync_tools.gyp:*',
        '../chrome/chrome.gyp:*',
        '../gfx/gfx.gyp:*',
        '../gpu/gpu.gyp:*',
        '../gpu/demos/demos.gyp:*',
        '../ipc/ipc.gyp:*',
        '../jingle/jingle.gyp:*',
        '../media/media.gyp:*',
        '../net/net.gyp:*',
        '../ppapi/ppapi.gyp:*',
        '../printing/printing.gyp:*',
        '../sdch/sdch.gyp:*',
        '../skia/skia.gyp:*',
        '../testing/gmock.gyp:*',
        '../testing/gtest.gyp:*',
        '../third_party/bzip2/bzip2.gyp:*',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:*',
        '../third_party/cld/cld.gyp:*',
        '../third_party/codesighs/codesighs.gyp:*',
        '../third_party/ffmpeg/ffmpeg.gyp:*',
        '../third_party/iccjpeg/iccjpeg.gyp:*',
        '../third_party/icu/icu.gyp:*',
        '../third_party/libpng/libpng.gyp:*',
        '../third_party/libwebp/libwebp.gyp:*',
        '../third_party/libxml/libxml.gyp:*',
        '../third_party/libxslt/libxslt.gyp:*',
        '../third_party/lzma_sdk/lzma_sdk.gyp:*',
        '../third_party/mesa/mesa.gyp:*',
        '../third_party/modp_b64/modp_b64.gyp:*',
        '../third_party/npapi/npapi.gyp:*',
        '../third_party/ots/ots.gyp:*',
        '../third_party/qcms/qcms.gyp:*',
        '../third_party/sqlite/sqlite.gyp:*',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:*',
        '../third_party/zlib/zlib.gyp:*',
        '../webkit/support/webkit_support.gyp:*',
        '../webkit/webkit.gyp:*',
        'util/build_util.gyp:*',
        'temp_gyp/googleurl.gyp:*',
        '<(libjpeg_gyp_path):*',
      ],
      'conditions': [
        ['javascript_engine=="v8"', {
          'dependencies': [
            '../v8/tools/gyp/v8.gyp:*',
          ],
        }],
        ['OS=="mac" or OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../third_party/yasm/yasm.gyp:*#host',
           ],
        }],
        ['OS=="mac" or OS=="win"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:*',
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
                '../chrome/chrome.gyp:linux_packages_<(channel)',
              ],
            }],
          ],
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            '../tools/gtk_clipboard_dump/gtk_clipboard_dump.gyp:*',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:*',
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
            '../ceee/ceee.gyp:*',
            '../chrome_frame/chrome_frame.gyp:*',
            '../courgette/courgette.gyp:*',
            '../gears/gears.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../third_party/angle/src/build_angle.gyp:*',
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
        ['toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:*',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../chrome/browser/chromeos/input_method/candidate_window.gyp:*',
          ],
        }],
        ['remoting==1', {
          'dependencies': [
            '../remoting/remoting.gyp:*',
          ],
        }],
        ['use_openssl!=1', {
          'dependencies': [
            '../net/third_party/nss/ssl.gyp:*',
          ],
        }],
      ],
    }, # target_name: All
    {
      'target_name': 'chromium_builder_tests',
      'type': 'none',
      'dependencies': [
        '../app/app.gyp:app_unittests',
        '../base/base.gyp:base_unittests',
        '../chrome/chrome.gyp:browser_tests',
        '../chrome/chrome.gyp:interactive_ui_tests',
        '../chrome/chrome.gyp:nacl_ui_tests',
        '../chrome/chrome.gyp:nacl_sandbox_tests',
        '../chrome/chrome.gyp:safe_browsing_tests',
        '../chrome/chrome.gyp:sync_integration_tests',
        '../chrome/chrome.gyp:sync_unit_tests',
        '../chrome/chrome.gyp:ui_tests',
        '../chrome/chrome.gyp:unit_tests',
        '../gfx/gfx.gyp:gfx_unittests',
        '../gpu/gpu.gyp:gpu_unittests',
        '../ipc/ipc.gyp:ipc_tests',
        '../jingle/jingle.gyp:notifier_unit_tests',
        '../media/media.gyp:media_unittests',
        '../net/net.gyp:net_unittests',
        '../printing/printing.gyp:printing_unittests',
        '../remoting/remoting.gyp:remoting_unittests',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
        'temp_gyp/googleurl.gyp:googleurl_unittests',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:mini_installer_test',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_perftests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_reliability_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
            '../courgette/courgette.gyp:courgette_unittests',
            '../sandbox/sandbox.gyp:sbox_integration_tests',
            '../sandbox/sandbox.gyp:sbox_unittests',
            '../sandbox/sandbox.gyp:sbox_validation_tests',
            '../views/views.gyp:views_unittests',
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:copy_TestNetscapePlugIn',
            # TODO(nsylvain) ui_tests.exe depends on test_shell_common.
            # This should:
            # 1) not be the case. OR.
            # 2) be expressed in the ui tests dependencies.
            '../webkit/webkit.gyp:test_shell_common',
           ],
        }],
      ],
    }, # target_name: chromium_builder_tests
    {
      'target_name': 'chromium_builder_perf',
      'type': 'none',
      'dependencies': [
        '../chrome/chrome.gyp:memory_test',
        '../chrome/chrome.gyp:page_cycler_tests',
        '../chrome/chrome.gyp:plugin_tests',
        '../chrome/chrome.gyp:startup_tests',
        '../chrome/chrome.gyp:tab_switching_test',
        '../chrome/chrome.gyp:ui_tests', # needed for dromaeo, sunspider, v8
        '../chrome/chrome.gyp:url_fetch_test',
      ],
    }, # target_name: chromium_builder_perf
    {
      'target_name': 'chromium_gpu_builder',
      'type': 'none',
      'dependencies': [
        '../chrome/chrome.gyp:gpu_tests',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:DumpRenderTree',
      ],
    }
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
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:nacl_ui_tests',
            '../chrome/chrome.gyp:nacl_sandbox_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:sync_unit_tests',
            '../chrome/chrome.gyp:ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../gfx/gfx.gyp:gfx_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:notifier_unit_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
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
            '../chrome/chrome.gyp:nacl_ui_tests',
            '../chrome/chrome.gyp:nacl_sandbox_tests',
            '../chrome/chrome.gyp:page_cycler_tests',
            '../chrome/chrome.gyp:plugin_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:startup_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:sync_unit_tests',
            '../chrome/chrome.gyp:tab_switching_test',
            '../chrome/chrome.gyp:ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome/chrome.gyp:url_fetch_test',
            '../gfx/gfx.gyp:gfx_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:notifier_unit_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
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
            '../jingle/jingle.gyp:notifier_unit_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_valgrind_mac',
          'type': 'none',
          'dependencies': [
            '../app/app.gyp:app_unittests',
            '../base/base.gyp:base_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:sync_unit_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome/chrome.gyp:ui_tests',
            '../jingle/jingle.gyp:notifier_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
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
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:memory_test',
            '../chrome/chrome.gyp:mini_installer_test',
            '../chrome/chrome.gyp:nacl_ui_tests',
            '../chrome/chrome.gyp:nacl_sandbox_tests',
            '../chrome/chrome.gyp:page_cycler_tests',
            '../chrome/chrome.gyp:plugin_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:selenium_tests',
            '../chrome/chrome.gyp:startup_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:sync_unit_tests',
            '../chrome/chrome.gyp:tab_switching_test',
            '../chrome/chrome.gyp:ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome/chrome.gyp:url_fetch_test',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_perftests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_reliability_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
            '../courgette/courgette.gyp:courgette_unittests',
            '../gfx/gfx.gyp:gfx_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:notifier_unit_tests',
            '../media/media.gyp:media_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:copy_TestNetscapePlugIn',
            '../views/views.gyp:views_unittests',
            # TODO(nsylvain) ui_tests.exe depends on test_shell_common.
            # This should:
            # 1) not be the case. OR.
            # 2) be expressed in the ui tests dependencies.
            '../webkit/webkit.gyp:test_shell_common',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_win',
          'type': 'none',
          'dependencies': [
            '../app/app.gyp:app_unittests',
            # TODO(bradnelson): app_unittests should depend on locales.
            # However, we can't add dependencies on chrome/ to app/
            # See http://crbug.com/43603
            '../base/base.gyp:base_unittests',
            '../chrome/app/locales/locales.gyp:*',
            '../chrome/chrome.gyp:sync_unit_tests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:notifier_unit_tests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
      ],  # targets
      'conditions': [
        ['branding=="Chrome"', {
          'targets': [
            {
              'target_name': 'chrome_official_builder',
              'type': 'none',
              'dependencies': [
                '../chrome/app/locales/locales.gyp:*',
                '../chrome/chrome.gyp:crash_service',
                '../chrome/chrome.gyp:page_cycler_tests',
                '../chrome/chrome.gyp:policy_templates',
                '../chrome/chrome.gyp:pyautolib',
                '../chrome/chrome.gyp:reliability_tests',
                '../chrome/chrome.gyp:startup_tests',
                '../chrome/chrome.gyp:automated_ui_tests',
                '../chrome/installer/mini_installer.gyp:mini_installer',
                '../chrome_frame/chrome_frame.gyp:chrome_frame_unittests',
                '../chrome_frame/chrome_frame.gyp:npchrome_frame',
                '../courgette/courgette.gyp:courgette',
                '../third_party/adobe/flash/flash_player.gyp:flash_player',
                '../webkit/webkit.gyp:test_shell',
              ],
              'conditions': [
                ['internal_pdf', {
                  'dependencies': [
                    '../pdf/pdf.gyp:pdf',
                  ],
                }], # internal_pdf
              ]
            },
          ], # targets
        }], # branding=="Chrome"
       ], # conditions
    }], # OS="win"
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'chromeos_builder',
          'type': 'none',
          'sources': [
            # TODO(bradnelson): This is here to work around gyp issue 137.
            #     Remove this sources list when that issue has been fixed.
            'all.gyp',
          ],
          'dependencies': [
            '../app/app.gyp:app_unittests',
            '../base/base.gyp:base_unittests',
            '../chrome/browser/chromeos/input_method/candidate_window.gyp:candidate_window',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:memory_test',
            '../chrome/chrome.gyp:page_cycler_tests',
            '../chrome/chrome.gyp:safe_browsing_tests',
            '../chrome/chrome.gyp:startup_tests',
            '../chrome/chrome.gyp:sync_unit_tests',
            '../chrome/chrome.gyp:tab_switching_test',
            '../chrome/chrome.gyp:ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome/chrome.gyp:url_fetch_test',
            '../gfx/gfx.gyp:gfx_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:notifier_unit_tests',
            '../media/media.gyp:ffmpeg_tests',
            '../media/media.gyp:media_unittests',
            '../media/media.gyp:omx_test',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../views/views.gyp:views_unittests',
            'temp_gyp/googleurl.gyp:googleurl_unittests',
          ],
        },
      ],  # targets
    }], # "chromeos==1"
  ], # conditions
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
