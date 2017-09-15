# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
{
  'includes': [
    '../WTF.gypi',
  ],
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "lb_shell" and gcc_version>=46', {
      'target_defaults': {
        # Disable warnings about c++0x compatibility, as some names (such as nullptr) conflict
        # with upcoming c++0x types.
        'cflags_cc': ['-Wno-c++0x-compat'],
      },
    }],
  ],
  'targets': [
    {
      # This target sets up defines and includes that are required by WTF and
      # its dependents.
      'target_name': 'wtf_config',
      'type': 'none',
      'direct_dependent_settings': {
        'conditions': [
          ['OS=="win"', {
            'defines': [
              '__STD_C',
              '_CRT_SECURE_NO_DEPRECATE',
              '_SCL_SECURE_NO_DEPRECATE',
              'CRASH=__debugbreak',
            ],
            'include_dirs': [
              '../../JavaScriptCore/os-win32',
            ],
          }],
          ['OS=="mac"', {
            'defines': [
              'WTF_USE_NEW_THEME=1',
            ],
          }],
          ['os_posix == 1 and OS != "mac"', {
            'defines': [
              'WTF_USE_PTHREADS=1',
            ],
          }],
        ],
      },
    },
    {
      'target_name': 'wtf',
      'type': 'static_library',
      'include_dirs': [
        '../',
        '../wtf',
        '../wtf/unicode',
      ],
      'dependencies': [
          'wtf_config',
          '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
          '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
      'sources': [
        '<@(wtf_privateheader_files)',
        '<@(wtf_files)',
      ],
      'sources/': [
        # FIXME: This is clearly not sustainable.
        ['exclude', '../wtf/efl'],
        ['exclude', '../wtf/gobject'],
        ['exclude', '../wtf/gtk'],
        ['exclude', '../wtf/mac'],
        ['exclude', '../wtf/qt'],
        ['exclude', '../wtf/url'],
        ['exclude', '../wtf/wince'],
        ['exclude', '../wtf/wx'],
        ['exclude', '../wtf/unicode/glib'],
        ['exclude', '../wtf/unicode/qt4'],
        ['exclude', '../wtf/unicode/wchar'],
        # GLib/GTK, even though its name doesn't really indicate.
        ['exclude', '/(gtk|glib|gobject)/.*\\.(cpp|h)$'],
        ['exclude', '(Default|Gtk|Mac|None|Qt|Win|Wx|Efl)\\.(cpp|mm)$'],
        ['exclude', 'wtf/MainThread.cpp$'],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../',
          # FIXME: This is too broad, but is needed for runtime/JSExportMacros.h and yarr.
          '../../JavaScriptCore',
        ],
        # Some warnings occur in JSC headers, so they must also be disabled
        # in targets that use JSC.
        'msvs_disabled_warnings': [
          # Don't complain about calling specific versions of templatized
          # functions (e.g. in RefPtrHashMap.h).
          4344,
          # Don't complain about using "this" in an initializer list
          # (e.g. in StringImpl.h).
          4355,
          # This warning is generated when a value that is not bool is assigned
          # or coerced into type bool. Returning integer values from boolean
          # functions is a common practice in WebKit.
          4800,
        ],
      },
      'export_dependent_settings': [
        'wtf_config',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
      'msvs_disabled_warnings': [
        # The compiler ignored an unrecognized pragma.
        4068,
        # The controlling expression of an if statement or while loop evaluates
        # to a constant.
        4127,
        # 'this' used in base member initializer list.
        4355,
        # A specialization of a function template cannot specify any
        # of the inline specifiers. MSVC compiler mistakenly emits this warning
        # in places where template friend functions are declared.
        4396,
        # This warning is generated when a value that is not bool is assigned
        # or coerced into type bool. Returning integer values from boolean
        # functions is a common practice in WebKit.
        4800,
      ],
      'conditions': [
        ['OS=="win"', {
          'sources/': [
            ['exclude', 'ThreadIdentifierDataPthreads\\.(h|cpp)$'],
            ['exclude', 'ThreadingPthreads\\.cpp$'],
            ['include', 'Thread(ing|Specific)Win\\.cpp$'],
            ['exclude', 'OSAllocatorPosix\\.cpp$'],
            ['include', 'OSAllocatorWin\\.cpp$'],
            ['include', 'win/OwnPtrWin\\.cpp$'],
          ],
          'include_dirs!': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'conditions': [
            ['inside_chromium_build==1 and component=="shared_library"', {
              # Chromium windows multi-dll build enables c++ exception and this
              # causes wtf generates 4291 warning due to operator new/delete
              # implementations. Disable the warning for chromium windows
              # multi-dll build.
              'msvs_disabled_warnings': [4291],
              'direct_dependent_settings': {
                'msvs_disabled_warnings': [4291],
              },
            }],
          ],
        }],
        ['OS=="lb_shell"', {
          'sources': [
            '../wtf/OSAllocatorShell.cpp',
            '<(lbshell_root)/src/platform/<(target_arch)/wtf/StackBoundsShell.cpp',
          ],
          'sources/': [
            ['exclude', 'OSAllocatorPosix\\.cpp$'],
          ],
        }],
        ['OS=="starboard"', {
          'sources': [
            '../wtf/OSAllocatorStarboard.cpp',
            '../wtf/ThreadingStarboard.cpp',
            '../wtf/ThreadIdentifierDataStarboard.cpp',
          ],
          'sources/': [
            ['exclude', 'OSAllocatorPosix\\.cpp$'],
            ['exclude', 'ThreadIdentifierDataPthreads\\.cpp$'],
            ['exclude', 'ThreadingPthreads\\.cpp$'],
          ],
        }],
        ['cobalt==1', {
          'sources/': [
            ['exclude', 'wtf/chromium'],
          ],
          'defines': [
            # See wtf/ExportMacros.h.
            'BUILDING_WTF',
          ],
          'defines!': [
            '__DISABLE_WTF_LOGGING__',
          ],
        }],
        ['OS!="mac"', {
          'sources/': [
            # mac is the only OS that uses WebKit's copy of TCMalloc.
          ],
        }],
      ],
    },
  ]
}
