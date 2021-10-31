# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#####################################################################
# If you modify this file, PLEASE REMEMBER TO UPDATE
# //cobalt/build/config/base.gni or //starboard/build/config/base.gni
# AS WELL
#####################################################################

# IMPORTANT:
# Please don't directly include this file if you are building via gyp_cobalt,
# since gyp_cobalt is automatically forcing its inclusion.
{
  # Variables expected to be overriden in the platform's gyp_configuration.gypi.
  'variables': {
    # Putting a variables dict inside another variables dict looks kind of
    # weird.  This is done so that 'host_arch', 'android_build_type', etc are
    # defined as variables within the outer variables dict here.  This is
    # necessary to get these variables defined for the conditions within this
    # variables dict that operate on these variables.
    'variables': {
      'variables': {
        'variables': {
          'host_arch%':
            'x64', # Building on any other arch is not supported
        },

        # Copy conditionally-set variables out one scope.
        'host_arch%': '<(host_arch)',

        # Default architecture we're building for is the architecture we're
        # building on.
        'target_arch%': '<(host_arch)',

        # Sets whether we're building with the Android SDK/NDK (and hence with
        # Ant, value 0), or as part of the Android system (and hence with the
        # Android build system, value 1).
        'android_build_type%': 0,
      },

      # Copy conditionally-set variables out one scope.
      'host_arch%': '<(host_arch)',
      'target_arch%': '<(target_arch)',
      'android_build_type%': '<(android_build_type)',

      # Set ARM-v7 compilation flags
      'armv7%': 0,

      # Set Neon compilation flags (only meaningful if armv7==1).
      'arm_neon%': 1,

      # The system root for cross-compiles. Default: none.
      'sysroot%': '',

      # Use system libjpeg. Note that the system's libjepg will be used even if
      # use_libjpeg_turbo is set.
      'use_system_libjpeg%': 0,

      # Variable 'component' is for cases where we would like to build some
      # components as dynamic shared libraries but still need variable
      # 'library' for static libraries.
      # By default, component is set to whatever library is set to and
      # it can be overriden by the GYP command line or by ~/.gyp/include.gypi.
      'component%': 'static_library',

      # Enable building with ASAN (Clang's -fsanitize=address option).
      # -fsanitize=address only works with clang, but asan=1 implies clang=1
      # See https://sites.google.com/a/chromium.org/dev/developers/testing/addresssanitizer
      'asan%': 0,

      # Enable building with TSAN (Clang's -fsanitize=thread option).
      # -fsanitize=thread only works with clang, but tsan=1 implies clang=1
      # See http://clang.llvm.org/docs/ThreadSanitizer.html
      'tsan%': 0,

      # Set this to true when building with Clang.
      # See http://code.google.com/p/chromium/wiki/Clang for details.
      'clang%': 0,

      'os_posix%': 0,
      'os_bsd%': 0,
    },

    # Make sure that cobalt is defined. This is needed in
    # the case where the code is built using Chromium stock
    # build tools.
    'cobalt%': 0,

    # Copy conditionally-set variables out one scope.
    'target_arch%': '<(target_arch)',
    'host_arch%': '<(host_arch)',
    'library%': 'static_library',
    'os_bsd%': '<(os_bsd)',
    'os_posix%': '<(os_posix)',
    'armv7%': '<(armv7)',
    'arm_neon%': '<(arm_neon)',
    'sysroot%': '<(sysroot)',
    'component%': '<(component)',
    'asan%': '<(asan)',
    'tsan%': '<(tsan)',
    'use_system_libjpeg%': '<(use_system_libjpeg)',
    'android_build_type%': '<(android_build_type)',

    # Use system protobuf instead of bundled one.
    'use_system_protobuf%': 0,

    # TODO(sgk): eliminate this if possible.
    # It would be nicer to support this via a setting in 'target_defaults'
    # in chrome/app/locales/locales.gypi overriding the setting in the
    # 'Debug' configuration in the 'target_defaults' dict below,
    # but that doesn't work as we'd like.
    'msvs_debug_link_incremental%': '2',

    # Clang stuff.
    'clang%': '<(clang)',
    'make_clang_dir%': 'third_party/llvm-build/Release+Asserts',

    # These two variables can be set in GYP_DEFINES while running
    # |gclient runhooks| to let clang run a plugin in every compilation.
    # Only has an effect if 'clang=1' is in GYP_DEFINES as well.
    # Example:
    #     GYP_DEFINES='clang=1 clang_load=/abs/path/to/libPrintFunctionNames.dylib clang_add_plugin=print-fns' gclient runhooks

    'clang_load%': '',
    'clang_add_plugin%': '',

    # Set Thumb compilation flags.
    'arm_thumb%': 0,

    # Set ARM fpu compilation flags (only meaningful if armv7==1 and
    # arm_neon==0).
    'arm_fpu%': 'vfpv3',

    # Set ARM float abi compilation flag.
    'arm_float_abi%': 'softfp',

    # .gyp files or targets should set chromium_code to 1 if they build
    # Chromium-specific code, as opposed to external code.  This variable is
    # used to control such things as the set of warnings to enable, and
    # whether warnings are treated as errors.
    'chromium_code%': 0,

    # Crashpad relies on excluding files in GYP based on platform specific
    # names, so it has its own set of filename_rules.gypi located under
    # "third_party/mini_chromium/build/".
    'crashpad_code%': 0,

    # TODO(thakis): Make this a blacklist instead, http://crbug.com/101600
    'enable_wexit_time_destructors%': 0,

    # Native Client is enabled by default.
    'disable_nacl%': 0,

    # Sets the default version name and code for Android app, by default we
    # do a developer build.
    'android_app_version_name%': 'Developer Build',
    'android_app_version_code%': 0,

    'windows_sdk_path%': 'C:/Program Files (x86)/Windows Kits/10',

    'conditions': [
      ['target_arch=="android"', {
        # Location of Android NDK.
        'variables': {
          'variables': {
            'variables': {
              'android_ndk_root%': '<!pymod_do_main(starboard.build.gyp_functions getenv ANDROID_NDK_ROOT)',
            },
            'android_ndk_root%': '<(android_ndk_root)',
            'conditions': [
              ['target_arch == "x86"', {
                'android_app_abi%': 'x86',
                'android_ndk_sysroot%': '<(android_ndk_root)/platforms/android-9/arch-x86',
              }],
              ['target_arch=="arm" or (OS=="lb_shell" and target_arch=="android")', {
                'android_ndk_sysroot%': '<(android_ndk_root)/platforms/android-9/arch-arm',
                'conditions': [
                  ['armv7==0', {
                    'android_app_abi%': 'armeabi',
                  }, {
                    'android_app_abi%': 'armeabi-v7a',
                  }],
                ],
              }],
            ],
          },
          'android_ndk_root%': '<(android_ndk_root)',
          'android_app_abi%': '<(android_app_abi)',
          'android_ndk_sysroot%': '<(android_ndk_sysroot)',
        },
        'android_ndk_root%': '<(android_ndk_root)',
        'android_ndk_sysroot': '<(android_ndk_sysroot)',
        'android_ndk_include': '<(android_ndk_sysroot)/usr/include',
        'android_ndk_lib': '<(android_ndk_sysroot)/usr/lib',
        'android_app_abi%': '<(android_app_abi)',

        # Location of the "strip" binary, used by both gyp and scripts.
        'android_strip%' : '<!(/bin/echo -n <(android_toolchain)/*-strip)',

        # Provides an absolute path to PRODUCT_DIR (e.g. out/Release). Used
        # to specify the output directory for Ant in the Android build.
        'ant_build_out': '`cd <(PRODUCT_DIR) && pwd -P`',

        # Disable Native Client.
        'disable_nacl%': 1,

        # When building as part of the Android system, use system libraries
        # where possible to reduce ROM size.
        # TODO(steveblock): Investigate using the system version of sqlite.
        'use_system_sqlite%': 0,  # '<(android_build_type)',
        'use_system_icu%': '<(android_build_type)',
        'use_system_stlport%': '<(android_build_type)',

        # Copy it out one scope.
        'android_build_type%': '<(android_build_type)',
      }],  # target_arch=="android"

      ['asan==1 and OS!="win"', {
        'clang%': 1,
      }],
      ['tsan==1', {
        'clang%': 1,
      }],
    ],
  },
  'target_defaults': {
    'variables': {
      # The condition that operates on chromium_code is in a target_conditions
      # section, and will not have access to the default fallback value of
      # chromium_code at the top of this file, or to the chromium_code
      # variable placed at the root variables scope of .gyp files, because
      # those variables are not set at target scope.  As a workaround,
      # if chromium_code is not set at target scope, define it in target scope
      # to contain whatever value it has during early variable expansion.
      # That's enough to make it available during target conditional
      # processing.
      'chromium_code%': '<(chromium_code)',

      # The crashpad_code variable operates the same way as described above for
      # the chromium_code variable.
      'crashpad_code%': '<(crashpad_code)',

      # See http://msdn.microsoft.com/en-us/library/aa652360(VS.71).aspx
      'win_release_Optimization%': '2', # 2 = /Os
      'win_debug_Optimization%': '0',   # 0 = /Od

      # See http://msdn.microsoft.com/en-us/library/2kxx5t2c(v=vs.80).aspx
      # Tri-state: blank is default, 1 on, 0 off
      'win_release_OmitFramePointers%': '0',
      # Tri-state: blank is default, 1 on, 0 off
      'win_debug_OmitFramePointers%': '',

      # See http://msdn.microsoft.com/en-us/library/8wtf2dfz(VS.71).aspx
      'win_debug_RuntimeChecks%': '3',    # 3 = all checks enabled, 0 = off

      # See http://msdn.microsoft.com/en-us/library/47238hez(VS.71).aspx
      'win_debug_InlineFunctionExpansion%': '',    # empty = default, 0 = off,
      'win_release_InlineFunctionExpansion%': '2', # 1 = only __inline, 2 = max

      # VS inserts quite a lot of extra checks to algorithms like
      # std::partial_sort in Debug build which make them O(N^2)
      # instead of O(N*logN). This is particularly slow under memory
      # tools like ThreadSanitizer so we want it to be disablable.
      # See http://msdn.microsoft.com/en-us/library/aa985982(v=VS.80).aspx
      'win_debug_disable_iterator_debugging%': '0',

      'release_extra_cflags%': '',
      'debug_extra_cflags%': '',

      # the non-qualified versions are widely assumed to be *nix-only
      'win_release_extra_cflags%': '',
      'win_debug_extra_cflags%': '',

      # TODO(thakis): Make this a blacklist instead, http://crbug.com/101600
      'enable_wexit_time_destructors%': '<(enable_wexit_time_destructors)',

      # Only used by Windows build for now.  Can be used to build into a
      # differet output directory, e.g., a build_dir_prefix of VS2010_ would
      # output files in src/build/VS2010_{Debug,Release}.
      'build_dir_prefix%': '',

      # Targets are by default not nacl untrusted code.
      'nacl_untrusted_build%': 0,

      'pnacl_compile_flags': [
        # pnacl uses the clang compiler so we need to supress all the
        # same warnings as we do for clang.
        # TODO(sbc): Remove these if/when they are removed from the clang
        # build.
        '-Wno-unused-function',
        '-Wno-char-subscripts',
        '-Wno-unnamed-type-template-args',
      ],

      # See http://msdn.microsoft.com/en-us/library/aa652367.aspx
      'win_release_RuntimeLibrary%': '0', # 0 = /MT (nondebug static)
      'win_debug_RuntimeLibrary%': '1',   # 1 = /MTd (debug static)

      # See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Optimize-Options.html
      'mac_release_optimization%': '3', # Use -O3 unless overridden
      'mac_debug_optimization%': '0',   # Use -O0 unless overridden
    },
    'defines': [
      'USE_OPENSSL=1',
    ],
    'conditions': [
      ['component=="shared_library"', {
        'defines': ['COMPONENT_BUILD'],
      }],
    ],  # conditions for 'target_defaults'
    'target_conditions': [
      ['enable_wexit_time_destructors==1', {
        'conditions': [
          [ 'clang==1', {
            'cflags': [
              '-Wexit-time-destructors',
            ],
            'xcode_settings': {
              'WARNING_CFLAGS': [
                '-Wexit-time-destructors',
              ],
            },
          }],
        ],
      }],
      ['chromium_code!=0', {
        'includes': [
           # Rules for excluding e.g. foo_win.cc from the build on non-Windows.
          'filename_rules.gypi',
        ],
      }],
      ['crashpad_code!=0', {
        'includes': [
           # Rules for excluding e.g. foo_win.cc from the build on non-Windows.
          '<(DEPTH)/third_party/mini_chromium/build/filename_rules.gypi',
        ],
      }],
      ['chromium_code!=0 or crashpad_code!=0', {
        # In Chromium code, we define __STDC_foo_MACROS in order to get the
        # C99 macros on Mac and Linux.
        'defines': [
          '__STDC_CONSTANT_MACROS',
          '__STDC_FORMAT_MACROS',
        ],
        'conditions': [
          ['(OS == "win" or target_arch=="xb1") and component=="shared_library"', {
            'msvs_disabled_warnings': [
              4251,  # class 'std::xx' needs to have dll-interface.
              4715,  # Not all control paths return a value.
            ],
          }],
        ],
      }],
    ],  # target_conditions for 'target_defaults'
  },
}
