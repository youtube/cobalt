# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# IMPORTANT:
# Please don't directly include this file if you are building via gyp_chromium,
# since gyp_chromium is automatically forcing its inclusion.
{
  # Variables expected to be overriden on the GYP command line (-D) or by
  # ~/.gyp/include.gypi.
  'variables': {
    # Putting a variables dict inside another variables dict looks kind of
    # weird.  This is done so that 'host_arch', 'chromeos', etc are defined as
    # variables within the outer variables dict here.  This is necessary
    # to get these variables defined for the conditions within this variables
    # dict that operate on these variables (e.g., for setting 'toolkit_views',
    # we need to have 'chromeos' already set).
    'variables': {
      'variables': {
        'variables': {
          # Whether we're building a ChromeOS build.
          'chromeos%': 0,

          # Disable touch support by default.
          'touchui%': 0,
        },
        # Copy conditionally-set variables out one scope.
        'chromeos%': '<(chromeos)',
        'touchui%': '<(touchui)',

        # To do a shared build on linux we need to be able to choose between
        # type static_library and shared_library. We default to doing a static
        # build but you can override this with "gyp -Dlibrary=shared_library"
        # or you can add the following line (without the #) to
        # ~/.gyp/include.gypi {'variables': {'library': 'shared_library'}}
        # to compile as shared by default
        'library%': 'static_library',

        # Compute the architecture that we're building on.
        'conditions': [
          [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
            # This handles the Linux platforms we generally deal with. Anything
            # else gets passed through, which probably won't work very well; such
            # hosts should pass an explicit target_arch to gyp.
            'host_arch%':
              '<!(uname -m | sed -e "s/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/")',
          }, {  # OS!="linux"
            'host_arch%': 'ia32',
          }],

          # Set default value of toolkit_views on for Windows, Chrome OS
          # and the touch UI.
          ['OS=="win" or chromeos==1 or touchui==1', {
            'toolkit_views%': 1,
          }, {
            'toolkit_views%': 0,
          }],
        ],
      },

      # Copy conditionally-set variables out one scope.
      'chromeos%': '<(chromeos)',
      'touchui%': '<(touchui)',
      'host_arch%': '<(host_arch)',
      'library%': '<(library)',
      'toolkit_views%': '<(toolkit_views)',

      # Override branding to select the desired branding flavor.
      'branding%': 'Chromium',

      # Override buildtype to select the desired build flavor.
      # Dev - everyday build for development/testing
      # Official - release build (generally implies additional processing)
      # TODO(mmoss) Once 'buildtype' is fully supported (e.g. Windows gyp
      # conversion is done), some of the things which are now controlled by
      # 'branding', such as symbol generation, will need to be refactored based
      # on 'buildtype' (i.e. we don't care about saving symbols for non-Official
      # builds).
      'buildtype%': 'Dev',

      # Default architecture we're building for is the architecture we're
      # building on.
      'target_arch%': '<(host_arch)',

      # This variable tells WebCore.gyp and JavaScriptCore.gyp whether they are
      # are built under a chromium full build (1) or a webkit.org chromium
      # build (0).
      'inside_chromium_build%': 1,

      # Set to 1 to enable fast builds. It disables debug info for fastest
      # compilation.
      'fastbuild%': 0,

      # Python version.
      'python_ver%': '2.5',

      # Set ARM-v7 compilation flags
      'armv7%': 0,

      # Set Neon compilation flags (only meaningful if armv7==1).
      'arm_neon%': 1,

      # The system root for cross-compiles. Default: none.
      'sysroot%': '',

      # On Linux, we build with sse2 for Chromium builds.
      'disable_sse2%': 0,

      # Use libjpeg-turbo as the JPEG codec used by Chromium.
      'use_libjpeg_turbo%': 0,

      # Variable 'component' is for cases where we would like to build some
      # components as dynamic shared libraries but still need variable
      # 'library' for static libraries.
      # By default, component is set to whatever library is set to and
      # it can be overriden by the GYP command line or by ~/.gyp/include.gypi.
      'component%': '<(library)',

      # Set to select the Title Case versions of strings in GRD files.
      'use_titlecase_in_grd_files%': 0,

      # Use translations provided by volunteers at launchpad.net.  This
      # currently only works on Linux.
      'use_third_party_translations%': 0,

      # Remoting compilation is enabled by default. Set to 0 to disable.
      'remoting%': 1,

      'conditions': [
        # A flag to enable or disable our compile-time dependency
        # on gnome-keyring. If that dependency is disabled, no gnome-keyring
        # support will be available. This option is useful
        # for Linux distributions.
        ['chromeos==1', {
          'use_gnome_keyring%': 0,
        }, {
          'use_gnome_keyring%': 1,
        }],

        # Set to 1 compile with -fPIC cflag on linux. This is a must for shared
        # libraries on linux x86-64 and arm.
        ['host_arch=="ia32"', {
          'linux_fpic%': 0,
        }, {
          'linux_fpic%': 1,
        }],

        ['toolkit_views==0 or OS=="mac"', {
          # GTK+ and Mac wants Title Case strings
          'use_titlecase_in_grd_files%': 1,
        }],

        # Enable some hacks to support Flapper only on Chrome OS.
        ['chromeos==1', {
          'enable_flapper_hacks%': 1,
        }, {
          'enable_flapper_hacks%': 0,
        }],
      ],
    },

    # Copy conditionally-set variables out one scope.
    'branding%': '<(branding)',
    'buildtype%': '<(buildtype)',
    'target_arch%': '<(target_arch)',
    'host_arch%': '<(host_arch)',
    'toolkit_views%': '<(toolkit_views)',
    'use_gnome_keyring%': '<(use_gnome_keyring)',
    'linux_fpic%': '<(linux_fpic)',
    'enable_flapper_hacks%': '<(enable_flapper_hacks)',
    'chromeos%': '<(chromeos)',
    'touchui%': '<(touchui)',
    'inside_chromium_build%': '<(inside_chromium_build)',
    'fastbuild%': '<(fastbuild)',
    'python_ver%': '<(python_ver)',
    'armv7%': '<(armv7)',
    'arm_neon%': '<(arm_neon)',
    'sysroot%': '<(sysroot)',
    'disable_sse2%': '<(disable_sse2)',
    'library%': '<(library)',
    'component%': '<(component)',
    'use_titlecase_in_grd_files%': '<(use_titlecase_in_grd_files)',
    'remoting%': '<(remoting)',

    # The release channel that this build targets. This is used to restrict
    # channel-specific build options, like which installer packages to create.
    # The default is 'all', which does no channel-specific filtering.
    'channel%': 'all',

    # Override chromium_mac_pch and set it to 0 to suppress the use of
    # precompiled headers on the Mac.  Prefix header injection may still be
    # used, but prefix headers will not be precompiled.  This is useful when
    # using distcc to distribute a build to compile slaves that don't
    # share the same compiler executable as the system driving the compilation,
    # because precompiled headers rely on pointers into a specific compiler
    # executable's image.  Setting this to 0 is needed to use an experimental
    # Linux-Mac cross compiler distcc farm.
    'chromium_mac_pch%': 1,

    # Mac OS X SDK and deployment target support.
    # The SDK identifies the version of the system headers that will be used,
    # and corresponds to the MAC_OS_X_VERSION_MAX_ALLOWED compile-time macro.
    # "Maximum allowed" refers to the operating system version whose APIs are
    # available in the headers.
    # The deployment target identifies the minimum system version that the
    # built products are expected to function on.  It corresponds to the
    # MAC_OS_X_VERSION_MIN_REQUIRED compile-time macro.
    # To ensure these macros are available, #include <AvailabilityMacros.h>.
    # Additional documentation on these macros is available at
    # http://developer.apple.com/mac/library/technotes/tn2002/tn2064.html#SECTION3
    # Chrome normally builds with the Mac OS X 10.5 SDK and sets the
    # deployment target to 10.5.  Other projects, such as O3D, may override
    # these defaults.
    'mac_sdk%': '10.5',
    'mac_deployment_target%': '10.5',

    # Set to 1 to enable code coverage.  In addition to build changes
    # (e.g. extra CFLAGS), also creates a new target in the src/chrome
    # project file called "coverage".
    # Currently ignored on Windows.
    'coverage%': 0,

    # Overridable specification for potential use of alternative
    # JavaScript engines.
    'javascript_engine%': 'v8',

    # Although base/allocator lets you select a heap library via an
    # environment variable, the libcmt shim it uses sometimes gets in
    # the way.  To disable it entirely, and switch to normal msvcrt, do e.g.
    #  'win_use_allocator_shim': 0,
    #  'win_release_RuntimeLibrary': 2
    # to ~/.gyp/include.gypi, gclient runhooks --force, and do a release build.
    'win_use_allocator_shim%': 1, # 1 = shim allocator via libcmt; 0 = msvcrt

    # Whether usage of OpenMAX is enabled.
    'enable_openmax%': 0,

    # Whether proprietary audio/video codecs are assumed to be included with
    # this build (only meaningful if branding!=Chrome).
    'proprietary_codecs%': 0,

    # TODO(bradnelson): eliminate this when possible.
    # To allow local gyp files to prevent release.vsprops from being included.
    # Yes(1) means include release.vsprops.
    # Once all vsprops settings are migrated into gyp, this can go away.
    'msvs_use_common_release%': 1,

    # TODO(bradnelson): eliminate this when possible.
    # To allow local gyp files to override additional linker options for msvs.
    # Yes(1) means set use the common linker options.
    'msvs_use_common_linker_extras%': 1,

    # TODO(sgk): eliminate this if possible.
    # It would be nicer to support this via a setting in 'target_defaults'
    # in chrome/app/locales/locales.gypi overriding the setting in the
    # 'Debug' configuration in the 'target_defaults' dict below,
    # but that doesn't work as we'd like.
    'msvs_debug_link_incremental%': '2',

    # Needed for some of the largest modules.
    'msvs_debug_link_nonincremental%': '1',

    # This is the location of the sandbox binary. Chrome looks for this before
    # running the zygote process. If found, and SUID, it will be used to
    # sandbox the zygote process and, thus, all renderer processes.
    'linux_sandbox_path%': '',

    # Set this to true to enable SELinux support.
    'selinux%': 0,

    # Set this to true when building with Clang.
    # See http://code.google.com/p/chromium/wiki/Clang for details.
    # TODO: eventually clang should behave identically to gcc, and this
    # won't be necessary.
    'clang%': 0,

    # These two variables can be set in GYP_DEFINES while running
    # |gclient runhooks| to let clang run a plugin in every compilation.
    # Only has an effect if 'clang=1' is in GYP_DEFINES as well.
    # Example:
    #     GYP_DEFINES='clang=1 clang_load=/abs/path/to/libPrintFunctionNames.dylib clang_add_plugin=print-fns' gclient runhooks

    'clang_load%': '',
    'clang_add_plugin%': '',

    # Enable sampling based profiler.
    # See http://google-perftools.googlecode.com/svn/trunk/doc/cpuprofile.html
    'profiling%': '0',

    # Override whether we should use Breakpad on Linux. I.e. for Chrome bot.
    'linux_breakpad%': 0,
    # And if we want to dump symbols for Breakpad-enabled builds.
    'linux_dump_symbols%': 0,
    # And if we want to strip the binary after dumping symbols.
    'linux_strip_binary%': 0,
    # Strip the test binaries needed for Linux reliability tests.
    'linux_strip_reliability_tests%': 0,

    # Enable TCMalloc.
    'linux_use_tcmalloc%': 1,

    # Disable TCMalloc's debugallocation.
    'linux_use_debugallocation%': 0,

    # Disable TCMalloc's heapchecker.
    'linux_use_heapchecker%': 0,

    # Disable shadow stack keeping used by heapcheck to unwind the stacks
    # better.
    'linux_keep_shadow_stacks%': 0,

    # Set to 1 to turn on seccomp sandbox by default.
    # (Note: this is ignored for official builds.)
    'linux_use_seccomp_sandbox%': 0,

    # Set to 1 to link against libgnome-keyring instead of using dlopen().
    'linux_link_gnome_keyring%': 0,

    # Used to disable Native Client at compile time, for platforms where it
    # isn't supported
    'disable_nacl%': 0,

    # Set Thumb compilation flags.
    'arm_thumb%': 0,

    # Set ARM fpu compilation flags (only meaningful if armv7==1 and
    # arm_neon==0).
    'arm_fpu%': 'vfpv3',

    # Enable new NPDevice API.
    'enable_new_npdevice_api%': 0,

    # Enable EGLImage support in OpenMAX
    'enable_eglimage%': 0,

    # Enable a variable used elsewhere throughout the GYP files to determine
    # whether to compile in the sources for the GPU plugin / process.
    'enable_gpu%': 1,

    # Use OpenSSL instead of NSS. Under development: see http://crbug.com/62803
    'use_openssl%': 0,

    # .gyp files or targets should set chromium_code to 1 if they build
    # Chromium-specific code, as opposed to external code.  This variable is
    # used to control such things as the set of warnings to enable, and
    # whether warnings are treated as errors.
    'chromium_code%': 0,

    # Set to 1 to compile with the built in pdf viewer.
    'internal_pdf%': 0,

    # This allows to use libcros from the current system, ie. /usr/lib/
    # The cros_api will be pulled in as a static library, and all headers
    # from the system include dirs.
    'system_libcros%': 0,

    # NOTE: When these end up in the Mac bundle, we need to replace '-' for '_'
    # so Cocoa is happy (http://crbug.com/20441).
    'locales': [
      'am', 'ar', 'bg', 'bn', 'ca', 'cs', 'da', 'de', 'el', 'en-GB',
      'en-US', 'es-419', 'es', 'et', 'fa', 'fi', 'fil', 'fr', 'gu', 'he',
      'hi', 'hr', 'hu', 'id', 'it', 'ja', 'kn', 'ko', 'lt', 'lv',
      'ml', 'mr', 'nb', 'nl', 'pl', 'pt-BR', 'pt-PT', 'ro', 'ru',
      'sk', 'sl', 'sr', 'sv', 'sw', 'ta', 'te', 'th', 'tr', 'uk',
      'vi', 'zh-CN', 'zh-TW',
    ],

    'grit_defines': [],

    # Use Harfbuzz-NG instead of Harfbuzz.
    # Under development: http://crbug.com/68551
    'use_harfbuzz_ng%': 0,

    'conditions': [
      ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
        # This will set gcc_version to XY if you are running gcc X.Y.*.
        # This is used to tweak build flags for gcc 4.4.
        'gcc_version%': '<!(python <(DEPTH)/build/compiler_version.py)',
        # Figure out the python architecture to decide if we build pyauto.
        'python_arch%': '<!(<(DEPTH)/build/linux/python_arch.sh <(sysroot)/usr/lib/libpython<(python_ver).so.1.0)',
        'conditions': [
          ['branding=="Chrome"', {
            'linux_breakpad%': 1,
          }],
          # All Chrome builds have breakpad symbols, but only process the
          # symbols from official builds.
          ['(branding=="Chrome" and buildtype=="Official")', {
            'linux_dump_symbols%': 1,
          }],
        ],
      }],  # OS=="linux" or OS=="freebsd" or OS=="openbsd"

      ['OS=="mac"', {
        'conditions': [
          # mac_product_name is set to the name of the .app bundle as it should
          # appear on disk.  This duplicates data from
          # chrome/app/theme/chromium/BRANDING and
          # chrome/app/theme/google_chrome/BRANDING, but is necessary to get
          # these names into the build system.
          ['branding=="Chrome"', {
            'mac_product_name%': 'Google Chrome',
          }, { # else: branding!="Chrome"
            'mac_product_name%': 'Chromium',
          }],

          # Feature variables for enabling Mac Breakpad and Keystone auto-update
          # support.  Both features are on by default in official builds with
          # Chrome branding.
          ['branding=="Chrome" and buildtype=="Official"', {
            'mac_breakpad%': 1,
            'mac_keystone%': 1,
          }, { # else: branding!="Chrome" or buildtype!="Official"
            'mac_breakpad%': 0,
            'mac_keystone%': 0,
          }],
        ],
      }],  # OS=="mac"

      # Whether to use multiple cores to compile with visual studio. This is
      # optional because it sometimes causes corruption on VS 2005.
      # It is on by default on VS 2008 and off on VS 2005.
      ['OS=="win"', {
        'conditions': [
          ['component=="shared_library"', {
            'win_use_allocator_shim%': 0,
          }],
          ['MSVS_VERSION=="2005"', {
            'msvs_multi_core_compile%': 0,
          },{
            'msvs_multi_core_compile%': 1,
          }],
          # Don't do incremental linking for large modules on 32-bit.
          ['MSVS_OS_BITS==32', {
            'msvs_large_module_debug_link_mode%': '1',  # No
          },{
            'msvs_large_module_debug_link_mode%': '2',  # Yes
          }],
        ],
        'nacl_win64_defines': [
          # This flag is used to minimize dependencies when building
          # Native Client loader for 64-bit Windows.
          'NACL_WIN64',
        ],
      }],

      ['OS=="mac" or (OS=="linux" and chromeos==0 and target_arch!="arm")', {
        'use_cups%': 1,
      }, {
        'use_cups%': 0,
      }],

      # Set the relative path from this file to the GYP file of the JPEG
      # library used by Chromium.
      ['use_libjpeg_turbo==1', {
        'libjpeg_gyp_path': '../third_party/libjpeg_turbo/libjpeg.gyp',
      }, {
        'libjpeg_gyp_path': '../third_party/libjpeg/libjpeg.gyp',
      }],  # use_libjpeg_turbo==1

      # Use GConf, the GNOME configuration system.
      ['chromeos==1', {
        'use_gconf%': 0,
      }, {
        'use_gconf%': 1,
      }],

      # Setup -D flags passed into grit.
      ['chromeos==1', {
        'grit_defines': ['-D', 'chromeos'],
      }],
      ['toolkit_views==1', {
        'grit_defines': ['-D', 'toolkit_views'],
      }],
      ['touchui==1', {
        'grit_defines': ['-D', 'touchui'],
      }],
      ['remoting==1', {
        'grit_defines': ['-D', 'remoting'],
      }],
      ['use_titlecase_in_grd_files==1', {
        'grit_defines': ['-D', 'use_titlecase'],
      }],
      ['use_third_party_translations==1', {
        'grit_defines': ['-D', 'use_third_party_translations'],
        'locales': ['ast', 'eu', 'gl', 'ka', 'ku', 'ug'],
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

      # See http://gcc.gnu.org/onlinedocs/gcc-4.4.2/gcc/Optimize-Options.html
      'mac_release_optimization%': '3', # Use -O3 unless overridden
      'mac_debug_optimization%': '0',   # Use -O0 unless overridden
      # See http://msdn.microsoft.com/en-us/library/aa652360(VS.71).aspx
      'win_release_Optimization%': '2', # 2 = /Os
      'win_debug_Optimization%': '0',   # 0 = /Od
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
      'release_valgrind_build%': 0,

      'conditions': [
        ['OS=="win" and component=="shared_library"', {
          # See http://msdn.microsoft.com/en-us/library/aa652367.aspx
          'win_release_RuntimeLibrary%': '2', # 2 = /MT (nondebug DLL)
          'win_debug_RuntimeLibrary%': '3',   # 3 = /MTd (debug DLL)
        }, {
          # See http://msdn.microsoft.com/en-us/library/aa652367.aspx
          'win_release_RuntimeLibrary%': '0', # 0 = /MT (nondebug static)
          'win_debug_RuntimeLibrary%': '1',   # 1 = /MTd (debug static)
        }],
      ],
    },
    'conditions': [
      ['branding=="Chrome"', {
        'defines': ['GOOGLE_CHROME_BUILD'],
      }, {  # else: branding!="Chrome"
        'defines': ['CHROMIUM_BUILD'],
      }],
      ['toolkit_views==1', {
        'defines': ['TOOLKIT_VIEWS=1'],
      }],
      ['chromeos==1', {
        'defines': ['OS_CHROMEOS=1'],
      }],
      ['touchui==1', {
        'defines': ['TOUCH_UI=1'],
      }],
      ['profiling==1', {
        'defines': ['ENABLE_PROFILING=1'],
      }],
      ['remoting==1', {
        'defines': ['ENABLE_REMOTING=1'],
      }],
      ['proprietary_codecs==1', {
        'defines': ['USE_PROPRIETARY_CODECS'],
      }],
      ['enable_flapper_hacks==1', {
        'defines': ['ENABLE_FLAPPER_HACKS=1'],
      }],
      ['fastbuild!=0', {
        'conditions': [
          # For Windows, we don't genererate debug information.
          ['OS=="win"', {
            'msvs_settings': {
              'VCLinkerTool': {
                'GenerateDebugInformation': 'false',
              },
              'VCCLCompilerTool': {
                'DebugInformationFormat': '0',
              }
            }
          }, { # else: OS != "win", generate less debug information.
            'variables': {
              'debug_extra_cflags': '-g1',
            },
          }],
          # Clang creates chubby debug information, which makes linking very
          # slow. For now, don't create debug information with clang.  See
          # http://crbug.com/70000
          ['OS=="linux" and clang==1', {
            'variables': {
              'debug_extra_cflags': '-g0',
            },
          }],
        ],  # conditions for fastbuild.
      }],  # fastbuild!=0
      ['selinux==1', {
        'defines': ['CHROMIUM_SELINUX=1'],
      }],
      ['win_use_allocator_shim==0', {
        'conditions': [
          ['OS=="win"', {
            'defines': ['NO_TCMALLOC'],
          }],
        ],
      }],
      ['enable_gpu==1', {
        'defines': [
          'ENABLE_GPU=1',
        ],
      }],
      ['use_openssl==1', {
        'defines': [
          'USE_OPENSSL=1',
        ],
      }],
      ['enable_eglimage==1', {
        'defines': [
          'ENABLE_EGLIMAGE=1',
        ],
      }],
      ['coverage!=0', {
        'conditions': [
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_INSTRUMENT_PROGRAM_FLOW_ARCS': 'YES',  # -fprofile-arcs
              'GCC_GENERATE_TEST_COVERAGE_FILES': 'YES',  # -ftest-coverage
            },
            # Add -lgcov for types executable, shared_library, and
            # loadable_module; not for static_library.
            # This is a delayed conditional.
            'target_conditions': [
              ['_type!="static_library"', {
                'xcode_settings': { 'OTHER_LDFLAGS': [ '-lgcov' ] },
              }],
            ],
          }],
          ['OS=="linux"', {
            'cflags': [ '-ftest-coverage',
                        '-fprofile-arcs' ],
            'link_settings': { 'libraries': [ '-lgcov' ] },
          }],
          # Finally, for Windows, we simply turn on profiling.
          ['OS=="win"', {
            'msvs_settings': {
              'VCLinkerTool': {
                'Profile': 'true',
              },
              'VCCLCompilerTool': {
                # /Z7, not /Zi, so coverage is happyb
                'DebugInformationFormat': '1',
                'AdditionalOptions': ['/Yd'],
              }
            }
         }],  # OS==win
        ],  # conditions for coverage
      }],  # coverage!=0
      ['OS=="win"', {
        'defines': [
          '__STD_C',
          '_CRT_SECURE_NO_DEPRECATE',
          '_SCL_SECURE_NO_DEPRECATE',
        ],
        'include_dirs': [
          '<(DEPTH)/third_party/wtl/include',
        ],
      }],  # OS==win
    ],  # conditions for 'target_defaults'
    'target_conditions': [
      ['chromium_code==0', {
        'conditions': [
          [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
            'cflags!': [
              '-Wall',
              '-Wextra',
              '-Werror',
            ],
          }],
          [ 'OS=="win"', {
            'defines': [
              '_CRT_SECURE_NO_DEPRECATE',
              '_CRT_NONSTDC_NO_WARNINGS',
              '_CRT_NONSTDC_NO_DEPRECATE',
              '_SCL_SECURE_NO_DEPRECATE',
            ],
            'msvs_disabled_warnings': [4800],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'WarnAsError': 'false',
                'Detect64BitPortabilityProblems': 'false',
              },
            },
          }],
          [ 'OS=="mac"', {
            'xcode_settings': {
              'GCC_TREAT_WARNINGS_AS_ERRORS': 'NO',
              'WARNING_CFLAGS!': ['-Wall', '-Wextra'],
            },
          }],
        ],
      }, {
        # In Chromium code, we define __STDC_FORMAT_MACROS in order to get the
        # C99 macros on Mac and Linux.
        'defines': [
          '__STDC_FORMAT_MACROS',
        ],
        'conditions': [
          ['OS!="win"', {
            'sources/': [ ['exclude', '_win(_unittest)?\\.(h|cc)$'],
                          ['exclude', '(^|/)win/'],
                          ['exclude', '(^|/)win_[^/]*\\.(h|cc)$'] ],
          }],
          ['OS!="mac"', {
            'sources/': [ ['exclude', '_(cocoa|mac)(_unittest)?\\.(h|cc)$'],
                          ['exclude', '(^|/)(cocoa|mac)/'],
                          ['exclude', '\\.mm?$' ] ],
          }],
          ['OS!="linux" and OS!="freebsd" and OS!="openbsd"', {
            'sources/': [
              ['exclude', '_(chromeos|gtk|x|x11|xdg)(_unittest)?\\.(h|cc)$'],
              ['exclude', '(^|/)gtk/'],
              ['exclude', '(^|/)(gtk|x11)_[^/]*\\.(h|cc)$'],
            ],
          }],
          ['OS!="linux"', {
            'sources/': [
              ['exclude', '_linux(_unittest)?\\.(h|cc)$'],
              ['exclude', '(^|/)linux/'],
            ],
          }],
          # We use "POSIX" to refer to all non-Windows operating systems.
          ['OS=="win"', {
            'sources/': [ ['exclude', '_posix\\.(h|cc)$'] ],
            # turn on warnings for signed/unsigned mismatch on chromium code.
            'msvs_settings': {
              'VCCLCompilerTool': {
                'AdditionalOptions': ['/we4389'],
              },
            },
          }],
          ['chromeos!=1', {
            'sources/': [ ['exclude', '_chromeos\\.(h|cc)$'] ]
          }],
          ['toolkit_views==0', {
            'sources/': [ ['exclude', '_views\\.(h|cc)$'] ]
          }],
        ],
      }],
    ],  # target_conditions for 'target_defaults'
    'default_configuration': 'Debug',
    'configurations': {
      # VCLinkerTool LinkIncremental values below:
      #   0 == default
      #   1 == /INCREMENTAL:NO
      #   2 == /INCREMENTAL
      # Debug links incremental, Release does not.
      #
      # Abstract base configurations to cover common attributes.
      #
      'Common_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
      },
      'x86_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCLinkerTool': {
            'TargetMachine': '1',
          },
        },
        'msvs_configuration_platform': 'Win32',
      },
      'x64_Base': {
        'abstract': 1,
        'msvs_configuration_platform': 'x64',
        'msvs_settings': {
          'VCLinkerTool': {
            'TargetMachine': '17', # x86 - 64
            'AdditionalLibraryDirectories!':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib'],
            'AdditionalLibraryDirectories':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib/x64'],
          },
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories!':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib'],
            'AdditionalLibraryDirectories':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib/x64'],
          },
        },
        'defines': [
          # Not sure if tcmalloc works on 64-bit Windows.
          'NO_TCMALLOC',
        ],
      },
      'Debug_Base': {
        'abstract': 1,
        'defines': ['DYNAMIC_ANNOTATIONS_ENABLED=1'],
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '<(mac_debug_optimization)',
          'OTHER_CFLAGS': [ '<@(debug_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '<(win_debug_Optimization)',
            'PreprocessorDefinitions': ['_DEBUG'],
            'BasicRuntimeChecks': '<(win_debug_RuntimeChecks)',
            'RuntimeLibrary': '<(win_debug_RuntimeLibrary)',
            'conditions': [
              # According to MSVS, InlineFunctionExpansion=0 means
              # "default inlining", not "/Ob0".
              # Thus, we have to handle InlineFunctionExpansion==0 separately.
              ['win_debug_InlineFunctionExpansion==0', {
                'AdditionalOptions': ['/Ob0'],
              }],
              ['win_debug_InlineFunctionExpansion!=""', {
                'InlineFunctionExpansion':
                  '<(win_debug_InlineFunctionExpansion)',
              }],
              ['win_debug_disable_iterator_debugging==1', {
                'PreprocessorDefinitions': ['_HAS_ITERATOR_DEBUGGING=0'],
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '<(msvs_debug_link_incremental)',
          },
          'VCResourceCompilerTool': {
            'PreprocessorDefinitions': ['_DEBUG'],
          },
        },
        'conditions': [
          ['OS=="linux"', {
            'cflags': [
              '<@(debug_extra_cflags)',
            ],
          }],
        ],
      },
      'Release_Base': {
        'abstract': 1,
        'defines': [
          'NDEBUG',
        ],
        'xcode_settings': {
          'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
          'GCC_OPTIMIZATION_LEVEL': '<(mac_release_optimization)',
          'OTHER_CFLAGS': [ '<@(release_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '<(win_release_Optimization)',
            'RuntimeLibrary': '<(win_release_RuntimeLibrary)',
            'conditions': [
              # According to MSVS, InlineFunctionExpansion=0 means
              # "default inlining", not "/Ob0".
              # Thus, we have to handle InlineFunctionExpansion==0 separately.
              ['win_release_InlineFunctionExpansion==0', {
                'AdditionalOptions': ['/Ob0'],
              }],
              ['win_release_InlineFunctionExpansion!=""', {
                'InlineFunctionExpansion':
                  '<(win_release_InlineFunctionExpansion)',
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
          },
        },
        'conditions': [
          ['release_valgrind_build==0', {
            'defines': ['NVALGRIND', 'DYNAMIC_ANNOTATIONS_ENABLED=0'],
          }, {
            'defines': ['DYNAMIC_ANNOTATIONS_ENABLED=1'],
          }],
          ['win_use_allocator_shim==0', {
            'defines': ['NO_TCMALLOC'],
          }],
          ['OS=="linux"', {
            'cflags': [
             '<@(release_extra_cflags)',
            ],
          }],
        ],
      },
      'Purify_Base': {
        'abstract': 1,
        'defines': [
          'PURIFY',
          'NO_TCMALLOC',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'RuntimeLibrary': '0',
            'BufferSecurityCheck': 'false',
          },
          'VCLinkerTool': {
            'EnableCOMDATFolding': '1',
            'LinkIncremental': '1',
          },
        },
      },
      #
      # Concrete configurations
      #
      'Debug': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Debug_Base'],
      },
      'Release': {
        'inherit_from': ['Common_Base', 'x86_Base', 'Release_Base'],
        'conditions': [
          ['msvs_use_common_release', {
            'includes': ['release.gypi'],
          }],
        ]
      },
      'conditions': [
        [ 'OS=="win"', {
          # TODO(bradnelson): add a gyp mechanism to make this more graceful.
          'Purify': {
            'inherit_from': ['Common_Base', 'x86_Base', 'Release_Base', 'Purify'],
          },
          'Debug_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Debug_Base'],
          },
          'Release_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Release_Base'],
          },
          'Purify_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Release_Base', 'Purify_Base'],
          },
        }],
      ],
    },
  },
  'conditions': [
    ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
      'target_defaults': {
        # Enable -Werror by default, but put it in a variable so it can
        # be disabled in ~/.gyp/include.gypi on the valgrind builders.
        'variables': {
          # Use -fno-strict-aliasing by default since gcc 4.4 has periodic
          # issues that slip through the cracks. We could do this just for
          # gcc 4.4 but it makes more sense to be consistent on all
          # compilers in use. TODO(Craig): turn this off again when
          # there is some 4.4 test infrastructure in place and existing
          # aliasing issues have been fixed.
          'no_strict_aliasing%': 1,
          'conditions': [['OS=="linux"', {'werror%': '-Werror',}],
                         ['OS=="freebsd"', {'werror%': '',}],
                         ['OS=="openbsd"', {'werror%': '',}],
          ],
        },
        'cflags': [
          '<(werror)',  # See note above about the werror variable.
          '-pthread',
          '-fno-exceptions',
          '-Wall',
          # TODO(evan): turn this back on once all the builds work.
          # '-Wextra',
          # Don't warn about unused function params.  We use those everywhere.
          '-Wno-unused-parameter',
          # Don't warn about the "struct foo f = {0};" initialization pattern.
          '-Wno-missing-field-initializers',
          '-D_FILE_OFFSET_BITS=64',
          # Don't export any symbols (for example, to plugins we dlopen()).
          # Note: this is *required* to make some plugins work.
          '-fvisibility=hidden',
          '-pipe',
        ],
        'cflags_cc': [
          '-fno-rtti',
          '-fno-threadsafe-statics',
          # Make inline functions have hidden visiblity by default.
          # Surprisingly, not covered by -fvisibility=hidden.
          '-fvisibility-inlines-hidden',
        ],
        'ldflags': [
          '-pthread', '-Wl,-z,noexecstack',
        ],
        'configurations': {
          'Debug_Base': {
            'variables': {
              'debug_optimize%': '0',
            },
            'defines': [
              '_DEBUG',
            ],
            'cflags': [
              '-O>(debug_optimize)',
              '-g',
            ],
          },
          'Release_Base': {
            'variables': {
              'release_optimize%': '2',
              # Binaries become big and gold is unable to perform GC
              # and remove unused sections for some of test targets
              # on 32 bit platform.
              # (This is currently observed only in chromeos valgrind bots)
              # The following flag is to disable --gc-sections linker
              # option for these bots.
              'no_gc_sections%': 0,
            },
            'cflags': [
              '-O>(release_optimize)',
              # Don't emit the GCC version ident directives, they just end up
              # in the .comment section taking up binary size.
              '-fno-ident',
              # Put data and code in their own sections, so that unused symbols
              # can be removed at link time with --gc-sections.
              '-fdata-sections',
              '-ffunction-sections',
            ],
            'ldflags': [
              # Specifically tell the linker to perform optimizations.
              # See http://lwn.net/Articles/192624/ .
              '-Wl,-O1',
              '-Wl,--as-needed',
            ],
            'conditions' : [
              ['no_gc_sections==0', {
                'ldflags': [
                  '-Wl,--gc-sections',
                ],
              }],
              ['clang==1', {
                'cflags!': [
                  '-fno-ident',
                ],
              }],
              ['profiling==1', {
                'cflags': [
                  '-fno-omit-frame-pointer',
                  '-g',
                ],
              }],
            ]
          },
        },
        'variants': {
          'coverage': {
            'cflags': ['-fprofile-arcs', '-ftest-coverage'],
            'ldflags': ['-fprofile-arcs'],
          },
          'profile': {
            'cflags': ['-pg', '-g'],
            'ldflags': ['-pg'],
          },
          'symbols': {
            'cflags': ['-g'],
          },
        },
        'conditions': [
          [ 'target_arch=="ia32"', {
            'asflags': [
              # Needed so that libs with .s files (e.g. libicudata.a)
              # are compatible with the general 32-bit-ness.
              '-32',
            ],
            # All floating-point computations on x87 happens in 80-bit
            # precision.  Because the C and C++ language standards allow
            # the compiler to keep the floating-point values in higher
            # precision than what's specified in the source and doing so
            # is more efficient than constantly rounding up to 64-bit or
            # 32-bit precision as specified in the source, the compiler,
            # especially in the optimized mode, tries very hard to keep
            # values in x87 floating-point stack (in 80-bit precision)
            # as long as possible. This has important side effects, that
            # the real value used in computation may change depending on
            # how the compiler did the optimization - that is, the value
            # kept in 80-bit is different than the value rounded down to
            # 64-bit or 32-bit. There are possible compiler options to make
            # this behavior consistent (e.g. -ffloat-store would keep all
            # floating-values in the memory, thus force them to be rounded
            # to its original precision) but they have significant runtime
            # performance penalty.
            #
            # -mfpmath=sse -msse2 makes the compiler use SSE instructions
            # which keep floating-point values in SSE registers in its
            # native precision (32-bit for single precision, and 64-bit for
            # double precision values). This means the floating-point value
            # used during computation does not change depending on how the
            # compiler optimized the code, since the value is always kept
            # in its specified precision.
            'conditions': [
              ['branding=="Chromium" and disable_sse2==0', {
                'cflags': [
                  '-march=pentium4',
                  '-msse2',
                  '-mfpmath=sse',
                ],
              }],
              # ChromeOS targets Pinetrail, which is sse3, but most of the
              # benefit comes from sse2 so this setting allows ChromeOS
              # to build on other CPUs.  In the future -march=atom would help
              # but requires a newer compiler.
              ['chromeos==1 and disable_sse2==0', {
                'cflags': [
                  '-msse2',
                ],
              }],
              # Install packages have started cropping up with
              # different headers between the 32-bit and 64-bit
              # versions, so we have to shadow those differences off
              # and make sure a 32-bit-on-64-bit build picks up the
              # right files.
              ['host_arch!="ia32"', {
                'include_dirs+': [
                  '/usr/include32',
                ],
              }],
            ],
            # -mmmx allows mmintrin.h to be used for mmx intrinsics.
            # video playback is mmx and sse2 optimized.
            'cflags': [
              '-m32',
              '-mmmx',
            ],
            'ldflags': [
              '-m32',
            ],
          }],
          ['target_arch=="arm"', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags_cc': [
                  # The codesourcery arm-2009q3 toolchain warns at that the ABI
                  # has changed whenever it encounters a varargs function. This
                  # silences those warnings, as they are not helpful and
                  # clutter legitimate warnings.
                  '-Wno-abi',
                ],
                'conditions': [
                  ['arm_thumb == 1', {
                    'cflags': [
                    '-mthumb',
                    # TODO(piman): -Wa,-mimplicit-it=thumb is needed for
                    # inline assembly that uses condition codes but it's
                    # suboptimal. Better would be to #ifdef __thumb__ at the
                    # right place and have a separate thumb path.
                    '-Wa,-mimplicit-it=thumb',
                    ]
                  }],
                  ['armv7==1', {
                    'cflags': [
                      '-march=armv7-a',
                      '-mtune=cortex-a8',
                      '-mfloat-abi=softfp',
                    ],
                    'conditions': [
                      ['arm_neon==1', {
                        'cflags': [ '-mfpu=neon', ],
                      }, {
                        'cflags': [ '-mfpu=<(arm_fpu)', ],
                      }]
                    ],
                  }],
                ],
              }],
            ],
          }],
          ['linux_fpic==1', {
            'cflags': [
              '-fPIC',
            ],
          }],
          ['sysroot!=""', {
            'target_conditions': [
              ['_toolset=="target"', {
                'cflags': [
                  '--sysroot=<(sysroot)',
                ],
                'ldflags': [
                  '--sysroot=<(sysroot)',
                ],
              }]]
          }],
          ['clang==1', {
            'cflags': [
              # Clang spots more unused functions.
              '-Wno-unused-function',
              # Don't die on dtoa code that uses a char as an array index.
              '-Wno-char-subscripts',
              # Survive EXPECT_EQ(unnamed_enum, unsigned int) -- see
              # http://code.google.com/p/googletest/source/detail?r=446 .
              # TODO(thakis): Use -isystem instead (http://crbug.com/58751 ).
              '-Wno-unnamed-type-template-args',
              # TODO(thakis): Reenable, http://crbug.com/71375
              '-Wno-uninitialized',
            ],
            'cflags!': [
              # Clang doesn't seem to know know this flag.
              '-mfpmath=sse',
            ],
          }],
          ['clang==1 and clang_load!="" and clang_add_plugin!=""', {
            'cflags': [
              '-Xclang', '-load', '-Xclang', '<(clang_load)',
              '-Xclang', '-add-plugin', '-Xclang', '<(clang_add_plugin)',
            ],
          }],
          ['no_strict_aliasing==1', {
            'cflags': [
              '-fno-strict-aliasing',
            ],
          }],
          ['linux_breakpad==1', {
            'cflags': [ '-g' ],
            'defines': ['USE_LINUX_BREAKPAD'],
          }],
          ['library=="shared_library"', {
            # When building with shared libraries, remove the visiblity-hiding
            # flag.
            'cflags!': [ '-fvisibility=hidden' ],
            'conditions': [
              ['target_arch=="x64" or target_arch=="arm"', {
                # Shared libraries need -fPIC on x86-64 and arm
                'cflags': ['-fPIC']
              }]
            ],
            'ldflags!': [
              # --as-needed confuses library interdependencies.
              # See http://code.google.com/p/chromium/issues/detail?id=61430
              '-Wl,--as-needed',
            ],
          }],
          ['linux_use_heapchecker==1', {
            'variables': {'linux_use_tcmalloc%': 1},
          }],
          ['linux_use_tcmalloc==0', {
            'defines': ['NO_TCMALLOC'],
          }],
          ['linux_use_heapchecker==0', {
            'defines': ['NO_HEAPCHECKER'],
          }],
          ['linux_keep_shadow_stacks==1', {
            'defines': ['KEEP_SHADOW_STACKS'],
            'cflags': ['-finstrument-functions'],
          }],
        ],
      },
    }],
    # FreeBSD-specific options; note that most FreeBSD options are set above,
    # with Linux.
    ['OS=="freebsd"', {
      'target_defaults': {
        'ldflags': [
          '-Wl,--no-keep-memory',
        ],
      },
    }],
    ['OS=="solaris"', {
      'cflags!': ['-fvisibility=hidden'],
      'cflags_cc!': ['-fvisibility-inlines-hidden'],
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'variables': {
          # These should be 'mac_real_dsym%' and 'mac_strip%', but there
          # seems to be a bug with % in variables that are intended to be
          # set to different values in different targets, like these two.
          'mac_strip': 1,      # Strip debugging symbols from the target.
          'mac_real_dsym': 0,  # Fake .dSYMs are fine in most cases.
        },
        'mac_bundle': 0,
        'xcode_settings': {
          'ALWAYS_SEARCH_USER_PATHS': 'NO',
          'GCC_C_LANGUAGE_STANDARD': 'c99',         # -std=c99
          'GCC_CW_ASM_SYNTAX': 'NO',                # No -fasm-blocks
          'GCC_DYNAMIC_NO_PIC': 'NO',               # No -mdynamic-no-pic
                                                    # (Equivalent to -fPIC)
          'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',        # -fno-exceptions
          'GCC_ENABLE_CPP_RTTI': 'NO',              # -fno-rtti
          'GCC_ENABLE_PASCAL_STRINGS': 'NO',        # No -mpascal-strings
          # GCC_INLINES_ARE_PRIVATE_EXTERN maps to -fvisibility-inlines-hidden
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES',
          'GCC_OBJC_CALL_CXX_CDTORS': 'YES',        # -fobjc-call-cxx-cdtors
          'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',      # -fvisibility=hidden
          'GCC_THREADSAFE_STATICS': 'NO',           # -fno-threadsafe-statics
          'GCC_TREAT_WARNINGS_AS_ERRORS': 'YES',    # -Werror
          'GCC_VERSION': '4.2',
          'GCC_WARN_ABOUT_MISSING_NEWLINE': 'YES',  # -Wnewline-eof
          # MACOSX_DEPLOYMENT_TARGET maps to -mmacosx-version-min
          'MACOSX_DEPLOYMENT_TARGET': '<(mac_deployment_target)',
          'PREBINDING': 'NO',                       # No -Wl,-prebind
          'USE_HEADERMAP': 'NO',
          'WARNING_CFLAGS': [
            '-Wall',
            '-Wendif-labels',
            '-Wextra',
            # Don't warn about unused function parameters.
            '-Wno-unused-parameter',
            # Don't warn about the "struct foo f = {0};" initialization
            # pattern.
            '-Wno-missing-field-initializers',
          ],
          'conditions': [
            ['chromium_mac_pch', {'GCC_PRECOMPILE_PREFIX_HEADER': 'YES'},
                                 {'GCC_PRECOMPILE_PREFIX_HEADER': 'NO'}
            ],
            ['clang==1', {
              'WARNING_CFLAGS': [
                # Don't die on dtoa code that uses a char as an array index.
                # This is required solely for base/third_party/dmg_fp/dtoa.cc.
                '-Wno-char-subscripts',
                # Clang spots more unused functions.
                '-Wno-unused-function',
                # Survive EXPECT_EQ(unnamed_enum, unsigned int) -- see
                # http://code.google.com/p/googletest/source/detail?r=446 .
                # TODO(thakis): Use -isystem instead (http://crbug.com/58751 ).
                '-Wno-unnamed-type-template-args',
                # TODO(thakis): Reenable, http://crbug.com/71375
                '-Wno-uninitialized',
              ],
            }],
            ['clang==1 and clang_load!="" and clang_add_plugin!=""', {
              'OTHER_CFLAGS': [
                '-Xclang', '-load', '-Xclang', '<(clang_load)',
                '-Xclang', '-add-plugin', '-Xclang', '<(clang_add_plugin)',
              ],
            }],
          ],
        },
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
          ['_mac_bundle', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          }],
          ['(_type=="executable" or _type=="shared_library" or \
             _type=="loadable_module") and mac_strip!=0', {
            'target_conditions': [
              ['mac_real_dsym == 1', {
                # To get a real .dSYM bundle produced by dsymutil, set the
                # debug information format to dwarf-with-dsym.  Since
                # strip_from_xcode will not be used, set Xcode to do the
                # stripping as well.
                'configurations': {
                  'Release_Base': {
                    'xcode_settings': {
                      'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
                      'DEPLOYMENT_POSTPROCESSING': 'YES',
                      'STRIP_INSTALLED_PRODUCT': 'YES',
                      'target_conditions': [
                        ['_type=="shared_library" or _type=="loadable_module"', {
                          # The Xcode default is to strip debugging symbols
                          # only (-S).  Local symbols should be stripped as
                          # well, which will be handled by -x.  Xcode will
                          # continue to insert -S when stripping even when
                          # additional flags are added with STRIPFLAGS.
                          'STRIPFLAGS': '-x',
                        }],  # _type=="shared_library" or _type=="loadable_module"'
                      ],  # target_conditions
                    },  # xcode_settings
                  },  # configuration "Release"
                },  # configurations
              }, {  # mac_real_dsym != 1
                # To get a fast fake .dSYM bundle, use a post-build step to
                # produce the .dSYM and strip the executable.  strip_from_xcode
                # only operates in the Release configuration.
                'postbuilds': [
                  {
                    'variables': {
                      # Define strip_from_xcode in a variable ending in _path
                      # so that gyp understands it's a path and performs proper
                      # relativization during dict merging.
                      'strip_from_xcode_path': 'mac/strip_from_xcode',
                    },
                    'postbuild_name': 'Strip If Needed',
                    'action': ['<(strip_from_xcode_path)'],
                  },
                ],  # postbuilds
              }],  # mac_real_dsym
            ],  # target_conditions
          }],  # (_type=="executable" or _type=="shared_library" or
               #  _type=="loadable_module") and mac_strip!=0
        ],  # target_conditions
      },  # target_defaults
    }],  # OS=="mac"
    ['OS=="win"', {
      'target_defaults': {
        'defines': [
          '_WIN32_WINNT=0x0600',
          'WINVER=0x0600',
          'WIN32',
          '_WINDOWS',
          'NOMINMAX',
          '_CRT_RAND_S',
          'CERT_CHAIN_PARA_HAS_EXTRA_FIELDS',
          'WIN32_LEAN_AND_MEAN',
          '_SECURE_ATL',
          '_ATL_NO_OPENGL',
          '_HAS_TR1=0',
        ],
        'conditions': [
          ['component=="static_library"', {
            'defines': [
              '_HAS_EXCEPTIONS=0',
            ],
          }],
        ],
        'msvs_system_include_dirs': [
          '<(DEPTH)/third_party/platformsdk_win7/files/Include',
          '<(DEPTH)/third_party/directxsdk/files/Include',
          '$(VSInstallDir)/VC/atlmfc/include',
        ],
        'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
        'msvs_disabled_warnings': [4351, 4396, 4503, 4819],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '3',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'conditions': [
              [ 'msvs_multi_core_compile', {
                'AdditionalOptions': ['/MP'],
              }],
              ['component=="shared_library"', {
                'ExceptionHandling': '1',  # /EHsc
              }, {
                'ExceptionHandling': '0',
              }],
            ],
          },
          'VCLibrarianTool': {
            'AdditionalOptions': ['/ignore:4221'],
            'AdditionalLibraryDirectories': [
              '<(DEPTH)/third_party/platformsdk_win7/files/Lib',
              '<(DEPTH)/third_party/directxsdk/files/Lib/x86',
            ],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'wininet.lib',
              'dnsapi.lib',
              'version.lib',
              'msimg32.lib',
              'ws2_32.lib',
              'usp10.lib',
              'psapi.lib',
              'dbghelp.lib',
            ],
            'AdditionalLibraryDirectories': [
              '<(DEPTH)/third_party/platformsdk_win7/files/Lib',
              '<(DEPTH)/third_party/directxsdk/files/Lib/x86',
            ],
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'FixedBaseAddress': '1',
            # SubSystem values:
            #   0 == not set
            #   1 == /SUBSYSTEM:CONSOLE
            #   2 == /SUBSYSTEM:WINDOWS
            # Most of the executables we'll ever create are tests
            # and utilities with console output.
            'SubSystem': '1',
          },
          'VCMIDLTool': {
            'GenerateStublessProxies': 'true',
            'TypeLibraryName': '$(InputName).tlb',
            'OutputDirectory': '$(IntDir)',
            'HeaderFileName': '$(InputName).h',
            'DLLDataFileName': 'dlldata.c',
            'InterfaceIdentifierFileName': '$(InputName)_i.c',
            'ProxyFileName': '$(InputName)_p.c',
          },
          'VCResourceCompilerTool': {
            'Culture' : '1033',
            'AdditionalIncludeDirectories': [
              '<(DEPTH)',
              '<(SHARED_INTERMEDIATE_DIR)',
            ],
          },
        },
      },
    }],
    ['disable_nacl==1 or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
      'target_defaults': {
        'defines': [
          'DISABLE_NACL',
        ],
      },
    }],
    ['OS=="win" and msvs_use_common_linker_extras', {
      'target_defaults': {
        'msvs_settings': {
          'VCLinkerTool': {
            'DelayLoadDLLs': [
              'dbghelp.dll',
              'dwmapi.dll',
              'uxtheme.dll',
            ],
          },
        },
        'configurations': {
          'x86_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalOptions': [
                  '/safeseh',
                  '/dynamicbase',
                  '/ignore:4199',
                  '/ignore:4221',
                  '/nxcompat',
                ],
              },
            },
          },
          'x64_Base': {
            'msvs_settings': {
              'VCLinkerTool': {
                'AdditionalOptions': [
                  # safeseh is not compatible with x64
                  '/dynamicbase',
                  '/ignore:4199',
                  '/ignore:4221',
                  '/nxcompat',
                ],
              },
            },
          },
        },
      },
    }],
    ['enable_new_npdevice_api==1', {
      'target_defaults': {
        'defines': [
          'ENABLE_NEW_NPDEVICE_API',
        ],
      },
    }],
  ],
  'xcode_settings': {
    # DON'T ADD ANYTHING NEW TO THIS BLOCK UNLESS YOU REALLY REALLY NEED IT!
    # This block adds *project-wide* configuration settings to each project
    # file.  It's almost always wrong to put things here.  Specify your
    # custom xcode_settings in target_defaults to add them to targets instead.

    # In an Xcode Project Info window, the "Base SDK for All Configurations"
    # setting sets the SDK on a project-wide basis.  In order to get the
    # configured SDK to show properly in the Xcode UI, SDKROOT must be set
    # here at the project level.
    'SDKROOT': 'macosx<(mac_sdk)',  # -isysroot

    # The Xcode generator will look for an xcode_settings section at the root
    # of each dict and use it to apply settings on a file-wide basis.  Most
    # settings should not be here, they should be in target-specific
    # xcode_settings sections, or better yet, should use non-Xcode-specific
    # settings in target dicts.  SYMROOT is a special case, because many other
    # Xcode variables depend on it, including variables such as
    # PROJECT_DERIVED_FILE_DIR.  When a source group corresponding to something
    # like PROJECT_DERIVED_FILE_DIR is added to a project, in order for the
    # files to appear (when present) in the UI as actual files and not red
    # red "missing file" proxies, the correct path to PROJECT_DERIVED_FILE_DIR,
    # and therefore SYMROOT, needs to be set at the project level.
    'SYMROOT': '<(DEPTH)/xcodebuild',
  },
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
