# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# IMPORTANT:
# Please don't directly include this file if you are building via gyp_chromium,
# since gyp_chromium is automatically forcing its inclusion.
{
  'variables': {
    # .gyp files should set chromium_code to 1 if they build Chromium-specific
    # code, as opposed to external code.  This variable is used to control
    # such things as the set of warnings to enable, and whether warnings are
    # treated as errors.
    'chromium_code%': 0,

    # Variables expected to be overriden on the GYP command line (-D) or by
    # ~/.gyp/include.gypi.

    # Putting a variables dict inside another variables dict looks kind of
    # weird.  This is done so that "branding" and "buildtype" are defined as
    # variables within the outer variables dict here.  This is necessary
    # to get these variables defined for the conditions within this variables
    # dict that operate on these variables.
    'variables': {
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

      # Compute the architecture that we're building for. Default to the
      # architecture that we're building on.
      'conditions': [
        [ 'OS=="linux"', {
          # This handles the Linux platforms we generally deal with. Anything
          # else gets passed through, which probably won't work very well; such
          # hosts should pass an explicit target_arch to gyp.
          'target_arch%':
            '<!(uname -m | sed -e "s/i.86/ia32/;s/x86_64/x64/;s/arm.*/arm/")',

        }, {  # OS!="linux"
          'target_arch%': 'ia32',
        }],
      ],

      # We do want to build Chromium with Breakpad support in certain
      # situations. I.e. for Chrome bot.
      'linux_chromium_breakpad%': 0,
      # And if we want to dump symbols.
      'linux_chromium_dump_symbols%': 0,
      # Also see linux_strip_binary below.

      # By default, Linux does not use views. To turn on views in Linux,
      # set the variable GYP_DEFINES to "toolkit_views=1", or modify
      # ~/.gyp/include.gypi .
      'toolkit_views%': 0,

      # Defaults to a desktop build, overridden via command line/env.
      'chromeos%': 0,

      # This variable tells WebCore.gyp and JavaScriptCore.gyp whether they are
      # are built under a chromium full build (1) or a webkit.org chromium
      # build (0).
      'inside_chromium_build%': 1,

      # Set to 1 to enable fast builds. It disables debug info for fastest
      # compilation.
      'fastbuild%': 0,
    },

    # Define branding and buildtype on the basis of their settings within the
    # variables sub-dict above, unless overridden.
    'branding%': '<(branding)',
    'buildtype%': '<(buildtype)',
    'target_arch%': '<(target_arch)',
    'toolkit_views%': '<(toolkit_views)',
    'chromeos%': '<(chromeos)',
    'inside_chromium_build%': '<(inside_chromium_build)',
    'fastbuild%': '<(fastbuild)',

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

    # To do a shared build on linux we need to be able to choose between type
    # static_library and shared_library. We default to doing a static build
    # but you can override this with "gyp -Dlibrary=shared_library" or you
    # can add the following line (without the #) to ~/.gyp/include.gypi
    # {'variables': {'library': 'shared_library'}}
    # to compile as shared by default
    'library%': 'static_library',

    # The Google Update appid.
    'google_update_appid%': '{8A69D345-D564-463c-AFF1-A69D9E530F96}',

    # Whether to add the experimental build define.
    'chrome_frame_define%': 0,

    # Whether pepper APIs are enabled.
    'enable_pepper%': 0,

    # Whether usage of OpenMAX is enabled.
    'enable_openmax%': 0,

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

    # The system root for cross-compiles. Default: none.
    'sysroot%': '',

    # This is the location of the sandbox binary. Chrome looks for this before
    # running the zygote process. If found, and SUID, it will be used to
    # sandbox the zygote process and, thus, all renderer processes.
    'linux_sandbox_path%': '',

    # Set this to true to enable SELinux support.
    'selinux%': 0,

    # Strip the binary after dumping symbols.
    'linux_strip_binary%': 0,

    # Enable TCMalloc.
    'linux_use_tcmalloc%': 0,

    # Set to select the Title Case versions of strings in GRD files.
    'use_titlecase_in_grd_files%': 0,

    # Used to disable Native Client at compile time, for platforms where it
    # isn't supported
    'disable_nacl%': 0,

    # Set ARM-v7 compilation flags
    'armv7%': 0,

    'conditions': [
      ['OS=="linux"', {
        # This will set gcc_version to XY if you are running gcc X.Y.*.
        # This is used to tweak build flags for gcc 4.4.
        'gcc_version%': '<!(python <(DEPTH)/build/compiler_version.py)',
        'conditions': [
          ['branding=="Chrome" or linux_chromium_breakpad==1', {
            'linux_breakpad%': 1,
          }, {
            'linux_breakpad%': 0,
          }],
          # All Chrome builds have breakpad symbols, but only process the
          # symbols from official builds.
          # TODO(mmoss) dump_syms segfaults on x64. Enable once dump_syms and
          # crash server handle 64-bit symbols.
          ['linux_chromium_dump_symbols==1 or '
           '(branding=="Chrome" and buildtype=="Official" and '
           'target_arch=="ia32")', {
            'linux_dump_symbols%': 1,
          }, {
            'linux_dump_symbols%': 0,
          }],
          ['toolkit_views==0', {
            # GTK wants Title Case strings
            'use_titlecase_in_grd_files%': 1,
          }],
        ],
      }],  # OS=="linux"
      ['OS=="mac"', {
        # Mac wants Title Case strings
        'use_titlecase_in_grd_files%': 1,
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
      }],
    ],

    # NOTE: When these end up in the Mac bundle, we need to replace '-' for '_'
    # so Cocoa is happy (http://crbug.com/20441).
    'locales': [
      'am', 'ar', 'bg', 'bn', 'ca', 'cs', 'da', 'de', 'el', 'en-GB',
      'en-US', 'es-419', 'es', 'et', 'fi', 'fil', 'fr', 'gu', 'he',
      'hi', 'hr', 'hu', 'id', 'it', 'ja', 'kn', 'ko', 'lt', 'lv',
      'ml', 'mr', 'nb', 'nl', 'or', 'pl', 'pt-BR', 'pt-PT', 'ro',
      'ru', 'sk', 'sl', 'sr', 'sv', 'sw', 'ta', 'te', 'th', 'tr',
      'uk', 'vi', 'zh-CN', 'zh-TW',
    ],
  },
  'target_defaults': {
    'variables': {
      'mac_release_optimization%': '3', # Use -O3 unless overridden
      'mac_debug_optimization%': '0',   # Use -O0 unless overridden
      'release_extra_cflags%': '',
      'debug_extra_cflags%': '',
      'release_valgrind_build%': 0,
    },
    'conditions': [
      ['branding=="Chrome"', {
        'defines': ['GOOGLE_CHROME_BUILD'],
      }, {  # else: branding!="Chrome"
        'defines': ['CHROMIUM_BUILD'],
      }],
      ['chrome_frame_define', {
        'defines': ['CHROME_FRAME_BUILD'],
      }],
      ['toolkit_views==1', {
        'defines': ['TOOLKIT_VIEWS=1'],
      }],
      ['chromeos==1', {
        'defines': ['CHROMEOS_TRANSITIONAL=1'],
      }],
      ['chromeos==1 or toolkit_views==1', {
        'defines': ['OS_CHROMEOS=1'],
      }],
      ['enable_pepper==1', {
        'defines': ['ENABLE_PEPPER=1'],
      }],
      ['fastbuild!=0', {
        'conditions': [
          # Finally, for Windows, we simply turn on profiling.
          ['OS=="win"', {
            'msvs_settings': {
              'VCLinkerTool': {
                'GenerateDebugInformation': 'false',
              },
              'VCCLCompilerTool': {
                'DebugInformationFormat': '0',
              }
            }
         }],  # OS==win
        ],  # conditions for fastbuild.
      }],  # fastbuild!=0
      ['selinux==1', {
        'defines': ['CHROMIUM_SELINUX=1'],
      }],
      ['coverage!=0', {
        'conditions': [
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_INSTRUMENT_PROGRAM_FLOW_ARCS': 'YES',  # -fprofile-arcs
              'GCC_GENERATE_TEST_COVERAGE_FILES': 'YES',  # -ftest-coverage
            },
            # Add -lgcov for executables and shared_libraries, not for
            # static_libraries.  This is a delayed conditional.
            'target_conditions': [
              ['_type=="executable" or _type=="shared_library"', {
                'xcode_settings': { 'OTHER_LDFLAGS': [ '-lgcov' ] },
              }],
            ],
          }],
          # Linux gyp (into scons) doesn't like target_conditions?
          # TODO(???): track down why 'target_conditions' doesn't work
          # on Linux gyp into scons like it does on Mac gyp into xcodeproj.
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
                'AdditionalOptions': '/Yd',
              }
            }
         }],  # OS==win
        ],  # conditions for coverage
      }],  # coverage!=0
    ],  # conditions for 'target_defaults'
    'default_configuration': 'Debug',
    'configurations': {
       # VCLinkerTool LinkIncremental values below:
       #   0 == default
       #   1 == /INCREMENTAL:NO
       #   2 == /INCREMENTAL
       # Debug links incremental, Release does not.
      'Common': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory': '$(SolutionDir)$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
        'msvs_configuration_platform': 'Win32',
      },
      'Debug': {
        'inherit_from': ['Common'],
        'xcode_settings': {
          'COPY_PHASE_STRIP': 'NO',
          'GCC_OPTIMIZATION_LEVEL': '<(mac_debug_optimization)',
          'OTHER_CFLAGS': [ '<@(debug_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'PreprocessorDefinitions': ['_DEBUG'],
            'BasicRuntimeChecks': '3',
            'RuntimeLibrary': '1',
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
      'Release': {
        'inherit_from': ['Common'],
        'defines': [
          'NDEBUG',
        ],
        'xcode_settings': {
          'DEAD_CODE_STRIPPING': 'YES',  # -Wl,-dead_strip
          'GCC_OPTIMIZATION_LEVEL': '<(mac_release_optimization)',
          'OTHER_CFLAGS': [ '<@(release_extra_cflags)', ],
        },
        'msvs_settings': {
          'VCLinkerTool': {
            'LinkIncremental': '1',
          },
        },
        'conditions': [
          ['release_valgrind_build==0', {
            'defines': ['NVALGRIND'],
          }],
          ['msvs_use_common_release', {
            'msvs_props': ['release.vsprops'],
          }],
          ['OS=="linux"', {
            'cflags': [
             '<@(release_extra_cflags)',
            ],
          }],
        ],
      },
      'conditions': [
        [ 'OS=="win"', {
          # TODO(bradnelson): add a gyp mechanism to make this more graceful.
          'Purify': {
            'inherit_from': ['Release'],
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
          'Release - no tcmalloc': {
            'inherit_from': ['Release'],
            'defines': ['NO_TCMALLOC'],
          },
          'Debug_x64': {
            'inherit_from': ['Debug'],
            'msvs_configuration_platform': 'x64',
          },
          'Release_x64': {
            'inherit_from': ['Release'],
            'msvs_configuration_platform': 'x64',
          },
          'Purify_x64': {
            'inherit_from': ['Purify'],
            'msvs_configuration_platform': 'x64',
          },
          'Release - no tcmalloc_x64': {
            'inherit_from': ['Release - no tcmalloc'],
            'msvs_configuration_platform': 'x64',
          },
        }],
      ],
    },
  },
  'conditions': [
    ['OS=="linux"', {
      'target_defaults': {
        # Enable -Werror by default, but put it in a variable so it can
        # be disabled in ~/.gyp/include.gypi on the valgrind builders.
        'variables': {
          'werror%': '-Werror',
          'no_strict_aliasing%': 0,
        },
        'cflags': [
          '<(werror)',  # See note above about the werror variable.
          '-pthread',
          # We don't use exceptions.  By disabling exceptions
          # (and asynchronous-unwind-tables), we shave off 2.5mb from
          # our resulting binary by not including the eh_frame section.
          '-fno-exceptions',
          '-fno-asynchronous-unwind-tables',
          '-fvisibility=hidden',
          '-Wall',
          '-D_FILE_OFFSET_BITS=64',
        ],
        'cflags_cc': [
          '-fno-rtti',
          '-fno-threadsafe-statics',
          # Make inline functions have hidden visiblity by default.
          # Surprisingly, not covered by -fvisibility=hidden.
          '-fvisibility-inlines-hidden',
        ],
        'ldflags': [
          '-pthread',
        ],
        'scons_variable_settings': {
          'LIBPATH': ['$LIB_DIR'],
          # Linking of large files uses lots of RAM, so serialize links
          # using the handy flock command from util-linux.
          'FLOCK_LINK': ['flock', '$TOP_BUILDDIR/linker.lock', '$LINK'],
          'FLOCK_SHLINK': ['flock', '$TOP_BUILDDIR/linker.lock', '$SHLINK'],
          'FLOCK_LDMODULE': ['flock', '$TOP_BUILDDIR/linker.lock', '$LDMODULE'],

          # We have several cases where archives depend on each other in
          # a cyclic fashion.  Since the GNU linker does only a single
          # pass over the archives we surround the libraries with
          # --start-group and --end-group (aka -( and -) ). That causes
          # ld to loop over the group until no more undefined symbols
          # are found. In an ideal world we would only make groups from
          # those libraries which we knew to be in cycles. However,
          # that's tough with SCons, so we bodge it by making all the
          # archives a group by redefining the linking command here.
          #
          # TODO:  investigate whether we still have cycles that
          # require --{start,end}-group.  There has been a lot of
          # refactoring since this was first coded, which might have
          # eliminated the circular dependencies.
          #
          # Note:  $_LIBDIRFLAGS comes before ${LINK,SHLINK,LDMODULE}FLAGS
          # so that we prefer our own built libraries (e.g. -lpng) to
          # system versions of libraries that pkg-config might turn up.
          # TODO(sgk): investigate handling this not by re-ordering the
          # flags this way, but by adding a hook to use the SCons
          # ParseFlags() option on the output from pkg-config.
          'LINKCOM': [['$FLOCK_LINK', '-o', '$TARGET',
                       '$_LIBDIRFLAGS', '$LINKFLAGS', '$SOURCES',
                       '-Wl,--start-group', '$_LIBFLAGS', '-Wl,--end-group']],
          'SHLINKCOM': [['$FLOCK_SHLINK', '-o', '$TARGET',
                         '$_LIBDIRFLAGS', '$SHLINKFLAGS', '$SOURCES',
                         '-Wl,--start-group', '$_LIBFLAGS', '-Wl,--end-group']],
          'LDMODULECOM': [['$FLOCK_LDMODULE', '-o', '$TARGET',
                           '$_LIBDIRFLAGS', '$LDMODULEFLAGS', '$SOURCES',
                           '-Wl,--start-group', '$_LIBFLAGS', '-Wl,--end-group']],
          'IMPLICIT_COMMAND_DEPENDENCIES': 0,
          # -rpath is only used when building with shared libraries.
          'conditions': [
            [ 'library=="shared_library"', {
              'RPATH': '$LIB_DIR',
            }],
          ],
        },
        'scons_import_variables': [
          'AS',
          'CC',
          'CXX',
          'LINK',
        ],
        'scons_propagate_variables': [
          'AS',
          'CC',
          'CCACHE_DIR',
          'CXX',
          'DISTCC_DIR',
          'DISTCC_HOSTS',
          'HOME',
          'INCLUDE_SERVER_ARGS',
          'INCLUDE_SERVER_PORT',
          'LINK',
          'CHROME_BUILD_TYPE',
          'CHROMIUM_BUILD',
          'OFFICIAL_BUILD',
        ],
        'configurations': {
          'Debug': {
            'variables': {
              'debug_optimize%': '0',
            },
            'defines': [
              '_DEBUG',
            ],
            'cflags': [
              '-O>(debug_optimize)',
              '-g',
              # One can use '-gstabs' to enable building the debugging
              # information in STABS format for breakpad's dumpsyms.
            ],
            'ldflags': [
              '-rdynamic',  # Allows backtrace to resolve symbols.
            ],
          },
          'Release': {
            'variables': {
              'release_optimize%': '2',
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
              '-Wl,--gc-sections',
            ],
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
              ['branding=="Chromium"', {
                'cflags': [
                  '-march=pentium4',
                  '-msse2',
                  '-mfpmath=sse',
                ],
              }],
            ],
            'cflags': [
              '-m32',
            ],
            'ldflags': [
              '-m32',
            ],
          }],
          ['target_arch=="arm"', {
            'conditions': [
              ['armv7==1', {
                'target_conditions': [
                  ['_toolset=="target"', {
                    'cflags': [
                      '-march=armv7-a',
                      '-mtune=cortex-a8',
                      '-mfpu=neon',
                      '-mfloat-abi=softfp',
                    ],
                  }],
                ],
              }],
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
          ['no_strict_aliasing==1', {
            'cflags': [
              '-fno-strict-aliasing',
            ],
          }],
          ['linux_breakpad==1', {
            'cflags': [ '-gstabs' ],
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
          }],
          ['linux_use_tcmalloc==1', {
            'defines': ['LINUX_USE_TCMALLOC'],
          }],
        ],
      },
    }],
    ['OS=="mac"', {
      'target_defaults': {
        'variables': {
          # This should be 'mac_real_dsym%', but there seems to be a bug
          # with % in variables that are intended to be set to different
          # values in different targets, like this one.
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
          'WARNING_CFLAGS': ['-Wall', '-Wendif-labels'],
          'conditions': [
            ['chromium_mac_pch', {'GCC_PRECOMPILE_PREFIX_HEADER': 'YES'},
                                 {'GCC_PRECOMPILE_PREFIX_HEADER': 'NO'}
            ],
          ],
        },
        'target_conditions': [
          ['_type!="static_library"', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-search_paths_first']},
          }],
          ['_mac_bundle', {
            'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          }],
          ['_type=="executable" or _type=="shared_library"', {
            'target_conditions': [
              ['mac_real_dsym == 1', {
                # To get a real .dSYM bundle produced by dsymutil, set the
                # debug information format to dwarf-with-dsym.  Since
                # strip_from_xcode will not be used, set Xcode to do the
                # stripping as well.
                'configurations': {
                  'Release': {
                    'xcode_settings': {
                      'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
                      'DEPLOYMENT_POSTPROCESSING': 'YES',
                      'STRIP_INSTALLED_PRODUCT': 'YES',
                      'target_conditions': [
                        ['_type=="shared_library"', {
                          # The Xcode default is to strip debugging symbols
                          # only (-S).  Local symbols should be stripped as
                          # well, which will be handled by -x.  Xcode will
                          # continue to insert -S when stripping even when
                          # additional flags are added with STRIPFLAGS.
                          'STRIPFLAGS': '-x',
                        }],  # _type=="shared_library"
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
          }],  # _type=="executable" or _type=="shared_library"
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
          '_HAS_EXCEPTIONS=0',
          'NOMINMAX',
          '_CRT_RAND_S',
          'CERT_CHAIN_PARA_HAS_EXTRA_FIELDS',
          'WIN32_LEAN_AND_MEAN',
          '_SECURE_ATL',
          '_HAS_TR1=0',
        ],
        'msvs_system_include_dirs': [
          '<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Include',
          '$(VSInstallDir)/VC/atlmfc/include',
        ],
        'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
        'msvs_disabled_warnings': [4396, 4503, 4819],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'MinimalRebuild': 'false',
            'ExceptionHandling': '0',
            'BufferSecurityCheck': 'true',
            'EnableFunctionLevelLinking': 'true',
            'RuntimeTypeInfo': 'false',
            'WarningLevel': '3',
            'WarnAsError': 'true',
            'DebugInformationFormat': '3',
            'conditions': [
              [ 'msvs_multi_core_compile', {
                'AdditionalOptions': '/MP',
              }],
            ],
          },
          'VCLibrarianTool': {
            'AdditionalOptions': '/ignore:4221',
            'AdditionalLibraryDirectories':
              ['<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib'],
          },
          'VCLinkerTool': {
            'AdditionalDependencies': [
              'wininet.lib',
              'version.lib',
              'msimg32.lib',
              'ws2_32.lib',
              'usp10.lib',
              'psapi.lib',
              'dbghelp.lib',
            ],
            'AdditionalLibraryDirectories':
              ['<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib'],
            'GenerateDebugInformation': 'true',
            'MapFileName': '$(OutDir)\\$(TargetName).map',
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'TargetMachine': '1',
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
            'AdditionalIncludeDirectories': ['<(DEPTH)'],
          },
        },
      },
    }],
    ['chromium_code==0', {
      # This section must follow the other condition sections above because
      # external_code.gypi expects to be merged into those settings.
      'includes': [
        'external_code.gypi',
      ],
    }, {
      'target_defaults': {
        # In Chromium code, we define __STDC_FORMAT_MACROS in order to get the
        # C99 macros on Mac and Linux.
        'defines': [
          '__STDC_FORMAT_MACROS',
        ],
        'conditions': [
          ['OS!="win"', {
            'sources/': [ ['exclude', '_win\\.cc$'],
                          ['exclude', '/win/'],
                          ['exclude', '/win_[^/]*\\.cc$'] ],
          }],
          ['OS!="mac"', {
            'sources/': [ ['exclude', '_(cocoa|mac)(_unittest)?\\.cc$'],
                          ['exclude', '/(cocoa|mac)/'],
                          ['exclude', '\.mm$' ] ],
          }],
          ['OS!="linux"', {
            'sources/': [
              ['exclude', '_(chromeos|gtk|linux|x|x11)(_unittest)?\\.cc$'],
              ['exclude', '/gtk/'],
              ['exclude', '/(gtk|x11)_[^/]*\\.cc$'] ],
          }],
          # We use "POSIX" to refer to all non-Windows operating systems.
          ['OS=="win"', {
            'sources/': [ ['exclude', '_posix\\.cc$'] ],
          }],
          # Though Skia is conceptually shared by Linux and Windows,
          # the only _skia files in our tree are Linux-specific.
          ['OS!="linux"', {
            'sources/': [ ['exclude', '_skia\\.cc$'] ],
          }],
          ['chromeos!=1', {
            'sources/': [ ['exclude', '_chromeos\\.cc$'] ]
          }],
          ['OS!="win" and toolkit_views!=1', {
            'sources/': [ ['exclude', '_views\\.cc$'] ]
          }],
        ],
      },
    }],
    ['disable_nacl==1', {
      'target_defaults': {
        'defines': [
          'DISABLE_NACL',
        ],
      },
    }],
    ['msvs_use_common_linker_extras', {
      'target_defaults': {
        'msvs_settings': {
          'VCLinkerTool': {
            'AdditionalOptions':
              '/safeseh /dynamicbase /ignore:4199 /ignore:4221 /nxcompat',
            'DelayLoadDLLs': [
              'dbghelp.dll',
              'dwmapi.dll',
              'uxtheme.dll',
            ],
          },
        },
      },
    }],
  ],
  'scons_settings': {
    'sconsbuild_dir': '<(DEPTH)/sconsbuild',
    'tools': ['ar', 'as', 'gcc', 'g++', 'gnulink', 'chromium_builders'],
  },
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
