# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'common.gypi',
  ],
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
        '../third_party/codesighs/codesighs.gyp:*',
        '../third_party/ffmpeg/ffmpeg.gyp:*',
        '../third_party/icu38/icu38.gyp:*',
        '../third_party/libjpeg/libjpeg.gyp:*',
        '../third_party/libpng/libpng.gyp:*',
        '../third_party/libxml/libxml.gyp:*',
        '../third_party/libxslt/libxslt.gyp:*',
        '../third_party/lzma_sdk/lzma_sdk.gyp:*',
        '../third_party/modp_b64/modp_b64.gyp:*',
        '../third_party/npapi/npapi.gyp:*',
        '../third_party/sqlite/sqlite.gyp:*',
        '../third_party/zlib/zlib.gyp:*',
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
        ['OS=="linux"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../third_party/harfbuzz/harfbuzz.gyp:*',
            '../third_party/tcmalloc/tcmalloc.gyp:*',
            '../tools/gtk_clipboard_dump/gtk_clipboard_dump.gyp:*',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:*',
            '../courgette/courgette.gyp:*',
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'dependencies': [
                '../chrome/installer/installer.gyp:linux_packages',
              ],
            }],
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:*',
            '../chrome/app/locales/locales.gyp:*',
            '../courgette/courgette.gyp:*',
            '../gears/gears.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../third_party/bsdiff/bsdiff.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../third_party/cld/cld.gyp:*',
            '../third_party/tcmalloc/tcmalloc.gyp:*',
            '../tools/memory_watcher/memory_watcher.gyp:*',
            '../webkit/activex_shim/activex_shim.gyp:*',
            '../webkit/activex_shim_dll/activex_shim_dll.gyp:*',
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
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          # This target is legacy and can go away when the bots have been
          # updated to direclty build the test_shell targets.
          'target_name': 'build_for_layout_tests',
          'type': 'none',
          'dependencies': [
            '../webkit/tools/test_shell/test_shell.gyp:*',
          ],
        },
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
      ],
    }],
  ],
}
