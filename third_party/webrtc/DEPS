# This file contains dependencies for WebRTC.

gclient_gn_args_file = 'src/build/config/gclient_args.gni'
gclient_gn_args = [
  'android_ndk_version',
  'generate_location_tags',
]

vars = {
  # Keep the Chromium default for these gn args.
  'generate_location_tags': True,
  'android_ndk_version': Str('2@30.0.14608247'),

  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. More info at: crbug.com/570091.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',
  'chromium_revision': 'e3bb41fa95c2027ea4403b4e5aa8a32892379228',

  # Fetch the prebuilt binaries for llvm-cov and llvm-profdata. Needed to
  # process the raw profiles produced by instrumented targets (built with
  # the gn arg 'use_clang_coverage').
  'checkout_clang_coverage_tools': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  'chromium_git': 'https://chromium.googlesource.com',

  # ResultDB version
  'result_adapter_revision': 'git_revision:5fb3ca203842fd691cab615453f8e5a14302a1d8',

  # By default, download the fuchsia sdk from the public sdk directory.
  'fuchsia_sdk_cipd_prefix': 'fuchsia/sdk/core/',
  'fuchsia_version': 'version:32.20260611.1.1',
  # By default, download the fuchsia images from the fuchsia GCS bucket.
  'fuchsia_images_bucket': 'fuchsia',
  'checkout_fuchsia': False,
  # Since the images are hundreds of MB, default to only downloading the image
  # most commonly useful for developers. Bots and developers that need to use
  # other images can override this with additional images.
  'checkout_fuchsia_boot_images': "terminal.x64",
  'checkout_fuchsia_product_bundles': '"{checkout_fuchsia_boot_images}" != ""',

  # Fetch configuration files required for the 'use_remoteexec' gn arg
  'download_remoteexec_cfg': False,
  # RBE instance to use for running remote builds
  'rbe_instance': 'projects/rbe-webrtc-developer/instances/default_instance',
  # reclient CIPD package version
  'reclient_version': 're_client_version:0.185.0.db415f21-gomaip',
  # siso CIPD package version.
  'siso_version': 'git_revision:0b5eb879ba9260eb6fd7dfe8f9437ef3396185c4',

  # ninja CIPD package.
  'ninja_package': 'infra/3pp/tools/ninja/',

  # ninja CIPD package version
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:3@1.12.1.chromium.4',

  # condition to allowlist deps for non-git-source processing.
  'non_git_source': 'True',

  # This can be overridden, e.g. with custom_vars, to build clang from HEAD
  # instead of downloading the prebuilt pinned revision.
  'llvm_force_head_revision': False,
}

deps = {
  'src/build':
    'https://chromium.googlesource.com/chromium/src/build@44ba1f86183dc23767357ca69406e877020b66ff',
  'src/buildtools':
    'https://chromium.googlesource.com/chromium/src/buildtools@f0ccfb5933f7daa9545159afbb35bdf8951efcc4',
  # Gradle 6.6.1. Used for testing Android Studio project generation for WebRTC.
  'src/examples/androidtests/third_party/gradle': {
    'url': 'https://chromium.googlesource.com/external/github.com/gradle/gradle.git@f2d1fb54a951d8b11d25748e4711bec8d128d7e3',
    'condition': 'checkout_android',
  },
  'src/ios': {
    'url': 'https://chromium.googlesource.com/chromium/src/ios@0a4639dc60ed790b2d7f5091e6e864189a12d3db',
    'condition': 'checkout_ios',
  },
  'src/testing':
    'https://chromium.googlesource.com/chromium/src/testing@6860555cede3652e2fe7699f2b05866a99f014df',
  'src/third_party':
    'https://chromium.googlesource.com/chromium/src/third_party@80d7ea1b85fc5305b835546d9e5df590ed02076e',

  'src/buildtools/third_party/mold/cipd': {
      'packages': [
          {
              'package': 'chromium/buildtools/third_party/mold/mold',
              'version': 'YFd2lmKykUBiJA5LyzCoTRAd_2gnRP5XuXc75Yr-9G0C',
          },
      ],
      'condition': 'host_os == "linux" and non_git_source',
      'dep_type': 'cipd',
  },
  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': 'git_revision:28df2d93df29e5f773dd51ed2f19c112987626ed',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_linux',
  },
  'src/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'git_revision:28df2d93df29e5f773dd51ed2f19c112987626ed',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac',
  },
  'src/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': 'git_revision:28df2d93df29e5f773dd51ed2f19c112987626ed',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_win',
  },
  'src/buildtools/reclient': {
    'packages': [
      {
         # https://chrome-infra-packages.appspot.com/p/infra/rbe/client/
        'package': 'infra/rbe/client/${{platform}}',
        'version': Var('reclient_version'),
      }
    ],
    'dep_type': 'cipd',
    # Reclient doesn't have linux-arm64 package.
    'condition': 'not (host_os == "linux" and host_cpu == "arm64")',
  },

  'src/third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'condition': 'not llvm_force_head_revision',
    'objects': [
      {
        # The Android libclang_rt.builtins libraries are currently only included in the Linux clang package.
        'object_name': 'Linux_x64/clang-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '164dfde4d4de8a7319d95815021942ac030415323b1244408f96c21677b349df',
        'size_bytes': 58951416,
        'generation': 1781115256257063,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '20c6c727d972cc0247893485828f0ff053bea5f39964deb4cc6d0b3202fe2950',
        'size_bytes': 14758824,
        'generation': 1781115256337964,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': 'd9f09d6781f43fcd930ec564279cab3b1c346876448791dc2cc85ac4605a497c',
        'size_bytes': 14978852,
        'generation': 1781115256400080,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '0e7a3c759e1f375963f7ca437d461fe36a36e601c5cc3f77f68c466eba4538af',
        'size_bytes': 2329784,
        'generation': 1781115256651956,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '2f5b517eda80b4b048247db78274c7fb191d738bbc49515d82552db38c127a0c',
        'size_bytes': 5846784,
        'generation': 1781115256522125,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '6ad707a575502501efd449944487a02fd563475dd361d53905ba0fe6c0a82355',
        'size_bytes': 55829844,
        'generation': 1781115258902181,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': 'f7f53fd31237749dbd5ea6069c69ee604f3f6217b37655cd71bfcbbb55d3ee01',
        'size_bytes': 1018152,
        'generation': 1781115268262301,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '9f92524dc48476ed7632c77b43a38da376aa9339bd271774bc11708125dcc62f',
        'size_bytes': 14709364,
        'generation': 1781115258993993,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': 'fbcc605a1ca55b08d7b708177a4695f759e72251121901bf27f15ca6237aea96',
        'size_bytes': 16662192,
        'generation': 1781115259155118,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '288c31b9eb3c2e12489501551b0a251c10ae66bb0f7840654415f4ff5bba28e0',
        'size_bytes': 2373824,
        'generation': 1781115259380183,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '95cf75916e63097162fbc265bdf009a70933f97cf1faa76c4860a0440e9ae2bc',
        'size_bytes': 5766032,
        'generation': 1781115259316003,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '8e527d3642e074838ffb6f321f09101fff01cdc78ecfe49eb055c0505f1ee1de',
        'size_bytes': 47050792,
        'generation': 1781115270422297,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': 'c588e08661765e41138885029838c229bbf9b542cc4f6c491289476ceed5c9ba',
        'size_bytes': 12817220,
        'generation': 1781115270412476,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '94e0a06eb4f93b1b6f602ab9b22a50832b00103ab484c0b147d241ebd0e36d31',
        'size_bytes': 13201040,
        'generation': 1781115270480814,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': 'dba5e29412709da269036af2267cea52f7a77f997fc754936de731e982ff608c',
        'size_bytes': 2001048,
        'generation': 1781115270854909,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '10e80e24573e6b3970db8079d015d944c2c470a7e7f6b5f7c94284ba7953b2c9',
        'size_bytes': 5516172,
        'generation': 1781115270678256,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '2f9d554551fedfebbee6eafb427ccb61c18a87298e9a1f8fbd4185c084c700b7',
        'size_bytes': 51216932,
        'generation': 1781115282148516,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': 'cd8fcb4f305956afb2915a733cbac3ec70bc2eb37f499007745716f30d441070',
        'size_bytes': 14844220,
        'generation': 1781115281957932,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '6d0978aa4dee03726d9f5dab00a0b686ccee7461a4338bfc03fc3138d51ffeed',
        'size_bytes': 2628500,
        'generation': 1781115291333514,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '79a8b66c135d21e2f88b449b0ecda80a09ec79a7cb2c0cc1e7e16f3e9a98ad08',
        'size_bytes': 15255240,
        'generation': 1781115282292038,
        'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': 'b6c65eb3add3c9f5d47d0d03278f408d3608abca10596c3a3c7964532e0be8c4',
        'size_bytes': 2497652,
        'generation': 1781115282583783,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-23-init-18172-g7389aa2e-2.tar.xz',
        'sha256sum': '75368141f971057ce98cf0a51f08136bf92378c36b985172b305457cdd7975d6',
        'size_bytes': 5908032,
        'generation': 1781115282311141,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ]
  },

  # Update prebuilt Rust toolchain.
  'src/third_party/rust-toolchain': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/rust-toolchain-8954863c81df429ebf96ea38a16c76f209995833-1-llvmorg-23-init-18172-g7389aa2e.tar.xz',
        'sha256sum': '59de327414e700ee1564a090054fdfd4d015194aeed1b351866176045476fd9e',
        'size_bytes': 273472068,
        'generation': 1780726784499844,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Mac/rust-toolchain-8954863c81df429ebf96ea38a16c76f209995833-1-llvmorg-23-init-18172-g7389aa2e.tar.xz',
        'sha256sum': '641439618f2f5c01ce0f60be71c9b95818aa84a34b4788774213d9cd632f15c5',
        'size_bytes': 260984128,
        'generation': 1780726786790945,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-8954863c81df429ebf96ea38a16c76f209995833-1-llvmorg-23-init-18172-g7389aa2e.tar.xz',
        'sha256sum': '012fc238b0eb22353a9e8cb4f557d12c8dd0df801c47f1af39b66c40e350fbc5',
        'size_bytes': 245765116,
        'generation': 1780726788814085,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-8954863c81df429ebf96ea38a16c76f209995833-1-llvmorg-23-init-18172-g7389aa2e.tar.xz',
        'sha256sum': 'a18d72e3196899bdcfde4ba936ba2d6c325d6d2ef22d9fe2f77a3990c0c72214',
        'size_bytes': 414331252,
        'generation': 1780726791039486,
        'condition': 'host_os == "win"',
      },
    ],
  },

  'src/third_party/clang-format/script':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@6eddfb5ec5f92127a531eda66c568d3a11e7ec11',
  'src/third_party/compiler-rt/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt.git@bc97b63a261e142e24df48a613a8ff3254f1c856',
  'src/third_party/libc++/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxx.git@5abc7f839700f0f17338434e1c1c6a8c87c00c11',
  'src/third_party/libc++abi/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxxabi.git@8f11bb1d4438d0239d0dfc1bd9456a9f31629dda',
  'src/third_party/llvm-libc/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libc.git@628eb31790a57f897742e219bf1774b15256de69',
  'src/third_party/libunwind/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libunwind.git@d6c7a21e978f0adaa43accaad53bc64f0b64f6ec',

  'src/third_party/test_fonts/test_fonts': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-fonts',
      'objects': [
          {
              'object_name': '9c07d19d9c5ee1ff94f717e6fb17e0c8c354e6f9',
              'sha256sum': 'f0e9628f9e43e3f3476cde06a1849058de460e0e037b7449ce0d42b9a73c37d5',
              'size_bytes': 33413117,
              'generation': 1775663926405386,
          },
      ],
  },

  'src/third_party/ninja': {
    'packages': [
      {
        'package': Var('ninja_package') + '${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'condition': 'non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/siso/cipd': {
    'packages': [
      {
        'package': 'build/siso/${{platform}}',
        'version': Var('siso_version'),
      }
    ],
    'condition': 'non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/android_system_sdk/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_system_sdk/public',
              'version': 'EpgkrtsLblLuw0BrsWCF0h_njBzIZsBNDxQ5VtA4s2UC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/tools/resultdb': {
    'packages': [
      {
        'package': 'infra/tools/result_adapter/${{platform}}',
        'version': Var('result_adapter_revision'),
      },
    ],
    'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/aapt2/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 'Aus5yjlxSSJKixA3AYNZmMOTauyAQUFiTlSKhGpacJYC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/bundletool/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/bundletool',
               'version': '7Vo6ZzIxIaC51ATTBlo_KUkgxJCmmGmXAijlVkXUTpAC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/dagger_compiler/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/dagger_compiler',
               'version': 'AC0DoTEXQf40KFt7hyCNSEJPrT9Rprw9zsZxNKdw7BQC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/error_prone/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone',
               'version': 'UXPQGFDgRKIPBtMqYAE1Jo4hZAe1cMHdpiAnAkAe2d4C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/error_prone_javac/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone_javac',
               'version': '7EcHxlEXEaLRWEyHIAxf0ouPjkmN1Od6jkutuo0sfBIC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/lint/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/lint',
               'version': 'UBV1Cd_9gG4yu9gqNmDC8_SA6pBLEwEDQrgGAaCl1a8C',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },


  'src/third_party/android_build_tools/nullaway/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/nullaway',
               'version': 'ds9Vm6LkQNc9O9nuG0_FbrsNQ5VoGPpFIWKur53l3wUC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/aosp_dalvik/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/aosp_dalvik/linux-amd64',
              'version': 'version:2@13.0.0_r24.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/boringssl/src':
    'https://boringssl.googlesource.com/boringssl.git@a30f663a47ad706852cd3a8c923a4981c2244f36',
  'src/third_party/breakpad/breakpad':
    'https://chromium.googlesource.com/breakpad/breakpad.git@5359c2336d74399e01c420dcc8034bf50cbe44bc',
  'src/third_party/catapult':
    'https://chromium.googlesource.com/catapult.git@ed9e3961c0dcf542f06fd195c2220dd45f7ccb7e',
  'src/third_party/ced/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/compact_enc_det.git@d127078cedef9c6642cbe592dacdd2292b50bb19',
  },
  'src/third_party/colorama/src':
    'https://chromium.googlesource.com/external/colorama.git@3de9f013df4b470069d03d250224062e8cf15c49',
  'src/third_party/cpu_features/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/cpu_features.git@81d13c49649f0714dd41fb56bb246398b6584085',
    'condition': 'checkout_android',
  },
  'src/third_party/crc32c/src':
    'https://chromium.googlesource.com/external/github.com/google/crc32c.git@2bbb3be42e20a0e6c0f7b39dc07dc863d9ffbc07',
  'src/third_party/depot_tools':
    'https://chromium.googlesource.com/chromium/tools/depot_tools.git@10eda50a3fd9c34ad8d31ec74e5f4eb5823d60f6',
  'src/third_party/ffmpeg':
    'https://chromium.googlesource.com/chromium/third_party/ffmpeg.git@ad41607c61898cf7150e0fb20fe4bbabd44922a3',
  'src/third_party/flatbuffers/src':
    'https://chromium.googlesource.com/external/github.com/google/flatbuffers.git@a86afae9399bbe631d1ea0783f8816e780e236cc',
  'src/third_party/grpc/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/grpc/grpc.git@8e43f7e85c55ce08911be5c22511afddb4018e2b',
  },
  # Used for embedded builds. CrOS & Linux use the system version.
  'src/third_party/fontconfig/src': {
      'url': 'https://chromium.googlesource.com/external/fontconfig.git@d62c2ab268d1679335daa8fb0ea6970f35224a76',
      'condition': 'checkout_linux',
  },
  'src/third_party/freetype/src':
    'https://chromium.googlesource.com/chromium/src/third_party/freetype2.git@b08a2eb0dd37f4a6c886fa5b0ecf5b3e1d27aac7',
  'src/third_party/harfbuzz/src':
    'https://chromium.googlesource.com/external/github.com/harfbuzz/harfbuzz.git@6dda1a083cb1285e8a2fbea7aa3e3c980ab719e7',
  'src/third_party/google_benchmark/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/benchmark.git@8abf1e701fbd88c8170f48fe0558247e2e5f8e7d',
  },
  'src/third_party/libpfm4/src':
    Var('chromium_git') + '/external/git.code.sf.net/p/perfmon2/libpfm4.git' + '@' + '6870a9f00412830ceaa7e4384bb92ee323e2a28f',
  # WebRTC-only dependency (not present in Chromium).
  'src/third_party/gtest-parallel':
    'https://chromium.googlesource.com/external/github.com/google/gtest-parallel@cd488bdedc1d2cffb98201a17afc1b298b0b90f1',
  'src/third_party/google-truth/src': {
      'url': 'https://chromium.googlesource.com/external/github.com/google/truth.git@2b500d6f97654261163e6cc7848cb826bbec21c1',
      'condition': 'checkout_android',
  },
  'src/third_party/googletest/src':
    'https://chromium.googlesource.com/external/github.com/google/googletest.git@4fe3307fb2d9f86d19777c7eb0e4809e9694dde7',
  'src/third_party/icu': {
    'url': 'https://chromium.googlesource.com/chromium/deps/icu.git@d578f2e8b7bd5938e21cfb6bf15c079e0aa5b738',
  },
  'src/third_party/jdk/current': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk/linux-amd64',
              'version': '2iiuF-nKDH3moTImx2op4WTRetbfhzKoZhH7Xo44zGsC',
          },
      ],
      # Needed on Linux for use on chromium_presubmit (for checkstyle).
      'condition': '(checkout_android or checkout_linux) and non_git_source',
      'dep_type': 'cipd',
  },
  # Deprecated - only use for tools which are broken real JDK.
  # Not used by WebRTC. Added for compatibility with Chromium.
  'src/third_party/jdk11': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk',
              # Do not update this hash - any newer hash will point to JDK17+.
              'version': 'egbcSHbmF1XZQbKxp_PQiGLFWlQK65krTGqQE-Bj4j8C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
 'src/third_party/jsoncpp/source':
    'https://chromium.googlesource.com/external/github.com/open-source-parsers/jsoncpp.git@5f1f240f10a19a61929b5c573974900cb62e9dac', # from svn 248
  'src/third_party/junit/src': {
    'url': 'https://chromium.googlesource.com/external/junit.git@0eb5ce72848d730da5bd6d42902fdd6a8a42055d',
    'condition': 'checkout_android',
  },
  'src/third_party/kotlin_stdlib/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlin_stdlib',
              'version': 'Txl5_fPKdqBa5jOHVYQ8e6FAYb6KqQHfuJ8dyK5BH3QC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/kotlinc/current': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlinc',
              'version': 'sK_DveKnfu2_RtILlaU_v3sbcNIxxphNaZRTZm2GuToC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/libFuzzer/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git@bea408a6e01f0f7e6c82a43121fe3af4506c932e',
  'src/third_party/fuzztest/src':
    'https://chromium.googlesource.com/external/github.com/google/fuzztest.git@0798a6305ea7b09febe2f655255d6344044a73ad',
  'src/third_party/libjpeg_turbo':
    'https://chromium.googlesource.com/chromium/deps/libjpeg_turbo.git@640f254ad0fa03f6b1f29f89b7dd9366f2f6e533',
  'src/third_party/libsrtp':
    'https://chromium.googlesource.com/chromium/deps/libsrtp.git@cd5d177bf1fde755ddb4c7f0d9ff7693f8b49e5e',
  'src/third_party/dav1d/libdav1d':
    'https://chromium.googlesource.com/external/github.com/videolan/dav1d.git@e9c5800ace888f1f10a66f7ffb8043a9c96d2508',
  'src/third_party/libaom/source/libaom':
    'https://aomedia.googlesource.com/aom.git@f8f91ecd56f5bc38f401853a48e1b233966085e4',
  'src/third_party/libgav1/src':
    Var('chromium_git') + '/codecs/libgav1.git' + '@' + '66ac17620652635392f6ab24065c77b035e281c9',
  'src/third_party/libunwindstack': {
      'url': 'https://chromium.googlesource.com/chromium/src/third_party/libunwindstack.git@333fcafb91bd3830c5ef814c071ff73df9cdc976',
      'condition': 'checkout_android',
  },
  'src/third_party/perfetto':
    Var('chromium_git') + '/external/github.com/google/perfetto.git' + '@' + 'edecd0480cca31bb45f7c2819bc54d05bb9af908',
  'src/third_party/protobuf-javascript/src':
    Var('chromium_git') + '/external/github.com/protocolbuffers/protobuf-javascript' + '@' + 'e6d763860001ba1a76a63adcff5efb12b1c96024',
  'src/third_party/libvpx/source/libvpx':
    'https://chromium.googlesource.com/webm/libvpx.git@9794f877a3e380ecd57b735b81fe94ab368f6748',
  'src/third_party/libyuv':
    'https://chromium.googlesource.com/libyuv/libyuv.git@d23308a2a7442be8e559b1b471862fd7588d6a57',
  'src/third_party/lss': {
    'url': 'https://chromium.googlesource.com/linux-syscall-support.git@29164a80da4d41134950d76d55199ea33fbb9613',
    'condition': 'checkout_android or checkout_linux',
  },
  'src/third_party/mockito/src': {
    'url': 'https://chromium.googlesource.com/external/mockito/mockito.git@04a2a289a4222f80ad20717c25144981210d2eac',
    'condition': 'checkout_android',
  },
  'src/third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' + '@' + 'e8cb570a9a2ee9128e2214c73417ad2a3c47780b',
    'condition': 'checkout_instrumented_libraries',
  },

  # Used by boringssl.
  'src/third_party/nasm': {
      'url': 'https://chromium.googlesource.com/chromium/deps/nasm.git@525a09a813be0f75b646ee93fc2a31c27b87d722'
  },

  'src/third_party/openh264/src':
    'https://chromium.googlesource.com/external/github.com/cisco/openh264@652bdb7719f30b52b08e506645a7322ff1b2cc6f',

  'src/third_party/re2/src':
    'https://chromium.googlesource.com/external/github.com/google/re2.git@972a15cedd008d846f1a39b2e88ce48d7f166cbd',

  'src/third_party/sframe/src':
    Var('chromium_git') + '/external/github.com/cisco/sframe' + '@' + 'b14090904433bed0d4ec3f875b9b39f3e0555930',

  'src/third_party/r8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'NyzXx3BYfzk2Pb7-q1eNk126IutSr96soPXBXH6BrzoC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  # This duplication is intentional, so we avoid updating the r8.jar used by
  # dexing unless necessary, since each update invalidates all incremental
  # dexing and unnecessarily slows down all bots.
  'src/third_party/r8/d8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'NyzXx3BYfzk2Pb7-q1eNk126IutSr96soPXBXH6BrzoC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/requests/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/kennethreitz/requests.git@c7e0fc087ceeadb8b4c84a0953a422c474093d6d',
    'condition': 'checkout_android',
  },
  'src/tools':
    'https://chromium.googlesource.com/chromium/src/tools@72a9404c957e137720672ec18e62ae5b920c7c26',

  'src/third_party/espresso': {
      'packages': [
          {
              'package': 'chromium/third_party/espresso',
              'version': '5LoBT0j383h_4dXbnap7gnNQMtMjpbMJD1JaGIYNj-IC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/hamcrest/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/hamcrest',
              'version': 'dBioOAmFJjqAr_DY7dipbXdVfAxUQwjOBNibMPtX8lQC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_toolchain/ndk': {
    'packages': [
      {
        'package': 'chromium/third_party/android_toolchain/android_toolchain',
        'version': 'version:2@30.0.14608247',
      },
    ],
    'condition': 'checkout_android',
    'dep_type': 'cipd',
  },

  'src/third_party/androidx/cipd': {
    'packages': [
      {
          'package': 'chromium/third_party/androidx',
          'version': 'rO584iETQkpleG9Y6EERCmFHbtQUTb3P37MUxw3vdn8C',
      },
    ],
    'condition': 'checkout_android and non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/manifest_merger/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/manifest_merger',
               'version': 'tuRQsN9mLsB8LZDP1tihK_EkR8xqR_u7v-SCMfTkyBAC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/37.0.0',
              'version': 'febJrTgiK9s1ANoUlc4Orn3--zs9GjGCj2vQc8g7OaMC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': '9lGp8nTUCRRWGMnI_96HcKfzjnxEJKUcfvfwmA3wXNkC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': 'qTD9QdBlBf3dyHsN1lJ0RH6AhHxR42Hmg2Ih-Vj4zIEC'
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-37.0',
              'version': 'WhtP32Q46ZHdTmgCgdauM3ws_H9iPoGKEZ_cPggcQ6wC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools/linux',
              'version': 'LZa8CWNVWS6UUQgQ7IJdFCqRV1Bmx2-alTNqEDJpJkcC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/icu4j/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/icu4j',
              'version': '8dV7WRVX0tTaNNqkLEnCA_dMofr2MJXFK400E7gOFygC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/robolectric/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/robolectric',
              'version': 'dr-aJxRAPYDTBJXnjfht-bdxyywD6BP1lrcjZZPnRG0C',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/sqlite4java/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/sqlite4java',
              'version': 'LofjKH9dgXIAJhRYCPQlMFywSwxYimrfDeBmaHc-Z5EC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/tflite/src':
    Var('chromium_git') + '/external/github.com/tensorflow/tensorflow.git' + '@' + '55025ab1474f86f31c778837b5e188ea53736c38',

  'src/third_party/turbine/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'B0beWpVLQ44lPgNCvSi0VNmDvyPNyWlhI_VIWV6ZllkC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/zstd/src': {
    'url': Var('chromium_git') + '/external/github.com/facebook/zstd.git' + '@' + '5233c58e6ca0b1c4c6b353ad79649191ed195bdc',
    'condition': 'checkout_android',
  },

  'src/tools/luci-go': {
      'packages': [
        {
          'package': 'infra/tools/luci/cas/${{platform}}',
          'version': 'git_revision:288f56893bcbd07c840fc47f8dce8b551d4f37ee',
        },
        {
          'package': 'infra/tools/luci/isolate/${{platform}}',
          'version': 'git_revision:288f56893bcbd07c840fc47f8dce8b551d4f37ee',
        },
        {
          'package': 'infra/tools/luci/swarming/${{platform}}',
          'version': 'git_revision:288f56893bcbd07c840fc47f8dce8b551d4f37ee',
        }
      ],
      'dep_type': 'cipd',
  },

  'src/third_party/pipewire/linux-amd64': {
    'packages': [
      {
        'package': 'chromium/third_party/pipewire/linux-amd64',
        'version': 'WccKnxvPRn8TBwQ4FUxoHaZRKvHXh0qJjODccy3gPTAC',
      },
      {
        'package': 'chromium/third_party/wireplumber/linux-amd64',
        'version': 'yfe349C2e6pWCcu7DaSPJHNWdwDbNP9FfSQoV9j6A5UC',
      },
    ],

    'condition': 'checkout_linux',
    'dep_type': 'cipd',
  },

  'src/third_party/android_deps/autorolled/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/autorolled',
              'version': 'Fr1ifGTI3QL92dGgX1luSuZABojGeVWyXZhpDoyp6AkC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },
  'src/third_party/pthreadpool/src':
    Var('chromium_git') + '/external/github.com/google/pthreadpool.git' + '@' + '02460584c6092e527c8b89f7df4de143d70e801f',

  'src/third_party/xnnpack/src':
    Var('chromium_git') + '/external/github.com/google/XNNPACK.git' + '@' + 'f6486e3e1dbba03be9a493c8d6ead20c787c9886',

  'src/third_party/farmhash/src':
    Var('chromium_git') + '/external/github.com/google/farmhash.git' + '@' + '816a4ae622e964763ca0862d9dbd19324a1eaf45',

  'src/third_party/ruy/src':
    Var('chromium_git') + '/external/github.com/google/ruy.git' + '@' + '2af88863614a8298689cc52b1a47b3fcad7be835',

  'src/third_party/cpuinfo/src':
    Var('chromium_git') + '/external/github.com/pytorch/cpuinfo.git' + '@' + 'ea6b9f1bb6e1001d8b21574d5bc78ddef62e499d',

  'src/third_party/eigen3/src':
    Var('chromium_git') + '/external/gitlab.com/libeigen/eigen.git' + '@' + 'f720207f7fce5a847adf7c3e5d7a5ff2529d1e16',

  'src/third_party/fp16/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FP16.git' + '@' + '3d2de1816307bac63c16a297e8c4dc501b4076df',

  'src/third_party/gemmlowp/src':
    Var('chromium_git') + '/external/github.com/google/gemmlowp.git' + '@' + '16e8662c34917be0065110bfcd9cc27d30f52fdf',

  'src/third_party/fxdiv/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FXdiv.git' + '@' + '63058eff77e11aa15bf531df5dd34395ec3017c8',

  'src/third_party/neon_2_sse/src':
    Var('chromium_git') + '/external/github.com/intel/ARM_NEON_2_x86_SSE.git' + '@' + 'ed59be8546632d5126ff69c87122ae5de20ffe4f',


  # Everything coming after this is automatically updated by the auto-roller.
  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //third_party/android_deps/fetch_all.py
  'src/third_party/android_deps/cipd/libs/com_android_tools_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_common',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_android_tools_layoutlib_layoutlib_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_android_tools_sdk_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_sdk_common',
              'version': 'version:2@30.2.0-beta01.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework',
              'version': 'version:2@4.0.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:2@1.3.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:2@1.13.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy',
              'version': 'version:2@1.17.6.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy_agent': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy_agent',
              'version': 'version:2@1.17.6.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:2@1.2.1.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2@2.5.5.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime',
              'version': 'version:2@1.9.22.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime',
              'version': 'version:2@1.9.22.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jsoup_jsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jsoup_jsoup',
              'version': 'version:2@1.15.1.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_android',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_core',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_subclass': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_subclass',
              'version': 'version:2@5.19.0.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_objenesis_objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_objenesis_objenesis',
              'version': 'version:2@3.3.cr2',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  # === ANDROID_DEPS Generated Code End ===
}

hooks = [
  {
    # This clobbers when necessary (based on get_landmines.py). It should be
    # an early hook but it will need to be run after syncing Chromium and
    # setting up the links, so the script actually exists.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python3',
        'src/build/landmines.py',
        '--landmine-scripts',
        'src/tools_webrtc/get_landmines.py',
        '--src-dir',
        'src',
    ],
  },
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [
        'python3',
        'src/third_party/depot_tools/update_depot_tools_toggle.py',
        '--disable',
    ],
  },
  {
    'name': 'sysroot_arm',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm'],
  },
  {
    'name': 'sysroot_arm64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_arm64',
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=arm64'],
  },
  {
    'name': 'sysroot_x86',
    'pattern': '.',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64)',
    # TODO(mbonadei): change to --arch=x86.
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=i386'],
  },
  {
    'name': 'sysroot_mips',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_mips',
    # TODO(mbonadei): change to --arch=mips.
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=mipsel'],
  },
  {
    'name': 'sysroot_x64',
    'pattern': '.',
    'condition': 'checkout_linux and checkout_x64',
    # TODO(mbonadei): change to --arch=x64.
    'action': ['python3', 'src/build/linux/sysroot_scripts/install-sysroot.py',
               '--arch=amd64'],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'src/build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary. Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', 'src/build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python3', 'src/build/mac_toolchain.py'],
  },

  {
    'name': 'Download Fuchsia SDK from GCS',
    'pattern': '.',
    'condition': 'checkout_fuchsia',
    'action': [
      'python3',
      'src/build/fuchsia/update_sdk.py',
      '--cipd-prefix={fuchsia_sdk_cipd_prefix}',
      '--version={fuchsia_version}',
    ],
  },
  {
    'name': 'Download Fuchsia system images',
    'pattern': '.',
    'condition': 'checkout_fuchsia and checkout_fuchsia_product_bundles',
    'action': [
      'python3',
      'src/build/fuchsia/update_product_bundles.py',
      '{checkout_fuchsia_boot_images}',
    ],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python3', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE'],
  },
  # Pull dsymutil binaries using checked-in hashes.
  {
    'name': 'dsymutil_mac_arm64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'src/tools/clang/dsymutil/bin/dsymutil.arm64.sha1',
                '-o', 'src/tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'dsymutil_mac_x64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'src/tools/clang/dsymutil/bin/dsymutil.x64.sha1',
                '-o', 'src/tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  # Pull rc binaries using checked-in hashes.
  {
    'name': 'rc_win',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "win"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/win/rc.exe.sha1',
    ],
  },
  {
    'name': 'rc_mac',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "mac"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/mac/rc.sha1',
    ],
  },
  {
    'name': 'rc_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/rc',
                '-s', 'src/build/toolchain/win/rc/linux64/rc.sha1',
    ],
  },
  {
    # Download test resources, i.e. video and audio files from Google Storage.
    'pattern': '.',
    'action': ['download_from_google_storage',
               '--directory',
               '--recursive',
               '--num_threads=10',
               '--quiet',
               '--bucket', 'chromium-webrtc-resources',
               'src/resources'],
  },
  {
    'name': 'Generate component metadata for tests',
    'pattern': '.',
    'action': [
      'vpython3',
      'src/testing/generate_location_tags.py',
      '--out',
      'src/testing/location_tags.json',
    ],
  },
  # Download and initialize "vpython" VirtualEnv environment packages.
  {
    'name': 'vpython_common',
    'pattern': '.',
    'action': [ 'vpython3',
                '-vpython-spec', 'src/.vpython3',
                '-vpython-tool', 'install',
    ],
  },
  # Download remote exec cfg files
  {
    'name': 'configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg',
    'action': ['python3',
               'src/buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--quiet',
               ],
  },
  # Configure Siso for developer builds.
  {
    'name': 'configure_siso',
    'pattern': '.',
    'action': ['python3',
               'src/build/config/siso/configure_siso.py',
               '--rbe_instance',
               Var('rbe_instance'),
               ],
  },
  {
    # Ensure we remove any file from disk that is no longer needed (e.g. after
    # hooks to native GCS deps migration).
    'name': 'remove_stale_files',
    'pattern': '.',
    'action': [
        'python3',
        'src/tools/remove_stale_files.py',
        'src/third_party/test_fonts/test_fonts.tar.gz', # Remove after 20240901
    ],
  },
]

recursedeps = [
  'src/buildtools',
  'src/third_party/instrumented_libs',
]

# Define rules for which include paths are allowed in our source.
include_rules = [
  # Base is only used to build Android APK tests and may not be referenced by
  # WebRTC production code.
  "-base",
  "-chromium",
  "+external/webrtc/webrtc",  # Android platform build.
  "+libyuv",

  # These should eventually move out of here.
  "+common_types.h",

  "+WebRTC",
  "+api",
  "+modules/include",
  "+rtc_base",
  "+test",
  "+rtc_tools",

  # Abseil allowlist. Keep this in sync with abseil-in-webrtc.md.
  "+absl/algorithm/algorithm.h",
  "+absl/algorithm/container.h",
  "+absl/base/attributes.h",
  "+absl/base/config.h",
  "+absl/base/nullability.h",
  "+absl/base/macros.h",
  "+absl/base/no_destructor.h",
  "+absl/cleanup/cleanup.h",
  "+absl/container",
  "-absl/container/fixed_array.h",
  "+absl/crc",
  "+absl/functional/any_invocable.h",
  "+absl/functional/bind_front.h",
  "+absl/memory/memory.h",
  "+absl/numeric/bits.h",
  "+absl/strings/ascii.h",
  "+absl/strings/escaping.h",
  "+absl/strings/match.h",
  "+absl/strings/str_cat.h",  # note - allowed for single argument version only
  "+absl/strings/str_replace.h",
  "+absl/strings/string_view.h",

  # Abseil flags are allowed in tests and tools.
  "+absl/flags",

  # Perfetto should be used through rtc_base/trace_event.h
  '-third_party/perfetto',
  '-perfetto',
  '-protos/perfetto',
]

specific_include_rules = {
  "webrtc_lib_link_test\\.cc": [
    "+media/engine",
    "+modules/audio_device",
    "+modules/audio_processing",
  ]
}