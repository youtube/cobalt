# This file contains dependencies for WebRTC.

gclient_gn_args_file = 'src/build/config/gclient_args.gni'
gclient_gn_args = [
  'generate_location_tags',
]

vars = {
  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. More info at: crbug.com/570091.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',
  'chromium_revision': 'fc67a3a940ac5169ef7f32434bc80f2ebbe3095d',

  # Fetch the prebuilt binaries for llvm-cov and llvm-profdata. Needed to
  # process the raw profiles produced by instrumented targets (built with
  # the gn arg 'use_clang_coverage').
  'checkout_clang_coverage_tools': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  'chromium_git': 'https://chromium.googlesource.com',

  # Keep the Chromium default of generating location tags.
  'generate_location_tags': True,

  # ResultDB version
  'result_adapter_revision': 'git_revision:5fb3ca203842fd691cab615453f8e5a14302a1d8',

  # By default, download the fuchsia sdk from the public sdk directory.
  'fuchsia_sdk_cipd_prefix': 'fuchsia/sdk/core/',
  'fuchsia_version': 'version:29.20250730.5.1',
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
  'reclient_version': 're_client_version:0.179.0.28341fc7-gomaip',
  # siso CIPD package version.
  'siso_version': 'git_revision:887a10bc8a038d8e330d06d1ad5ae6ec646f3040',

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
    'https://chromium.googlesource.com/chromium/src/build@5f872b88fac8c0bfb0ee8e1ca384aa26270be070',
  'src/buildtools':
    'https://chromium.googlesource.com/chromium/src/buildtools@16b1d7d539c0ea4a367bc4860f9002a0bb6c790e',
  # Gradle 6.6.1. Used for testing Android Studio project generation for WebRTC.
  'src/examples/androidtests/third_party/gradle': {
    'url': 'https://chromium.googlesource.com/external/github.com/gradle/gradle.git@f2d1fb54a951d8b11d25748e4711bec8d128d7e3',
    'condition': 'checkout_android',
  },
  'src/ios': {
    'url': 'https://chromium.googlesource.com/chromium/src/ios@94138f0e5734efd90a024a61ce9da4618f5177aa',
    'condition': 'checkout_ios',
  },
  'src/testing':
    'https://chromium.googlesource.com/chromium/src/testing@64cb343cf0a195ce9d7551d95d36666552d7682a',
  'src/third_party':
    'https://chromium.googlesource.com/chromium/src/third_party@10837a15d5dfab73ff3205f4b1ee026d3c56d5af',

  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': 'git_revision:cc7c61e105fb56183fd6c5c52bbe3c37ff724a0d',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_linux',
  },
  'src/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': 'git_revision:cc7c61e105fb56183fd6c5c52bbe3c37ff724a0d',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac',
  },
  'src/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': 'git_revision:cc7c61e105fb56183fd6c5c52bbe3c37ff724a0d',
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
        'object_name': 'Linux_x64/clang-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': 'b448a78b42740352a37b458115a9482f213097bbf28f1f839e72cab829bdfa6d',
        'size_bytes': 55133120,
        'generation': 1753983011666559,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '98da47515c1f996bdf884487afff4cca0bbbd6a23347b5fc9442ba4210ca66a2',
        'size_bytes': 13659920,
        'generation': 1753983011671003,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': 'e133b8b139770ab170832e1fe68d03453bec0315dbc53ffa6928a61d0f348d41',
        'size_bytes': 13852732,
        'generation': 1753983011663674,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '2a61faffe37a31330cd825b567c4a422724b8d35426d79d4ac265f07ab8ecedf',
        'size_bytes': 2314532,
        'generation': 1753983011700114,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '1e4be8737102bff65c33e37867d28960b297bdd718fb43d544e5a977aa73e58e',
        'size_bytes': 5675780,
        'generation': 1753983011751371,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '0cfd4ac1d5c922f3868b7d0f59731596c9f7651541a4e2cd32fcae91cb707389',
        'size_bytes': 52423592,
        'generation': 1753983013393204,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '6267cf2fdd697c4f5ff60c1ea949dcde2c5638c86c254e410fd8093d8ba88854',
        'size_bytes': 995072,
        'generation': 1753983021140243,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': 'cb6aeb04f059f1d504efb4a8e018ce3dd6e71c9ca3b37e07974c1bce9a473444',
        'size_bytes': 13738952,
        'generation': 1753983013426743,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': 'd4bd53a928a682b68d9f268ef2a7e5fc098090a051944bd3bd293f34920bfa1a',
        'size_bytes': 15157732,
        'generation': 1753983013465928,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': 'a3aa1c0792baa8aba7e91fb9d72ec80c2fe6bb88ced406660f23e5b93c40e88c',
        'size_bytes': 2282708,
        'generation': 1753983013526049,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '3ee5d49e92ad6cd1e532118f0713532962d6ef14aa119f81a893b72c95046ac6',
        'size_bytes': 5484504,
        'generation': 1753983013468350,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': 'afed7bb50e1e7f25472f3a43e942a774c3ef5dcc31a1a9cf817224aec411f4fb',
        'size_bytes': 44337520,
        'generation': 1753983022809149,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '51e2940861747013b11833da69821364552776e29be54287e7d8b299c35357de',
        'size_bytes': 11873540,
        'generation': 1753983022864078,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '3344e7ed3d42be6c827502e2c4addbae42dfbe9699e5e391649fdf53ce3d16d7',
        'size_bytes': 12136100,
        'generation': 1753983022893296,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '59709d0a6bc2f1930c0971b4f0cf4d8932d1321621cff570419342ee396cc43e',
        'size_bytes': 1989132,
        'generation': 1753983022902753,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '76f2d491e7535e6051f3a6bc4a6948602a8e4a231882305e13c9adb9ce6af125',
        'size_bytes': 5313688,
        'generation': 1753983022869504,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '418c186740c926b9ba85c173540ffa3ed459e3fab06ee45fe524f9b480c2a7a6',
        'size_bytes': 47378940,
        'generation': 1753983032535097,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '0865195b3efdaab630f9acb06d4a4e414813752b28f7511dddf3cd69e38df4a0',
        'size_bytes': 13488148,
        'generation': 1753983032729035,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '2a7678deda052620fa8aff586922d1607d1e39e51726dee9e9325a668c495df4',
        'size_bytes': 2506544,
        'generation': 1753983040294404,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '79a70ce9405ed33f4dfddb4d3be82d827964fb79b898b82f892d9855d5dd3195',
        'size_bytes': 13904768,
        'generation': 1753983032665184,
        'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': 'b57dddb09757df2bb2bfd83e57a87c3f1da72a700afb66e853f60fe0102e1ba3',
        'size_bytes': 2384644,
        'generation': 1753983032706862,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-21-init-16348-gbd809ffb-15.tar.xz',
        'sha256sum': '4fd02b442dcb5cc8f4a80e4c8e6aeb972dc0163a8ee79b3ce3416d9cc24cb137',
        'size_bytes': 5654156,
        'generation': 1753983032704697,
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
        'object_name': 'Linux_x64/rust-toolchain-22be76b7e259f27bf3e55eb931f354cd8b69d55f-3-llvmorg-21-init-16348-gbd809ffb.tar.xz',
        'sha256sum': '5f8e9ad847e5bf586e0de1bb563c9a49e05ad36edfad5037900d7510004fc577',
        'size_bytes': 138573136,
        'generation': 1750840933611077,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Mac/rust-toolchain-22be76b7e259f27bf3e55eb931f354cd8b69d55f-3-llvmorg-21-init-16348-gbd809ffb.tar.xz',
        'sha256sum': '357db812ca0a518ef0fc4394ddc859d68f23384931294412b7424bb3aabb3c09',
        'size_bytes': 132392604,
        'generation': 1750840935469331,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-22be76b7e259f27bf3e55eb931f354cd8b69d55f-3-llvmorg-21-init-16348-gbd809ffb.tar.xz',
        'sha256sum': 'd3cb60c6388e86d3d1a0c46c539f1ea0ed1ff48cf907dc21b2cb5ff441b23c03',
        'size_bytes': 120354192,
        'generation': 1750840937280735,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-22be76b7e259f27bf3e55eb931f354cd8b69d55f-3-llvmorg-21-init-16348-gbd809ffb.tar.xz',
        'sha256sum': '7e804f3a8bef4c8ca32d3368ca7564e5c12b684899453d9a522bdd05b1f1df7b',
        'size_bytes': 195000356,
        'generation': 1750840939064273,
        'condition': 'host_os == "win"',
      },
    ],
  },

  'src/third_party/clang-format/script':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@37f6e68a107df43b7d7e044fd36a13cbae3413f2',
  'src/third_party/compiler-rt/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt.git@693bb2c75d48037e5ce35c61d3cbde3cf59622b6',
  'src/third_party/libc++/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxx.git@6f8fe913d64254ab2231e0e98d2b6b5650ef4917',
  'src/third_party/libc++abi/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libcxxabi.git@a6c815c69d55ec59d020abde636754d120b402ad',
  'src/third_party/llvm-libc/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libc.git@3d02389abeb19c6e871de64dd04ec3c4412c2cbc',
  'src/third_party/libunwind/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/libunwind.git@84c5262b57147e9934c0a8f2302d989b44ec7093',

  'src/third_party/test_fonts/test_fonts': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-fonts',
      'objects': [
          {
              'object_name': 'a28b222b79851716f8358d2800157d9ffe117b3545031ae51f69b7e1e1b9a969',
              'sha256sum': 'a28b222b79851716f8358d2800157d9ffe117b3545031ae51f69b7e1e1b9a969',
              'size_bytes': 33233004,
              'generation': 1746454415712520,
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
        'package': 'infra/build/siso/${{platform}}',
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
              'version': 'Pfb3HDUW_uRir_VVTCYkGhf6bnPPF55NUJO2WXOxIe0C',
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
              'version': '2Tff49a5dYTM5YvxxqyOsTQ9k8nOqM4ayydbCuKoxIwC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/bundletool/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/bundletool',
               'version': 'zV93G9_1s5h6x7c2qdcibr0uuQ_5Q2QgcxhkUs9-tOsC',
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
               'version': '56Xo0VEDkKxzOr5L7pafI6SaIY2S7wnOOcgy4CNSANEC',
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
               'version': 'AGywP4kNoPZjZa3fVdf1g2cKgEPkXnVFRxhlFjroXJcC',
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
    'https://boringssl.googlesource.com/boringssl.git@6cd35e8ba6aa0b8bb3eaff35a7aa6b6385e34fa0',
  'src/third_party/breakpad/breakpad':
    'https://chromium.googlesource.com/breakpad/breakpad.git@ff252ff6faf5e3a52dc4955aab0d84831697dc94',
  'src/third_party/catapult':
    'https://chromium.googlesource.com/catapult.git@baad9d05b2fcf3d8ea9b7983f1868460df40d14a',
  'src/third_party/ced/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/compact_enc_det.git@ba412eaaacd3186085babcd901679a48863c7dd5',
  },
  'src/third_party/colorama/src':
    'https://chromium.googlesource.com/external/colorama.git@3de9f013df4b470069d03d250224062e8cf15c49',
  'src/third_party/cpu_features/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/cpu_features.git@936b9ab5515dead115606559502e3864958f7f6e',
    'condition': 'checkout_android',
  },
  'src/third_party/crc32c/src':
    'https://chromium.googlesource.com/external/github.com/google/crc32c.git@d3d60ac6e0f16780bcfcc825385e1d338801a558',
  'src/third_party/depot_tools':
    'https://chromium.googlesource.com/chromium/tools/depot_tools.git@55b7745daa200ac8f8956da2478841335168d874',
  'src/third_party/ffmpeg':
    'https://chromium.googlesource.com/chromium/third_party/ffmpeg.git@d2d06b12c22d27af58114e779270521074ff1f85',
  'src/third_party/flatbuffers/src':
    'https://chromium.googlesource.com/external/github.com/google/flatbuffers.git@8db59321d9f02cdffa30126654059c7d02f70c32',
  'src/third_party/grpc/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/grpc/grpc.git@c8c673c99a533cce8de6fead337865b0da3cae9b',
  },
  # Used for embedded builds. CrOS & Linux use the system version.
  'src/third_party/fontconfig/src': {
      'url': 'https://chromium.googlesource.com/external/fontconfig.git@86b48ec01ece451d5270d0c5181a43151e45a042',
      'condition': 'checkout_linux',
  },
  'src/third_party/freetype/src':
    'https://chromium.googlesource.com/chromium/src/third_party/freetype2.git@27c1cb10a52420515ce66729dfca897be21691b8',
  'src/third_party/harfbuzz-ng/src':
    'https://chromium.googlesource.com/external/github.com/harfbuzz/harfbuzz.git@9f83bbbe64654b45ba5bb06927ff36c2e7588495',
  'src/third_party/google_benchmark/src': {
    'url': 'https://chromium.googlesource.com/external/github.com/google/benchmark.git@761305ec3b33abf30e08d50eb829e19a802581cc',
  },
  # WebRTC-only dependency (not present in Chromium).
  'src/third_party/gtest-parallel':
    'https://chromium.googlesource.com/external/github.com/google/gtest-parallel@cd488bdedc1d2cffb98201a17afc1b298b0b90f1',
  'src/third_party/google-truth/src': {
      'url': 'https://chromium.googlesource.com/external/github.com/google/truth.git@33387149b465f82712a817e6744847fe136949b3',
      'condition': 'checkout_android',
  },
  'src/third_party/googletest/src':
    'https://chromium.googlesource.com/external/github.com/google/googletest.git@373af2e3df71599b87a40ce0e37164523849166b',
  'src/third_party/icu': {
    'url': 'https://chromium.googlesource.com/chromium/deps/icu.git@1b2e3e8a421efae36141a7b932b41e315b089af8',
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
    'https://chromium.googlesource.com/external/github.com/open-source-parsers/jsoncpp.git@42e892d96e47b1f6e29844cc705e148ec4856448', # from svn 248
  'src/third_party/junit/src': {
    'url': 'https://chromium.googlesource.com/external/junit.git@0eb5ce72848d730da5bd6d42902fdd6a8a42055d',
    'condition': 'checkout_android',
  },
  'src/third_party/kotlin_stdlib/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlin_stdlib',
              'version': 'jHhaILmhjHo4cYTvCdIRMwWPOAGSwjhEtrMxwREj52gC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/kotlinc/current': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlinc',
              'version': 'clr2_okgxf1Zih4H2dIOKUWGJhrBo7fj3ZZ6nKV0dJIC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  'src/third_party/libFuzzer/src':
    'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git@bea408a6e01f0f7e6c82a43121fe3af4506c932e',
  'src/third_party/fuzztest/src':
    'https://chromium.googlesource.com/external/github.com/google/fuzztest.git@7bab06ff5fbbf8b8cce05a8661369dc2e11cde66',
  'src/third_party/libjpeg_turbo':
    'https://chromium.googlesource.com/chromium/deps/libjpeg_turbo.git@e14cbfaa85529d47f9f55b0f104a579c1061f9ad',
  'src/third_party/libsrtp':
    'https://chromium.googlesource.com/chromium/deps/libsrtp.git@a52756acb1c5e133089c798736dd171567df11f5',
  'src/third_party/dav1d/libdav1d':
    'https://chromium.googlesource.com/external/github.com/videolan/dav1d.git@716164239ad6e6b11c5dcdaa3fb540309d499833',
  'src/third_party/libaom/source/libaom':
    'https://aomedia.googlesource.com/aom.git@1f097193b0353e32c5f0b010642d89d146234804',
  'src/third_party/libunwindstack': {
      'url': 'https://chromium.googlesource.com/chromium/src/third_party/libunwindstack.git@68973d9d4690b905cab27a553a0892868b697e4a',
      'condition': 'checkout_android',
  },
  'src/third_party/perfetto':
    Var('chromium_git') + '/external/github.com/google/perfetto.git' + '@' + '27062b54dde5b49c79f4b85e0a21bde2345219a8',
  'src/third_party/protobuf-javascript/src':
    Var('chromium_git') + '/external/github.com/protocolbuffers/protobuf-javascript' + '@' + '28bf5df73ef2f345a936d9cc95d64ba8ed426a53',
  'src/third_party/libvpx/source/libvpx':
    'https://chromium.googlesource.com/webm/libvpx.git@3789524817091963ce09f1f783a919bb0615bdfc',
  'src/third_party/libyuv':
    'https://chromium.googlesource.com/libyuv/libyuv.git@b7d97d5f3f8f897b88872b6935e4c996b955bc1f',
  'src/third_party/lss': {
    'url': 'https://chromium.googlesource.com/linux-syscall-support.git@ed31caa60f20a4f6569883b2d752ef7522de51e0',
    'condition': 'checkout_android or checkout_linux',
  },
  'src/third_party/mockito/src': {
    'url': 'https://chromium.googlesource.com/external/mockito/mockito.git@04a2a289a4222f80ad20717c25144981210d2eac',
    'condition': 'checkout_android',
  },
  'src/third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' + '@' + '69015643b3f68dbd438c010439c59adc52cac808',
    'condition': 'checkout_instrumented_libraries',
  },

  # Used by boringssl.
  'src/third_party/nasm': {
      'url': 'https://chromium.googlesource.com/chromium/deps/nasm.git@e2c93c34982b286b27ce8b56dd7159e0b90869a2'
  },

  'src/third_party/openh264/src':
    'https://chromium.googlesource.com/external/github.com/cisco/openh264@652bdb7719f30b52b08e506645a7322ff1b2cc6f',

  'src/third_party/re2/src':
    'https://chromium.googlesource.com/external/github.com/google/re2.git@85c7c0fb1163b0bd83a7951f5a205ee7b489e33e',

  'src/third_party/r8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'qD5apdNjxRv3dBblf27nEZYf7r_pbLEgfN7hWyXvP_UC',
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
              'version': 'qD5apdNjxRv3dBblf27nEZYf7r_pbLEgfN7hWyXvP_UC',
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
    'https://chromium.googlesource.com/chromium/src/tools@f5323c085792b5ab131bf07f21167d4dbbc11c9c',

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
        'version': 'KXOia11cm9lVdUdPlbGLu8sCz6Y4ey_HV2s8_8qeqhgC',
      },
    ],
    'condition': 'checkout_android',
    'dep_type': 'cipd',
  },

  'src/third_party/androidx/cipd': {
    'packages': [
      {
          'package': 'chromium/third_party/androidx',
          'version': 'IG08WxVoI4_gwNrbpXqXXmytPWFopEI1x0EaFC1KosAC',
      },
    ],
    'condition': 'checkout_android and non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/manifest_merger/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/manifest_merger',
               'version': 'K-0K2jznbc6LGEnKhT8NliSShwC43TBuVFZ6gRjOuxgC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/36.0.0',
              'version': 'y3EsZLg4bxPmpW0oYsAHylywNyMnIwPS3kh1VbQLAFAC',
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
              'package': 'chromium/third_party/android_sdk/public/platforms/android-36',
              'version': '_YHemUrK49JrE7Mctdf5DDNOHu1VKBx_PTcWnZ-cbOAC',
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': 'gekOVsZjseS1w9BXAT3FsoW__ByGDJYS9DgqesiwKYoC',
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
              'version': 'G3VkWqTv1YWDvC6zCrL34iQREzrzdBmSL4GMboAIiAEC',
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

  'src/third_party/turbine/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': 'TkTaFqF4Bs-JmFtxFV_Nfh4u5jh_401ZQsLhC7vIEqQC',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },

  'src/third_party/zstd/src': {
    'url': Var('chromium_git') + '/external/github.com/facebook/zstd.git' + '@' + 'e128976193546dceb24249206a02ff8f444f7120',
    'condition': 'checkout_android',
  },

  'src/tools/luci-go': {
      'packages': [
        {
          'package': 'infra/tools/luci/cas/${{platform}}',
          'version': 'git_revision:69966f1a4984c4ade24b8e2e1af0bbc51b64d3c0',
        },
        {
          'package': 'infra/tools/luci/isolate/${{platform}}',
          'version': 'git_revision:69966f1a4984c4ade24b8e2e1af0bbc51b64d3c0',
        },
        {
          'package': 'infra/tools/luci/swarming/${{platform}}',
          'version': 'git_revision:69966f1a4984c4ade24b8e2e1af0bbc51b64d3c0',
        }
      ],
      'dep_type': 'cipd',
  },

  'src/third_party/pipewire/linux-amd64': {
    'packages': [
      {
        'package': 'chromium/third_party/pipewire/linux-amd64',
        'version': 'BaVKmAmwpjdS6O0pnjSaMNSKhO1nmk5mRnyPVAJ2-HEC',
      },
      {
        'package': 'chromium/third_party/pipewire-media-session/linux-amd64',
        'version': 'Y6wUeITvAA0QD1vt8_a7eQdzbp0gkI1B02qfZUMJdowC',
      },
    ],

    'condition': 'checkout_linux',
    'dep_type': 'cipd',
  },

  'src/third_party/android_deps/autorolled/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/autorolled',
              'version': 'ywLef-7rK_zL0qLcTNa_wmcOKCYOyx817He_Jry8jLQC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  # Everything coming after this is automatically updated by the auto-roller.
  # === ANDROID_DEPS Generated Code Start ===
  # Generated by //third_party/android_deps/fetch_all.py
  'src/third_party/android_deps/cipd/libs/com_android_support_support_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_support_support_annotations',
              'version': 'version:2@28.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_android_tools_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_common',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_android_tools_layoutlib_layoutlib_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_layoutlib_layoutlib_api',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_android_tools_sdk_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_android_tools_sdk_common',
              'version': 'version:2@30.2.0-beta01.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_apps_common_testing_accessibility_framework_accessibility_test_framework',
              'version': 'version:2@4.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_ar_impress': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_ar_impress',
              'version': 'version:2@0.0.3.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_googlecode_java_diff_utils_diffutils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_googlecode_java_diff_utils_diffutils',
              'version': 'version:2@1.3.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_squareup_javapoet': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javapoet',
              'version': 'version:2@1.13.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy',
              'version': 'version:2@1.14.12.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/net_bytebuddy_byte_buddy_agent': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/net_bytebuddy_byte_buddy_agent',
              'version': 'version:2@1.14.12.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_ccil_cowan_tagsoup_tagsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ccil_cowan_tagsoup_tagsoup',
              'version': 'version:2@1.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_checkerframework_checker_compat_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_compat_qual',
              'version': 'version:2@2.5.5.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_android_extensions_runtime',
              'version': 'version:2@1.9.22.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlin_kotlin_parcelize_runtime',
              'version': 'version:2@1.9.22.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlinx_atomicfu_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_atomicfu_jvm',
              'version': 'version:2@0.23.2.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlinx_kotlinx_coroutines_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_guava',
              'version': 'version:2@1.8.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jsoup_jsoup': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jsoup_jsoup',
              'version': 'version:2@1.15.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_android',
              'version': 'version:2@5.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_core',
              'version': 'version:2@5.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_mockito_mockito_subclass': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_mockito_mockito_subclass',
              'version': 'version:2@5.11.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_objenesis_objenesis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_objenesis_objenesis',
              'version': 'version:2@3.3.cr1',
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
                '--no_auth',
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
                '--no_auth',
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
                '--no_auth',
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
                '--no_auth',
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
                '--no_auth',
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
                '--no_auth',
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
               '--no_auth',
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
  "+absl/cleanup/cleanup.h",
  "+absl/container",
  "-absl/container/fixed_array.h",
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
  "webrtc_lib_link_test\.cc": [
    "+media/engine",
    "+modules/audio_device",
    "+modules/audio_processing",
  ]
}
