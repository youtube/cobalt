# This file is used to manage the dependencies of the Chromium src repo. It is
# used by gclient to determine what version of each dependency to check out, and
# where.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.
#
# -----------------------------------------------------------------------------
# Rolling deps
# -----------------------------------------------------------------------------
# All repositories in this file are git-based, using Chromium git mirrors where
# necessary (e.g., a git mirror is used when the source project is SVN-based).
# To update the revision that Chromium pulls for a given dependency:
#
#  # Create and switch to a new branch
#  git new-branch depsroll
#  # Run roll-dep (provided by depot_tools) giving the dep's path and optionally
#  # a regex that will match the line in this file that contains the current
#  # revision. The script ALWAYS rolls the dependency to the latest revision
#  # in origin/master. The path for the dep should start with src/.
#  roll-dep src/third_party/foo_package/src foo_package.git
#  # You should now have a modified DEPS file; commit and upload as normal
#  git commit -aspv_he
#  git cl upload
#
# For more on the syntax and semantics of this file, see:
#   https://bit.ly/chromium-gclient-conditionals
#
# which is a bit incomplete but the best documentation we have at the
# moment.

# We expect all git dependencies specified in this file to be in sync with git
# submodules (gitlinks).
git_dependencies = 'SYNC'

gclient_gn_args_file = 'src/build/config/gclient_args.gni'
gclient_gn_args = [
  'build_with_chromium',
  'checkout_android',
  'checkout_android_prebuilts_build_tools',
  'checkout_clang_coverage_tools',
  'checkout_copybara',
  'checkout_ios_webkit',
  'checkout_nacl',
  'checkout_openxr',
  'checkout_src_internal',
  'cros_boards',
  'cros_boards_with_qemu_images',
  'generate_location_tags',
]


vars = {
  # Variable that can be used to support multiple build scenarios, like having
  # Chromium specific targets in a client project's GN file or sync dependencies
  # conditionally etc.
  'build_with_chromium': True,

  # By default, we should check out everything needed to run on the main
  # chromium waterfalls. This var can be also be set to "small", in order
  # to skip things are not strictly needed to build chromium for development
  # purposes, by adding the following line to src.git's .gclient entry:
  #      "custom_vars": { "checkout_configuration": "small" },
  'checkout_configuration': 'default',

  # By default, don't check out android. Will be overridden by gclient
  # variables.
  # TODO(crbug.com/875037): Remove this once the problem in gclient is fixed.
  'checkout_android': False,

  # By default, don't check out Fuchsia. Will be overridden by gclient
  # variables.
  # TODO(crbug.com/875037): Remove this once the problem in gclient is fixed.
  'checkout_fuchsia': False,

  # For code related to internal Fuchsia images.
  'checkout_fuchsia_internal': False,

  # Fetches the internal Fuchsia SDK boot images, with the images in a
  # comma-separated list.
  'checkout_fuchsia_internal_images': '',

  # Used for downloading the Fuchsia SDK without running hooks.
  'checkout_fuchsia_no_hooks': False,

  # Pull in Android prebuilts build tools so we can create Java xrefs
  'checkout_android_prebuilts_build_tools': False,

  # By default, do not check out Cast3P.
  'checkout_cast3p': False,

  # By default, do not check out Chromium autofill captured sites test
  # dependencies. These dependencies include very large numbers of very
  # large web capture files. Captured sites test dependencies are also
  # restricted to Googlers only.
  'checkout_chromium_autofill_test_dependencies': False,

  # By default, do not check out Chromium password manager captured sites test
  # dependencies. These dependencies include very large numbers of very
  # large web capture files. Captured sites test dependencies are also
  # restricted to Googlers only.
  'checkout_chromium_password_manager_test_dependencies': False,

  # Checkout fuzz archive. Should not need in builders.
  'checkout_clusterfuzz_data': False,

  # By default, checkout JavaScript coverage node modules. These packages
  # are used to post-process raw v8 coverage reports into IstanbulJS compliant
  # output.
  'checkout_js_coverage_modules': True,

  # Check out and download nacl for ChromeOS only.
  # This can be disabled e.g. with custom_vars.
  'checkout_nacl': 'checkout_chromeos',

  # By default, do not check out src-internal. This can be overridden e.g. with
  # custom_vars.
  'checkout_src_internal': False,

  # Checkout legacy src_internal. This variable is ignored if
  # checkout_src_internal is set as false.
  'checkout_legacy_src_internal': True,

  # For super-internal deps. Set by the official builders.
  'checkout_google_internal': False,

  # Checkout SODA (Speech On-Device API go/chrome-live-caption)
  'checkout_soda': False,

  # Fetch the additional packages and files needed to run all of the
  # telemetry tests. This is false by default as some stuff is only
  # privately accessible.
  'checkout_telemetry_dependencies': False,

  # Bots that don't consume WPR archives can skip downloading
  # them.
  'skip_wpr_archives_download': False,

  # Fetch the prebuilt binaries for llvm-cov and llvm-profdata. Needed to
  # process the raw profiles produced by instrumented targets (built with
  # the gn arg 'use_clang_coverage').
  'checkout_clang_coverage_tools': False,

  # Fetch the pgo profiles to optimize official builds.
  'checkout_pgo_profiles': False,

  # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # By default checkout the OpenXR loader library only on Windows and Android.
  # The OpenXR backend for VR in Chromium is currently only supported for these
  # platforms, but support for other platforms may be added in the future.
  'checkout_openxr' : 'checkout_win or checkout_android',

  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration != "small"',

  # By default bot checkouts the WPR archive files only when this
  # flag is set True.
  'checkout_wpr_archives': False,

  # By default, do not check out WebKit for iOS, as it is not needed unless
  # running against ToT WebKit rather than system WebKit. This can be overridden
  # e.g. with custom_vars.
  'checkout_ios_webkit': False,

  # Fetches only the SDK boot images that match at least one of the
  # entries in a comma-separated list.
  #
  # Available images:
  #   Emulation:
  #   - core.x64-dfv2
  #   - terminal.x64
  #   - terminal.qemu-arm64
  #   - workstation.qemu-x64
  #   Hardware:
  #   - workstation_eng.chromebook-x64
  #   - workstation_eng.chromebook-x64-dfv2
  #
  # Since the images are hundreds of MB, default to only downloading the image
  # most commonly useful for developers. Bots and developers that need to use
  # other images can override this with additional images.
  'checkout_fuchsia_boot_images': "terminal.x64",
  'checkout_fuchsia_product_bundles': '"{checkout_fuchsia_boot_images}" != ""',

  # By default, do not check out files required to run fuchsia tests in
  # qemu on linux-arm64 machines.
  'checkout_fuchsia_for_arm64_host': False,

  # By default, download the fuchsia sdk from the public sdk directory.
  'fuchsia_sdk_cipd_prefix': 'fuchsia/sdk/core/',

  # By default, download the fuchsia images from the fuchsia GCS bucket.
  'fuchsia_images_bucket': 'fuchsia',

  # Default to the empty board. Desktop Chrome OS builds don't need cros SDK
  # dependencies. Other Chrome OS builds should always define this explicitly.
  'cros_boards': Str(''),
  'cros_boards_with_qemu_images': Str(''),
  # Building for CrOS is only supported on linux currently.
  'checkout_simplechrome': '"{cros_boards}" != ""',
  'checkout_simplechrome_with_vms': '"{cros_boards_with_qemu_images}" != ""',

  # Generate location tag metadata to include in tests result data uploaded
  # to ResultDB. This isn't needed on some configs and the tool that generates
  # the data may not run on them, so we make it possible for this to be
  # turned off. Note that you also generate the metadata but not include it
  # via a GN build arg (tests_have_location_tags).
  'generate_location_tags': True,

  # By default, do not check out Copybara 3pp dependency that is specifically
  # needed by Cronet gn2bp CI builder.
  'checkout_copybara': False,

  # luci-go CIPD package version.
  # Make sure the revision is uploaded by infra-packagers builder.
  # https://ci.chromium.org/p/infra-internal/g/infra-packagers/console
  'luci_go': 'git_revision:a52658ecc54d0fa4ad0065918588c890982634b2',

  # This can be overridden, e.g. with custom_vars, to build clang from HEAD
  # instead of downloading the prebuilt pinned revision.
  'llvm_force_head_revision': False,

  # Make Dawn skip its standalone dependencies
  'dawn_standalone': False,

  # Fetch configuration files required for the 'use_remoteexec' gn arg
  'download_remoteexec_cfg': False,
  # RBE instance to use for running remote builds
  'rbe_instance': Str('projects/rbe-chrome-untrusted/instances/default_instance'),
  # RBE project to download rewrapper config files for. Only needed if
  # different from the project used in 'rbe_instance'
  'rewrapper_cfg_project': Str(''),
  # reclient CIPD package
  'reclient_package': 'infra/rbe/client/',
  # reclient CIPD package version
  'reclient_version': 're_client_version:0.171.0.1869ad5c-gomaip',

  # screen-ai CIPD packages
  'screen_ai_linux': 'version:126.18',
  'screen_ai_macos_amd64': 'version:126.18',
  'screen_ai_macos_arm64': 'version:126.18',
  'screen_ai_windows_amd64': 'version:126.18',
  'screen_ai_windows_386': 'version:126.18',

  # siso CIPD package version.
  'siso_version': 'git_revision:aa31236f9e208039b6196cbae4318d56d4c30e3d',

  # download libaom test data
  'download_libaom_testdata': False,

  # download libvpx test data
  'download_libvpx_testdata': False,

  'android_git': 'https://android.googlesource.com',
  'aomedia_git': 'https://aomedia.googlesource.com',
  'boringssl_git': 'https://boringssl.googlesource.com',
  'chrome_git': 'https://chrome-internal.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
  'dawn_git': 'https://dawn.googlesource.com',
  'pdfium_git': 'https://pdfium.googlesource.com',
  'quiche_git': 'https://quiche.googlesource.com',
  'skia_git': 'https://skia.googlesource.com',
  'swiftshader_git': 'https://swiftshader.googlesource.com',
  'webrtc_git': 'https://webrtc.googlesource.com',
  'betocore_git': 'https://beto-core.googlesource.com',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling V8
  # and whatever else without interference from each other.
  'src_internal_revision': '6ff450af765519bb2a24b48b496cfbae1d87b0c8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Skia
  # and whatever else without interference from each other.
  'skia_revision': '9ab7c2064b2b1ab22f856a7f0a8c3b3ae4cb89c7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling V8
  # and whatever else without interference from each other.
  'v8_revision': 'a4e1b9a47f4f04f5e33590049f41c55a4f6edf10',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ANGLE
  # and whatever else without interference from each other.
  'angle_revision': 'f0a78b0de48ee4d1d0f5a30d0f33defa0365f130',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling SwiftShader
  # and whatever else without interference from each other.
  'swiftshader_revision': 'd5c4284774115bb1e32c012a2be1b5fbeb1ab1f9',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling PDFium
  # and whatever else without interference from each other.
  'pdfium_revision': '84a8011ec69d0e2de271c05be7d62979608040d9',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling BoringSSL
  # and whatever else without interference from each other.
  'boringssl_revision': '571c76e919c0c48219ced35bef83e1fc83b00eed',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Fuchsia sdk
  # and whatever else without interference from each other.
  'fuchsia_version': 'version:25.20241109.3.1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling google-toolbox-for-mac
  # and whatever else without interference from each other.
  'google_toolbox_for_mac_revision': '42b12f10cd8342f5cb41a1e3e3a2f13fd9943b0d',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling googletest
  # and whatever else without interference from each other.
  'googletest_revision': 'd144031940543e15423a25ae5a8a74141044862f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lighttpd
  # and whatever else without interference from each other.
  'lighttpd_revision': '9dfa55d15937a688a92cbf2b7a8621b0927d06eb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling lss
  # and whatever else without interference from each other.
  'lss_revision': 'ce877209e11aa69dcfffbd53ef90ea1d07136521',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling NaCl
  # and whatever else without interference from each other.
  'nacl_revision': '6944e6b79dbd1b9776681c025bd4f4c281bb4791',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_revision': '0ae7e607370cc66218ccfacf5de4db8a35424c2f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_testing_revision': '7a69b1a2b028476f840ab7d4a2ffdfe4eb2c389f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling HarfBuzz
  # and whatever else without interference from each other.
  'harfbuzz_revision': '1da053e87f0487382404656edca98b85fe51f2fd',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Emoji Segmenter
  # and whatever else without interference from each other.
  'emoji_segmenter_revision': '955936be8b391e00835257059607d7c5b72ce744',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling OTS
  # and whatever else without interference from each other.
  'ots_revision': '46bea9879127d0ff1c6601b078e2ce98e83fcd33',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': 'b91cf840ac3255ef03b23cc93621369627422a1a',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling chromium_variations
  # and whatever else without interference from each other.
  'chromium_variations_revision': 'c170abb48f7715c237f4c06eaed0fe6f8a4c6f8d',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling CrossBench
  # and whatever else without interference from each other.
  'crossbench_revision': 'ae6f165652e0ea983d73f5d04b7470d08c869e4f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libFuzzer
  # and whatever else without interference from each other.
  'libfuzzer_revision': 'a7128317fe7935a43d6c9f39df54f21113951941',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling fuzztest
  # and whatever else without interference from each other.
  'fuzztest_revision': '0021f30508bc7f73fa5270962d022acb480d242f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling domato
  # and whatever else without interference from each other.
  'domato_revision': '053714bccbda79cf76dac3fee48ab2b27f21925e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling devtools-frontend
  # and whatever else without interference from each other.
  'devtools_frontend_revision': 'f2f3682c9db8ca427f8c64f0402cc2c5152c6c24',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libprotobuf-mutator
  # and whatever else without interference from each other.
  'libprotobuf-mutator': 'a304ec48dcf15d942607032151f7e9ee504b5dcf',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_build-tools_version
  # and whatever else without interference from each other.
  'android_sdk_build-tools_version': 'DxwAZ3hD551Neu6ycuW5CPnXFrdleRBd93oX1eB_m9YC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_emulator_version
  # and whatever else without interference from each other.
  'android_sdk_emulator_version': '9lGp8nTUCRRWGMnI_96HcKfzjnxEJKUcfvfwmA3wXNkC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platform-tools_version
  # and whatever else without interference from each other.
  'android_sdk_platform-tools_version': 'WihaseZR6cojZbkzIqwGhpTp92ztaGfqq8njBU8eTXYC',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_sdk_platforms_version
  # and whatever else without interference from each other.
  'android_sdk_platforms_version': 'kIXA-9XuCfOESodXEdOBkW5f1ytrGWdbp3HFp1I8A_0C',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'dawn_revision': 'c3530f2883610bb6606a5f55935c189e732e67d0',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'quiche_revision': '9616efc903b7469161996006c8cf963238e26503',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ink
  # and whatever else without interference from each other.
  'ink_revision': '4300dc7402a257b85fc5bf2559137edacb050227',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ink_stroke_modeler
  # and whatever else without interference from each other.
  'ink_stroke_modeler_revision': '0999e4cf816b42c770d07916698bce943b873048',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ios_webkit
  # and whatever else without interference from each other.
  'ios_webkit_revision': 'f8c0fe750d94b7db23d193c0b1f31858c2537620',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libexpat
  # and whatever else without interference from each other.
  'libexpat_revision': '624da0f593bb8d7e146b9f42b06d8e6c80d032a3',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling wuffs
  # and whatever else without interference from each other.
  'wuffs_revision': 'e3f919ccfe3ef542cfc983a82146070258fb57f8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libavif
  # and whatever else without interference from each other.
  'libavif_revision': '1cdeff7ecf456492c47cf48fc0cef6591cdc95da',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling crabbyavif
  # and whatever else without interference from each other.
  'crabbyavif_revision': 'c3548280e0a516ed7cad7ff1591b5807cef64aa4',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Speedometer v3.0
  # and whatever else without interference from each other.
  'speedometer_3.0_revision': '8d67f28d0281ac4330f283495b7f48286654ad7d',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling nearby
  # and whatever else without interference from each other.
  'nearby_revision': '8e87a6e51c93e7836ecdbcc0a520c7992f3ece13',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling securemessage
  # and whatever else without interference from each other.
  'securemessage_revision': 'fa07beb12babc3b25e0c5b1f38c16aa8cb6b8f84',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ukey2
  # and whatever else without interference from each other.
  'ukey2_revision': '0275885d8e6038c39b8a8ca55e75d1d4d1727f47',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'cros_components_revision': '9129cf4b2a5ca775c280243257a0b4856a93c7fb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'resultdb_version': 'git_revision:ebc74d10fa0d64057daa6f128e89f3672eeeec95',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'libcxxabi_revision':    'cec7f478354a8c8599f264ed8bb6043b5468f72d',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'libunwind_revision':    '5b01ea4a6f3b666b7d190e7cb7c31db2ed4d94ce',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'clang_format_revision':    '3c0acd2d4e73dd911309d9e970ba09d58bf23a62',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling feed
  # and whatever else without interference from each other.
  'highway_revision': '00fe003dac355b979f36157f9407c7c46448958e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling ffmpeg
  # and whatever else without interference from each other.
  'ffmpeg_revision': '591ae4b02eaff9a03e2ec863da895128b0b49910',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling beto-core
  # and whatever else without interference from each other.
  'betocore_revision': '89563fec14c756482afa08b016eeba9087c8d1e3',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling llvm-libc
  # and whatever else without interference from each other.
  'llvm_libc_revision':    'ca74a72e2b32ad804522bbef04dfe32560a10206',

  # If you change this, also update the libc++ revision in
  # //buildtools/deps_revisions.gni.
  'libcxx_revision':       '8e31ad42561900383e10dbefc1d3e8f38cedfbe9',

  # GN CIPD package version.
  'gn_version': 'git_revision:feafd1012a32c05ec6095f69ddc3850afb621f3a',

  # ninja CIPD package.
  'ninja_package': 'infra/3pp/tools/ninja/',

  # ninja CIPD package version.
  # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
  'ninja_version': 'version:3@1.12.1.chromium.4',

  # 'magic' variable to tell depot_tools that git submodules should be accepted
  # but parity with DEPS file is expected.
  'SUBMODULE_MIGRATION': 'True',

  # condition to allowlist deps to be synced in Cider. Allowlisting is needed
  # because not all deps are compatible with Cider. Once we migrate everything
  # to be compatible we can get rid of this allowlisting mecahnism and remove
  # this condition. Tracking bug for removing this condition: b/349365433
  'non_git_source': 'True',
}

# Only these hosts are allowed for dependencies in this DEPS file.
# If you need to add a new host, contact chrome infrastracture team.
allowed_hosts = [
  'android.googlesource.com',
  'aomedia.googlesource.com',
  'beto-core.googlesource.com',
  'boringssl.googlesource.com',
  'chrome-infra-packages.appspot.com',
  'chrome-internal.googlesource.com',
  'chromium.googlesource.com',
  'dawn.googlesource.com',
  'pdfium.googlesource.com',
  'quiche.googlesource.com',
  'skia.googlesource.com',
  'swiftshader.googlesource.com',
  'webrtc.googlesource.com',

   # TODO(337061377): Move into a separate allowed gcs bucket list.
  'chromium-ads-detection',
  'chromium-browser-clang',
  'chromium-clang-format',
  'chromium-doclava',
  'chromium-nodejs',
  'chrome-linux-sysroot',
  'chromium-fonts',
  'chromium-style-perftest',
  'chromium-telemetry',
  'chromium-webrtc-resources',
  'perfetto',
]

deps = {
  # NPM dependencies for JavaScript code coverage.
  'src/third_party/js_code_coverage/node_modules': {
    'dep_type': 'gcs',
    'bucket': 'chromium-nodejs',
    'objects': [
      {
        'object_name': 'js_code_coverage/90d7a8ecae092222e585481b64e9928bcf4a689e723a0af4b94922280cd55a48',
        'sha256sum': '90d7a8ecae092222e585481b64e9928bcf4a689e723a0af4b94922280cd55a48',
        'size_bytes': 1472459,
        'generation': 1716929997740855
      }
    ]

  },
  'src/build/linux/debian_bullseye_amd64-sysroot': {
    'bucket': 'chrome-linux-sysroot',
    'condition': 'checkout_linux and checkout_x64 and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'generation': 1714590045814759,
        'object_name': 'dec7a3a0fc5b83b909cba1b6d119077e0429a138eadef6bf5a0f2e03b1904631',
        'sha256sum': 'dec7a3a0fc5b83b909cba1b6d119077e0429a138eadef6bf5a0f2e03b1904631',
        'size_bytes': 129948576,
      },
    ],
  },
  'src/build/linux/debian_bullseye_arm64-sysroot': {
    'bucket': 'chrome-linux-sysroot',
    'condition': 'checkout_linux and checkout_arm64 and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'generation': 1714589974958986,
        'object_name': '308e23faba3174bd01accfe358467b8a40fad4db4c49ef629da30219f65a275f',
        'sha256sum': '308e23faba3174bd01accfe358467b8a40fad4db4c49ef629da30219f65a275f',
        'size_bytes': 108470444,
      },
    ],
  },
  'src/build/linux/debian_bullseye_armhf-sysroot': {
    'bucket': 'chrome-linux-sysroot',
    'condition': 'checkout_linux and checkout_arm and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'generation': 1714589870087834,
        'object_name': 'fe81e7114b97440262bce004caf02c1514732e2fa7f99693b2836932ad1c4626',
        'sha256sum': 'fe81e7114b97440262bce004caf02c1514732e2fa7f99693b2836932ad1c4626',
        'size_bytes': 99265992,
      },
    ],
  },
  'src/build/linux/debian_bullseye_i386-sysroot': {
    'bucket': 'chrome-linux-sysroot',
    'condition': 'checkout_linux and (checkout_x86 or checkout_x64) and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'generation': 1714589989387491,
        'object_name': 'b53933120bb08ffc38140a817e3f0f99782254a6bf9622271574fa004e8783a4',
        'sha256sum': 'b53933120bb08ffc38140a817e3f0f99782254a6bf9622271574fa004e8783a4',
        'size_bytes': 122047968,
      },
    ],
  },
  'src/build/linux/debian_bullseye_mips64el-sysroot': {
    'bucket': 'chrome-linux-sysroot',
    'condition': 'checkout_linux and checkout_mips64 and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'generation': 1714590006168779,
        'object_name': '783cb79f26736c69e8125788d95ffb65a28172349009d75188838a004280a92b',
        'sha256sum': '783cb79f26736c69e8125788d95ffb65a28172349009d75188838a004280a92b',
        'size_bytes': 103362108,
      },
    ],
  },
  'src/build/linux/debian_bullseye_mipsel-sysroot': {
    'bucket': 'chrome-linux-sysroot',
    'condition': 'checkout_linux and checkout_mips and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'generation': 1714589936675352,
        'object_name': 'fcf8c3931476dd097c58f2f5d44621c7090b135e85ab56885aa4b44f4bd6cdb5',
        'sha256sum': 'fcf8c3931476dd097c58f2f5d44621c7090b135e85ab56885aa4b44f4bd6cdb5',
        'size_bytes': 96161964,
      },
    ],
  },
  'src/buildtools/win-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "win" and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '49458d4c1e884a38308f8dc6a2c7eb55fc478755',
        'sha256sum': '2f964ea355762d28005568a1cf888114d13b18631c618543586fb40589a22224',
        'size_bytes': 3214848,
        'generation': 1699478813805380,
        'output_file': 'clang-format.exe',
      },
    ],
  },
  'src/buildtools/mac-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "mac" and host_cpu == "x64" and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '0b4bd257a1f4cd27d27d6919b0f9e52ecdfa8f1e',
        'sha256sum': '0f3c38a6af0a04fd4161f1948f02e83a8827727e77242d3b5b61ae4f009a270a',
        'size_bytes': 2869976,
        'generation': 1699478821342910,
        'output_file': 'clang-format',
      },
    ],
  },
  'src/buildtools/mac_arm64-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "mac" and host_cpu == "arm64" and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '96c34e77259c4cc1fc7bdf067fc058bfd341ab85',
        'sha256sum': '66c5243cd530702defcbe18dffdbed0da9a3d1474b158a949580f6d269fbac17',
        'size_bytes': 2847744,
        'generation': 1699478828600976,
        'output_file': 'clang-format',
      },
    ],
  },
  'src/buildtools/linux64-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "linux" and non_git_source',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': 'b42097ca924d1f1736a5a7806068fed9d7345eb4',
        'sha256sum': '82df59a7d4390892c3eeaf0c8bf626e2869f1138a6ad3eb90dd51da0011ba630',
        'size_bytes': 3539912,
        'generation': 1699478806427152,
        'output_file': 'clang-format',
      },
    ],
  },
  'src/third_party/data_sharing_sdk': {
      'packages': [
          {
              'package': 'chrome_internal/third_party/google3/data_sharing_sdk',
              'version': 'HAQZ7vmCc21oA10rsyKWcwSpXIxctI1_Ids7jIaMvZoC',
          },
      ],
      'condition': 'checkout_src_internal and non_git_source',
      'dep_type': 'cipd',
  },
  # Pull down Node binaries for WebUI toolchain.
  # The Linux binary is always downloaded regardless of host os and architecture
  # since remote node actions run on Linux worker.
  # See also //third_party/node/node.gni
  'src/third_party/node/linux': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-nodejs',
      'objects': [
          {
              'object_name': 'fa98c6432de572206bc5519f85e9c96bd518b039',
              'sha256sum': 'fb563633b5bfe2d4307075c54c6bb54664a3b5ec6bc811f5b15742720549007a',
              'size_bytes': 50288755,
              'generation': 1730835522207929,
              'output_file': 'node-linux-x64.tar.gz',
          },
      ],
  },
  # The Mac x64/arm64 binaries are downloaded regardless of host architecture
  # since it's possible to cross-compile for the other architecture. This can
  # cause problems for tests that use node if the test device architecture does
  # not match the architecture of the compile machine.
  'src/third_party/node/mac': {
      'dep_type': 'gcs',
      'condition': 'host_os == "mac" and non_git_source',
      'bucket': 'chromium-nodejs',
      'objects': [
          {
              'object_name': '4c8952a65a1ce7a2e4cff6db68f9b7454c46349f',
              'sha256sum': 'fadb4530fbe6e35ed298848c66102a0aa7d92974789e6222c4eadee26a381e7e',
              'size_bytes': 45672893,
              'generation': 1730835514382259,
              'output_file': 'node-darwin-x64.tar.gz',
          },
      ],
  },
  'src/third_party/node/mac_arm64': {
      'dep_type': 'gcs',
      'condition': 'host_os == "mac" and non_git_source',
      'bucket': 'chromium-nodejs',
      'objects': [
          {
              'object_name': '0886aa6a146cb5c213cb09b59ed1075982e4cb57',
              'sha256sum': 'd39e2d44d58bb89740b9aca1073959fc92edbdbbe810a5e48448e331cf72c196',
              'size_bytes': 44929037,
              'generation': 1730835518292126,
              'output_file': 'node-darwin-arm64.tar.gz',
          },
      ],
  },
  'src/third_party/node/win': {
      'dep_type': 'gcs',
      'condition': 'host_os == "win" and non_git_source',
      'bucket': 'chromium-nodejs',
      'objects': [
          {
              'object_name': '907d7e104e7389dc74cec7d32527c1db704b7f96',
              'sha256sum': '7447c4ece014aa41fb2ff866c993c708e5a8213a00913cc2ac5049ea3ffc230d',
              'size_bytes': 80511640,
              'generation': 1730835526374028,
              'output_file': 'node.exe',
          },
      ],
  },
  # Pull down NPM dependencies for WebUI toolchain.
  'src/third_party/node/node_modules': {
    'bucket': 'chromium-nodejs',
    'dep_type': 'gcs',
    'condition': 'non_git_source',
    'objects': [
      {
        'object_name': '8eb0a40003464becccc4da796843e35116d0aa2e',
        'sha256sum': 'cb5af9a19b3ec0a01717022bbaa3bd0974f21e18db5397c097af4a54c198080e',
        'size_bytes': 8877615,
        'generation': 1729559018744319,
        'output_file': 'node_modules.tar.gz',
      },
    ],
  },
  'src/third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'condition': 'not llvm_force_head_revision',
    'objects': [
      {
        # The Android libclang_rt.builtins libraries are currently only included in the Linux clang package.
        'object_name': 'Linux_x64/clang-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '10fbc53c0730a50e2117b3414a45784667d6cf7d91a3bcaab2b3a78ede08c5d2',
        'size_bytes': 54119960,
        'generation': 1730990401272305,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'b53177355aae0a5f8bf0cf230d8d783d7eb1dc32fb525175aa64ebe93a697d59',
        'size_bytes': 13409228,
        'generation': 1730990401560844,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'ea60c8ed153b175d60973203d4efd8e03627d5eee3f567dd61dcc66cb76af152',
        'size_bytes': 27940520,
        'generation': 1730990401938247,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'aab41caead88fbbec57a352e5ad540ebdd54eda869ea632cadc01882454df23b',
        'size_bytes': 2385012,
        'generation': 1730990403020118,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '16d08de19205ebd84e8fe5850132e29eb89c25ccd6ecd6cdf18ff0d6d5756047',
        'size_bytes': 5471316,
        'generation': 1730990402077586,
        'condition': '(checkout_linux or checkout_mac or checkout_android and host_os != "mac") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'cfc0c381a64b2770e1f3fe934dc08927b78cbe8204c4b145c175e1ac47a79b3f',
        'size_bytes': 48586148,
        'generation': 1730990404989096,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '994d94a647c818d28f9551d46e39637b4a9f9e72ae0f3d98f75db949decfc811',
        'size_bytes': 973888,
        'generation': 1730990424944699,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'e3e779988cfbc9443fe050a31f92f0b2bfc8b855a8fa0d50b39bda520214323f',
        'size_bytes': 12989428,
        'generation': 1730990405580532,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'fb3652a79650eb196f7229e8420d8482934db18ad899322a59d9532a967ba8f0',
        'size_bytes': 26917008,
        'generation': 1730990405502536,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '366c73a45297d36c015eb046232cecd6bbbb8fe8a460761ea4a488427c934846',
        'size_bytes': 2252492,
        'generation': 1730990406483704,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '07ea023580ba850f1cfde29018c6094f19fa2f876949afdd9b7ceacd02614315',
        'size_bytes': 42643072,
        'generation': 1730990426320272,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '2ba853495a456c137a03615bc90c1dc13378a083cd245c752e5cdb32cc5d710b',
        'size_bytes': 11551140,
        'generation': 1730990427298192,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '8bd0290834025283494137c71f21cc53e5180a0fe5526ce5fc1def6b71adf56d',
        'size_bytes': 22996480,
        'generation': 1730990427104379,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '009bf6dca60483ceef0f5b267bab206e71f70150f595dee378704ac1bab15810',
        'size_bytes': 1978164,
        'generation': 1730990428146704,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/clang-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '307a92baa82c49a97fa342e836c7d99c461757e3c4992e525ec11946e7e0c273',
        'size_bytes': 45504620,
        'generation': 1730990452347678,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'e38e29eb61f0907e43dd9e4b13f7b624e88577deadbe783d0b6f5ba0fe73470f',
        'size_bytes': 13198876,
        'generation': 1730990452638793,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '7dada03189cd68e993f32d2f44ba60308c21d5aaff841072fb023f569f62981e',
        'size_bytes': 2481484,
        'generation': 1730990473380663,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': '35d002034b9758bb22a81d68b29ef1fc8584bb8c80d8f8161daf811dd88af8c0',
        'size_bytes': 25507672,
        'generation': 1730990452771999,
       'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'cec41d8e54630d4b4cc27c231af57f334c9149c0faeaf1f74bb4f258f47aee4d',
        'size_bytes': 2394780,
        'generation': 1730990453517854,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-20-init-9764-gb81d8e90-6.tar.xz',
        'sha256sum': 'b53acffcb3bf0ef000b8af31ee622c34da647ecec28fbf9777db4d4c31057dfd',
        'size_bytes': 5495104,
        'generation': 1730990453035029,
        'condition': 'checkout_linux or checkout_mac or checkout_android and host_os == "win"',
      },
    ]
  },
  # Update prebuilt Rust toolchain.
  'src/third_party/rust-toolchain': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/rust-toolchain-bca5fdebe0e539d123f33df5f2149d5976392e76-1-llvmorg-20-init-9764-gb81d8e90.tar.xz',
        'sha256sum': '0c4092f5769c5ececd096732b00f2c5c4e1741d4112881a2c83fe7e78271d792',
        'size_bytes': 122758604,
        'generation': 1729629177370195,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Mac/rust-toolchain-bca5fdebe0e539d123f33df5f2149d5976392e76-1-llvmorg-20-init-9764-gb81d8e90.tar.xz',
        'sha256sum': '41c0265504ce0ace325f87e7db24329170fcc88d3931ff76e61126db46f2b5e1',
        'size_bytes': 115814060,
        'generation': 1729629179200874,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-bca5fdebe0e539d123f33df5f2149d5976392e76-1-llvmorg-20-init-9764-gb81d8e90.tar.xz',
        'sha256sum': '4c264b2fcc4a4bb7d908cf7838c70878cbcfa6dabec5b5b97a5fcfdab8658eb6',
        'size_bytes': 104504052,
        'generation': 1729629180998449,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-bca5fdebe0e539d123f33df5f2149d5976392e76-1-llvmorg-20-init-9764-gb81d8e90.tar.xz',
        'sha256sum': 'afade008b0eea5d8d30e19ceef22e9f9c4fe9b4cdcb9c61926665ec1658379cc',
        'size_bytes': 181085496,
        'generation': 1729629182819402,
        'condition': 'host_os == "win"',
      },
    ],
  },
  'src/third_party/clang-format/script':
    Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@' +
    Var('clang_format_revision'),
  'src/buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux" and non_git_source',
  },
  'src/buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'src/buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },
  'src/buildtools/reclient': {
    'packages': [
      {
        'package': Var('reclient_package') + '${{platform}}',
        'version': Var('reclient_version'),
      }
    ],
    'condition': 'non_git_source',
    'dep_type': 'cipd',
  },

  # We don't know target_cpu at deps time. At least until there's a universal
  # binary of httpd-php, pull both intel and arm versions in DEPS and then pick
  # the right one at runtime.
  'src/third_party/apache-mac': {
    'packages': [
      {
        'package': 'infra/3pp/tools/httpd-php/mac-amd64',
        'version': 'version:2@httpd2.4.38.php7.3.31.chromium.3',
      },
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac or checkout_ios',
  },
  'src/third_party/apache-mac-arm64': {
    'packages': [
      {
        'package': 'infra/3pp/tools/httpd-php/mac-arm64',
        'version': 'version:2@httpd2.4.38.php7.3.31.chromium.3',
      },
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac or checkout_ios',
  },

  'src/third_party/apache-linux': {
    'packages': [
      {
        'package': 'infra/3pp/tools/httpd-php/linux-amd64',
        'version': 'version:2@httpd2.4.38.php7.3.31.chromium.3',
      },
    ],
    'dep_type': 'cipd',
    'condition': '(host_os == "linux") and non_git_source',
  },

  'src/third_party/apache-windows-arm64': {
    'packages': [
        {
            'package': 'infra/3pp/tools/httpd-php/windows-arm64',
            'version': 'version:3@httpd2.4.55-php8.2.5.chromium.6',
        }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_win'
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

  'src/third_party/cronet_android_mainline_clang/linux-amd64': {
      'packages': [
          {
              'package': 'chromium/third_party/cronet_android_mainline_clang/linux-amd64',
              'version': 'YBJ6tRxent4zFvyGNyAxWQ160dRHZ6vj7in9Al3VeGAC',
          },
      ],
      'condition': 'checkout_android and host_os == "linux"',
      'dep_type': 'cipd',
  },

  'src/android_webview/tools/cts_archive/cipd': {
      'packages': [
          {
              'package': 'chromium/android_webview/tools/cts_archive',
              'version': 'UYQZhJpB3MWpJIAcesI-M1bqRoTghiKCYr_SD9tPDewC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/chrome/browser/resources/preinstalled_web_apps/internal': {
    'url': Var('chrome_git') + '/chrome/components/default_apps.git' + '@' + 'ef722130bdfed458eb5380f10585b880ae5dff4e',
    'condition': 'checkout_src_internal',
  },

  'src/chrome/installer/mac/third_party/xz/xz': {
      'url': Var('chromium_git') + '/chromium/deps/xz.git' + '@' + '10d236393a338a55830db628356f022a91978b61',
      'condition': 'checkout_mac',
  },

# Cobalt: imported
#  'src/third_party/libc++/src':
#    Var('chromium_git') +
#    '/external/github.com/llvm/llvm-project/libcxx.git' + '@' +
#    Var('libcxx_revision'),
  'src/third_party/libc++abi/src':
    Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' +
    Var('libcxxabi_revision'),
  'src/third_party/libunwind/src':
    Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libunwind.git' + '@' +
    Var('libunwind_revision'),
  'src/third_party/llvm-libc/src':
    Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libc.git' + '@' +
    Var('llvm_libc_revision'),

  'src/third_party/updater/chrome_linux64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_linux and non_git_source',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chrome_linux64',
          'version': 'ytJ0UbU9gMLUMLRQlmqQpGpOy1dYswI3rOJ0ILnIFbUC',
        },
      ],
  },

  'src/third_party/updater/chrome_mac_universal/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_mac',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chrome_mac_universal',
          'version': 'gzutuY-G7u8n5746jgmishm8uWjUR070TXdFc23Ea7YC',
        },
      ],
  },

  'src/third_party/updater/chrome_mac_universal_prod/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_mac',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chrome_mac_universal_prod',
          # 129.0.6651.0
          'version': 'IrAigaqukp1GbaksroZcR3Jo0oOYKg9kzatjzNNbXKQC',
        },
      ],
  },

  'src/third_party/updater/chrome_win_arm64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_win',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chrome_win_arm64',
          'version': 'lVDOE8L-CQNHAhRQpy3gM0lNESM1J_U7BSo9bA5pynkC',
        },
      ],
  },

  # version:2@131.0.6776.0
  'src/third_party/updater/chrome_win_x86/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_win',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chrome_win_x86',
          'version': 'ZunaL0ySQ0b5WQqOPpKgTt3D0dAq2gE58WYhUeFllwMC',
        },
      ],
  },

  'src/third_party/updater/chrome_win_x86_64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_win',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chrome_win_x86_64',
          'version': '4R5OznuthRryKbHdx7HjPG8NaJTt59TDBrk0_JUvBfsC',
        },
      ],
  },

  'src/third_party/updater/chromium_linux64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_linux and non_git_source',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chromium_linux64',
          'version': 'OLbfeePAbqPnFqcPmkFtR-GK8dN3T3NeH2AChZwBHjgC',
        },
      ],
  },

  # A somewhat recent Chromium-branded updater build. (x86_64)
  'src/third_party/updater/chromium_mac_amd64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_mac',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chromium_mac_amd64',
          'version': 'zVv93X5XSClxTR1YejkQuBdSpye7JfPS_h6GcH1N4i4C',
        },
      ],
  },

  # A somewhat recent Chromium-branded updater build. (ARM64)
  'src/third_party/updater/chromium_mac_arm64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_mac',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chromium_mac_arm64',
          'version': 'Va20qxSst3lq4WfEZlWiwzXCpSo5XbhhuqJXyqzvhF8C',
        },
      ],
  },

  # r1372006
  'src/third_party/updater/chromium_win_arm64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_win',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chromium_win_arm64',
          'version': 'ksRzLnqewvz7P-YMX2e8mxZuDI1hgPtLNCuAXIisXhoC',
        },
      ],
  },

  # r1372046
  'src/third_party/updater/chromium_win_x86/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_win',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chromium_win_x86',
          'version': 'tcFWGGE3wmwKDhT_s-9mzPiQVYxMOI1U9sRAhm1_5QcC',
        },
      ],
  },

  # r1372030
  'src/third_party/updater/chromium_win_x86_64/cipd': {
      'dep_type': 'cipd',
      'condition': 'checkout_win',
      'packages': [
        {
          'package': 'chromium/third_party/updater/chromium_win_x86_64',
          'version': 'iXamEEmbStVBV_c-vEG4Xtrda8tTqSRCZ-tmE7VRHb0C',
        },
      ],
  },

  'src/chrome/test/data/autofill/captured_sites/artifacts': {
    'url': Var('chrome_git') + '/chrome/test/captured_sites/autofill.git' + '@' + 'f03bf2c01ce906c9e55bb339748fcc9aee9e3569',
    'condition': 'checkout_chromium_autofill_test_dependencies',
  },

  'src/chrome/test/data/password/captured_sites/artifacts': {
    'url': Var('chrome_git') + '/chrome/test/captured_sites/password.git' + '@' + '878100ee5ceacb182f148d68d4c8fb4c71099a90',
    'condition': 'checkout_chromium_password_manager_test_dependencies',
  },

  'src/chrome/test/data/perf/canvas_bench':
    Var('chromium_git') + '/chromium/canvas_bench.git' + '@' + 'a7b40ea5ae0239517d78845a5fc9b12976bfc732',

  'src/chrome/test/data/perf/frame_rate/content':
    Var('chromium_git') + '/chromium/frame_rate/content.git' + '@' + 'c10272c88463efeef6bb19c9ec07c42bc8fe22b9',

  'src/chrome/test/data/safe_browsing/dmg': {
    'packages': [
      {
        'package': 'chromium/chrome/test/data/safe_browsing/dmg',
        'version': '03TLfNQgc59nHmyWtYWJfFaUrEW8QDJJzXwm-672m-QC',
      },
    ],
    'condition': 'checkout_mac',
    'dep_type': 'cipd',
  },

  'src/chrome/test/data/xr/webvr_info':
    Var('chromium_git') + '/external/github.com/toji/webvr.info.git' + '@' + 'c58ae99b9ff9e2aa4c524633519570bf33536248',

  'src/clank': {
    'url': Var('chrome_git') + '/clank/internal/apps.git' + '@' +
    '72ea81191f780d065a25dc8f297ca750e86ddd93',
    'condition': 'checkout_android and checkout_src_internal',
  },

  'src/docs/website': {
    'url': Var('chromium_git') + '/website.git' + '@' + 'be9c3dfd3781964fc0bab0d6c91d9ad117b71b02',
  },

  'src/ios/third_party/earl_grey2/src': {
      'url': Var('chromium_git') + '/external/github.com/google/EarlGrey.git' + '@' + '65067e3585d853f637cea4b8eb90ccc52c80c813',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/edo/src': {
      'url': Var('chromium_git') + '/external/github.com/google/eDistantObject.git' + '@' + '8c825efc8ba6ca9147a148445449470a66b9dc54',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/gtx/src': {
      'url': Var('chromium_git') + '/external/github.com/google/GTXiLib.git' + '@' + '0e6d6628c5b4d733dfc8f605ab576dcbb72aeeb9',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/lottie/src': {
      'url': Var('chromium_git') + '/external/github.com/airbnb/lottie-ios.git' + '@' + '4a4367659c0b8576d4a106669ff2ba129026085f',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_components_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-components/material-components-ios.git' + '@' + 'c4743a1951d448f2fcf67ea3b657fe4e0864e843',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_font_disk_loader_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-font-disk-loader-ios.git' + '@' + '93acc021e3034898716028822cb802a3a816be7e',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_internationalization_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-internationalization-ios.git' + '@' + '305aa8d276f5137c98c5c1c888efc22e02251ee7',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_roboto_font_loader_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-roboto-font-loader-ios.git' + '@' + '4be05d4676645febc453a6cde7f5adfb1b785dc1',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_sprited_animation_view_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-sprited-animation-view-ios.git' + '@' + '8af9adaa182044cf2920dfb620b863669e1aeb7c',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/material_text_accessibility_ios/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-text-accessibility-ios.git' + '@' + '8cd910c1c8bbae261ae0d7e873ed96c69a386448',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/motion_interchange_objc/src': {
      'url': Var('chromium_git') + '/external/github.com/material-motion/motion-interchange-objc.git' + '@' + '2f8b548f74c52f71d4c2160715a4ba9c887321dd',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/motion_animator_objc/src': {
      'url': Var('chromium_git') + '/external/github.com/material-motion/motion-animator-objc.git' + '@' + '296f529321dd7c59c6284c7ccd85dec978c225cc',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/motion_transitioning_objc/src': {
      'url': Var('chromium_git') + '/external/github.com/material-motion/motion-transitioning-objc.git' + '@' + '1fe4a9d81433c1d43e54b118f29642e9b233907b',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/ochamcrest/src': {
      'url': Var('chromium_git') + '/external/github.com/hamcrest/OCHamcrest.git' + '@' + '92d9c14d13bb864255e65c09383564653896916b',
      'condition': 'checkout_ios',
  },

  'src/ios/third_party/webkit/src': {
      'url': Var('chromium_git') + '/external/github.com/WebKit/webkit.git' +
             '@' + Var('ios_webkit_revision'),
      'condition': 'checkout_ios and checkout_ios_webkit'
  },

  'src/media/cdm/api':
    Var('chromium_git') + '/chromium/cdm.git' + '@' + 'eb21edc44e8e5a82095037be80c8b15c51624293',

  'src/native_client': {
      'url': Var('chromium_git') + '/native_client/src/native_client.git' + '@' + Var('nacl_revision'),
      'condition': 'checkout_nacl',
  },

# Cobalt: imported
#  'src/net/third_party/quiche/src':
#    Var('quiche_git') + '/quiche.git' + '@' +  Var('quiche_revision'),

  'src/testing/libfuzzer/fuzzers/wasm_corpus':
    Var('chromium_git') + '/v8/fuzzer_wasm_corpus.git' + '@' +  '1df5e50a45db9518a56ebb42cb020a94a090258b',

  'src/tools/copybara': {
      'packages' : [
          {
              'package': 'infra/3pp/tools/copybara',
              'version': 'Hk6b79rXoJ1r6QWfiNoTYGqqIFuMpRvu4kPAGxPPULYC',
          },
      ],
      'condition': 'host_os == "linux" and checkout_copybara',
      'dep_type': 'cipd',
  },

  'src/tools/luci-go': {
      'packages': [
        {
          'package': 'infra/tools/luci/isolate/${{platform}}',
          'version': Var('luci_go'),
        },
        {
          'package': 'infra/tools/luci/swarming/${{platform}}',
          'version': Var('luci_go'),
        },
      ],
      'condition': 'non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/accessibility_test_framework/src': {
      'url': Var('chromium_git') + '/external/github.com/google/Accessibility-Test-Framework-for-Android.git' + '@' + '4a764c690353ea136c82f1a696a70bf38d1ef5fe',
  },

  'src/third_party/android_build_tools/protoc/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/protoc',
              'version': 'ivH_8voaWaRDbk7bDHj8n5YR2IH7sFuenkqy0bVOb2cC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_toolchain/ndk': {
      'packages': [
            {
                'package': 'chromium/third_party/android_toolchain/android_toolchain',
                'version': 'Idl-vYnWGnM8K3XJhM3h6zjYVDXlnljVz3FE00V9IM8C',
            },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/androidx/cipd': {
    'packages': [
      {
          'package': 'chromium/third_party/androidx',
          'version': 'DNRcaJmAiSz4higrR9ExVm53a6g6FTonwmzgouFWloMC',
      },
    ],
    'condition': 'checkout_android and non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/androidx_javascriptengine/src': {
      'url': Var('chromium_git') + '/aosp/platform/frameworks/support/javascriptengine/javascriptengine/src.git' + '@' + 'e3b6fc680728040f97442dc29d978ce5df6e76f4',
      'condition': 'checkout_android',
  },

  'src/third_party/android_system_sdk/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_system_sdk/public',
              'version': 'XzzECzCzGLrccJS1U-HdmM5VMh9BotgQ_mWhFQ464PwC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/aapt2/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/aapt2',
              'version': 'cfGQ9GV4juNnGZIPzTmaL3ehiZM1hs6UsB5HUA8fT6oC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/apkanalyzer/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/android_build_tools/apkanalyzer',
              'version': 'O8Lyta0y6jpvFD1rbPp7trvcM2rdny3ngyhyeYAWXK4C',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/bundletool/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/bundletool',
               'version': 'Q-7KJYcc15Cn9iVy-7bk0tPH9-oUrhqHis6HmUzck80C',
          },
      ],
      'condition': 'checkout_android and non_git_source',
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
               'version': 'DBKfaigj6AuTYPdN1_6jkOlQUuJa1szDRIMvHbtZrD8C',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/error_prone_javac/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/error_prone_javac',
               'version': '7EcHxlEXEaLRWEyHIAxf0ouPjkmN1Od6jkutuo0sfBIC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/lint/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/lint',
               'version': 'vBoRPSPyoHi8GYvjtidLlKAtqp5a-2CaHgpEZb16dAoC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_build_tools/manifest_merger/cipd': {
      'packages': [
          {
               'package': 'chromium/third_party/android_build_tools/manifest_merger',
               'version': '1gDDnuAjB1-mlxlnLFAfx4h9f_hG3lEIIbwyYr86i1wC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_sdk/public': {
      'packages': [
          {
              'package': 'chromium/third_party/android_sdk/public/build-tools/35.0.0',
              'version': Var('android_sdk_build-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/emulator',
              'version': Var('android_sdk_emulator_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platform-tools',
              'version': Var('android_sdk_platform-tools_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/platforms/android-35',
              'version': Var('android_sdk_platforms_version'),
          },
          {
              'package': 'chromium/third_party/android_sdk/public/cmdline-tools',
              'version': 'B4p95sDPpm34K8Cf4JcfTM-iYSglWko9qjWgbT9dxWQC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

# Cobalt: imported
#  'src/third_party/angle':
#    Var('chromium_git') + '/angle/angle.git' + '@' +  Var('angle_revision'),
# Cobalt: Dependencies from angle's DEPS file.
  'src/third_party/angle/third_party/rapidjson/src':
    Var('chromium_git') + '/external/github.com/Tencent/rapidjson.git' + '@' + '781a4e667d84aeedbeb8184b7b62425ea66ec59f',
  'src/third_party/angle/third_party/glmark2/src':
    Var('chromium_git') + '/external/github.com/glmark2/glmark2.git' + '@' + 'ca8de51fedb70bace5351c6b002eb952c747e889',

  'src/third_party/anonymous_tokens/src':
    Var('chromium_git') + '/external/github.com/google/anonymous-tokens.git' + '@' + '6ea6ec78f9e4998d0a7a5677b2aec08f0ac858f8',

    'src/third_party/blink/renderer/core/css/perftest_data': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-style-perftest',
      'objects': [
          {
              'object_name': 'e9ce994346c62f8c9fd6d0cecb2b2b0b93b4c2d8',
              'sha256sum': '519019df16c628c6c0893df18928faeaa3150a9d8f26a787a16ce7c6b2cec2ad',
              'size_bytes': 601672,
              'generation': 1664794185950162,
              'output_file': 'ecommerce.json',
          },
          {
              'object_name': '756068da5e551516b23b0ba133e55c144f623d38',
              'sha256sum': '84ef87a8163335a95111d9709306596f96742539da0b34fbe7397f799946a168',
              'size_bytes': 2156935,
              'generation': 1664794188995509,
              'output_file': 'encyclopedia.json',
          },
          {
              'object_name': '314e4e0d5e89ea9e9e9a234c617b4413adf48aa9',
              'sha256sum': 'a721ada40011a286631baae6d76878f2023ff000151792228c83b1958ea8a197',
              'size_bytes': 608840,
              'generation': 1664794191929032,
              'output_file': 'extension.json',
          },
          {
              'object_name': '3a19b42a7c46257b716d55d6733f070c87180b1e',
              'sha256sum': 'f203ff9e8c8a6a3b714f0a26db38cc940544a907435c62c79b21f4bd3f8bee8e',
              'size_bytes': 1750837,
              'generation': 1664794194891567,
              'output_file': 'news.json',
          },
          {
              'object_name': 'fdc43ee18cbd65487249441849f58aa13484aaef',
              'sha256sum': '0e92de92f49abc9a521f7175106c80744196f8cefc0263bc0f4a6b4f724a7d10',
              'size_bytes': 1310798,
              'generation': 1664794197855470,
              'output_file': 'search.json',
          },
          {
              'object_name': '7fc9338af75b7d9d185c91ddf262a356def5623d',
              'sha256sum': '34e92acae8aade2a186abe79ed1f379c266f04f72f1eb54bd3a912e889bc5cc0',
              'size_bytes': 2280846,
              'generation': 1664794200867034,
              'output_file': 'social1.json',
          },
          {
              'object_name': 'c2d7e9ce67522dad138c7feb0a6911b828bfb130',
              'sha256sum': '95c6b148577b891310c024b2daa5d68faf644a37707ac0cb21501eefe8a399a3',
              'size_bytes': 411708,
              'generation': 1664794203829582,
              'output_file': 'social2.json',
          },
          {
              'object_name': '031d5599c8a21118754e30dbea141be66104f556',
              'sha256sum': '8e7b765d72bb8e7742f5bf955f4bf64d5469f61197dad8b632304095a52322d7',
              'size_bytes': 3203922,
              'generation': 1664794206824773,
              'output_file': 'sports.json',
          },
          {
              'object_name': '8aac3db2a8c9e44babec81e539a3d60aeab4985c',
              'sha256sum': '6aeb0036dfafaf5e905abdb0ffe515a3952ffe35a7c59afb0fc8b233b27c6ce4',
              'size_bytes': 5902660,
              'generation': 1664794209886788,
              'output_file': 'video.json',
          },
      ],
  },

  'src/third_party/content_analysis_sdk/src':
    Var('chromium_git') + '/external/github.com/chromium/content_analysis_sdk.git' + '@' + '9a408736204513e0e95dd2ab3c08de0d95963efc',

  'src/third_party/dav1d/libdav1d':
    Var('chromium_git') + '/external/github.com/videolan/dav1d.git' + '@' + '93f12c117a4e1c0cc2b129dcc52e84dbd9b84200',

  'src/third_party/dawn':
    Var('dawn_git') + '/dawn.git' + '@' +  Var('dawn_revision'),

  'src/third_party/highway/src':
    Var('chromium_git') + '/external/github.com/google/highway.git' + '@' + Var('highway_revision'),

  'src/third_party/apache-portable-runtime/src': {
      'url': Var('chromium_git') + '/external/apache-portable-runtime.git' + '@' + 'c3f11fcd86b42922834cae91103cf068246c6bb6',
      'condition': 'checkout_android',
  },

  'src/third_party/barhopper': {
      'url': Var('chrome_git') + '/chrome/deps/barhopper.git' + '@' + '865bd06ef4a839b0a15d17e38e25f8911e4cdf9f',
      'condition': 'checkout_src_internal and checkout_chromeos',
  },

  'src/third_party/google_benchmark/src':
    Var('chromium_git') + '/external/github.com/google/benchmark.git' + '@' + '761305ec3b33abf30e08d50eb829e19a802581cc',

  # Download test data for Maps telemetry_gpu_integration_test.
  'src/tools/perf/page_sets/maps_perf_test/dataset': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-telemetry',
      'objects': [
          {
              'object_name': 'e6bf26977c2fd80c18789d1f279d474096a7b0d1',
              'sha256sum': 'f5f7fe360ad2b9c3d9dda2612f17336c0541bac15b4e4992f2c167e059a190fa',
              'size_bytes': 3285237,
              'generation': 1513305740113238,
              'output_file': 'load_dataset',
          },
      ],
  },

# Cobalt: imported
#  'src/third_party/boringssl/src':
#    Var('boringssl_git') + '/boringssl.git' + '@' +  Var('boringssl_revision'),

  'src/third_party/breakpad/breakpad':
    Var('chromium_git') + '/breakpad/breakpad.git' + '@' + '47f7823bdf4b1f39e462b2a497a674860e922e38',

  'src/third_party/cast_core/public/src':
    Var('chromium_git') + '/cast_core/public' + '@' + 'fbc5e98031e1271a0a566fcd4d9092b2d3275d05',

  'src/third_party/catapult':
    Var('chromium_git') + '/catapult.git' + '@' + Var('catapult_revision'),

  'src/third_party/ced/src':
    Var('chromium_git') + '/external/github.com/google/compact_enc_det.git' + '@' + 'ba412eaaacd3186085babcd901679a48863c7dd5',

  'src/third_party/checkstyle/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/checkstyle',
              'version': 'vnbLn0H_kr5nVeziAzIlGqjH1LhxEslL7O0w-UKTHh4C',
          },
      ],
      # Needed on Linux for use on chromium_presubmit.
      'condition': '(checkout_android or checkout_linux) and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/chromium-variations':
    Var('chromium_git') + '/chromium-variations.git' + '@' + Var('chromium_variations_revision'),

  # Dependency for ChromeVox.
  'src/third_party/chromevox/third_party/sre/src': {
      'url': Var('chromium_git') + '/external/github.com/zorkow/speech-rule-engine.git' + '@' + '5a56d4d33d67dc7c692da032d2ebbdefd7de780e',
      'condition': 'checkout_chromeos',
  },

  # Tools used when building Chrome for Chrome OS. This affects both the Simple
  # Chrome workflow, as well as the chromeos-chrome ebuild.
  'src/third_party/chromite': {
      'url': Var('chromium_git') + '/chromiumos/chromite.git' + '@' + 'f843e9572d9816e1193f7ff262263a415d5e0476',
      'condition': 'checkout_chromeos',
  },

  'src/third_party/cld_3/src':
    Var('chromium_git') + '/external/github.com/google/cld_3.git' + '@' + 'b48dc46512566f5a2d41118c8c1116c4f96dc661',

  'src/third_party/colorama/src':
    Var('chromium_git') + '/external/colorama.git' + '@' + '3de9f013df4b470069d03d250224062e8cf15c49',

  'src/third_party/cpu_features/src':
    Var('chromium_git') + '/external/github.com/google/cpu_features.git' + '@' + '936b9ab5515dead115606559502e3864958f7f6e',

# Cobalt: imported
#  'src/third_party/cpuinfo/src':
#    Var('chromium_git') + '/external/github.com/pytorch/cpuinfo.git' + '@' + '8df44962d437a0477f07ba6b8843d0b6a48646a4',

  'src/third_party/crc32c/src':
    Var('chromium_git') + '/external/github.com/google/crc32c.git' + '@' + 'd3d60ac6e0f16780bcfcc825385e1d338801a558',

  # For Linux and Chromium OS.
  'src/third_party/cros_system_api': {
      'url': Var('chromium_git') + '/chromiumos/platform2/system_api.git' + '@' + '554629b9242e6ae832ef14e3384654426f7fcc06',
      'condition': 'checkout_linux or checkout_chromeos',
  },

  'src/third_party/crossbench':
    Var('chromium_git') + '/crossbench.git' + '@' + Var('crossbench_revision'),


  'src/third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git' + '@' + '41d43a2a2290450aeab946883542f8049b155c87',

  'src/third_party/devtools-frontend/src':
    Var('chromium_git') + '/devtools/devtools-frontend' + '@' + Var('devtools_frontend_revision'),

  'src/third_party/devtools-frontend-internal': {
      'url': Var('chrome_git') + '/devtools/devtools-internal.git' + '@' + 'd53b91528791937d253f35cf889720af2c60aa43',
    'condition': 'checkout_src_internal',
  },

  'src/third_party/dom_distiller_js/dist':
    Var('chromium_git') + '/chromium/dom-distiller/dist.git' + '@' + '199de96b345ada7c6e7e6ba3d2fa7a6911b8767d',

  'src/third_party/eigen3/src':
    Var('chromium_git') + '/external/gitlab.com/libeigen/eigen.git' + '@' + 'b396a6fbb2e173f52edb3360485dedf3389ef830',

  'src/third_party/emoji-metadata/src': {
    'url': Var('chromium_git') + '/external/github.com/googlefonts/emoji-metadata' + '@' + '045f146fca682a836e01cd265171312bfb300e06',
    'condition': 'checkout_chromeos',
  },

  'src/third_party/espresso': {
      'packages': [
          {
              'package': 'chromium/third_party/espresso',
              'version': '5LoBT0j383h_4dXbnap7gnNQMtMjpbMJD1JaGIYNj-IC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/farmhash/src':
    Var('chromium_git') + '/external/github.com/google/farmhash.git' + '@' + '816a4ae622e964763ca0862d9dbd19324a1eaf45',

  'src/third_party/fast_float/src':
    Var('chromium_git') + '/external/github.com/fastfloat/fast_float.git' + '@' + '3e57d8dcfb0a04b5a8a26b486b54490a2e9b310f',

  'src/third_party/ffmpeg':
    Var('chromium_git') + '/chromium/third_party/ffmpeg.git' + '@' + Var('ffmpeg_revision'),

  'src/third_party/flac':
    Var('chromium_git') + '/chromium/deps/flac.git' + '@' + '689da3a7ed50af7448c3f1961d1791c7c1d9c85c',

  'src/third_party/flatbuffers/src':
    Var('chromium_git') + '/external/github.com/google/flatbuffers.git' + '@' + '8db59321d9f02cdffa30126654059c7d02f70c32',

  # Used for embedded builds. CrOS & Linux use the system version.
  'src/third_party/fontconfig/src': {
      'url': Var('chromium_git') + '/external/fontconfig.git' + '@' + '14d466b30a8ab4a9d789977ed94f2c30e7209267',
      'condition': 'checkout_linux',
  },

  'src/third_party/fp16/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FP16.git' + '@' + '0a92994d729ff76a58f692d3028ca1b64b145d91',

  'src/third_party/gemmlowp/src':
    Var('chromium_git') + '/external/github.com/google/gemmlowp.git' + '@' + '13d57703abca3005d97b19df1f2db731607a7dc2',

  'src/third_party/grpc/src': {
      'url': Var('chromium_git') + '/external/github.com/grpc/grpc.git' + '@' + '822dab21d9995c5cf942476b35ca12a1aa9d2737',
  },

  'src/third_party/freetype/src':
    Var('chromium_git') + '/chromium/src/third_party/freetype2.git' + '@' + Var('freetype_revision'),

  'src/third_party/freetype-testing/src':
    Var('chromium_git') + '/external/github.com/freetype/freetype2-testing.git' + '@' + Var('freetype_testing_revision'),

  'src/third_party/fxdiv/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FXdiv.git' + '@' + '63058eff77e11aa15bf531df5dd34395ec3017c8',

  'src/third_party/harfbuzz-ng/src':
    Var('chromium_git') + '/external/github.com/harfbuzz/harfbuzz.git' + '@' + Var('harfbuzz_revision'),

  'src/third_party/ink/src':
    Var('chromium_git') + '/external/github.com/google/ink.git' + '@' + Var('ink_revision'),

  'src/third_party/ink_stroke_modeler/src':
    Var('chromium_git') + '/external/github.com/google/ink-stroke-modeler.git' + '@' + Var('ink_stroke_modeler_revision'),

  'src/third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' + '@' + 'bb6dbcf2df7a9beb34c3773ef4df161800e3aed9',
    'condition': 'checkout_instrumented_libraries',
  },

  'src/third_party/jszip/src': {
    'url': Var('chromium_git') + '/external/github.com/Stuk/jszip.git' + '@' + '2ceb998e29d4171b4f3f2ecab1a2195c696543c0',
    'condition': 'checkout_ios',
  },

  'src/third_party/emoji-segmenter/src':
    Var('chromium_git') + '/external/github.com/google/emoji-segmenter.git' + '@' + Var('emoji_segmenter_revision'),

  'src/third_party/ots/src':
    Var('chromium_git') + '/external/github.com/khaledhosny/ots.git' + '@' + Var('ots_revision'),

  'src/third_party/libgav1/src':
    Var('chromium_git') + '/codecs/libgav1.git' + '@' + 'a2f139e9123bdb5edf7707ac6f1b73b3aa5038dd',

  'src/third_party/google_toolbox_for_mac/src': {
      'url': Var('chromium_git') + '/external/github.com/google/google-toolbox-for-mac.git' + '@' + Var('google_toolbox_for_mac_revision'),
      'condition': 'checkout_ios or checkout_mac',
  },

  'src/third_party/google-truth/src': {
      'url': Var('chromium_git') + '/external/github.com/google/truth.git' + '@' + '33387149b465f82712a817e6744847fe136949b3',
      'condition': 'checkout_android',
  },

# Cobalt: imported
#  'src/third_party/googletest/src':
#    Var('chromium_git') + '/external/github.com/google/googletest.git' + '@' + Var('googletest_revision'),

  'src/third_party/gperf': {
      'url': Var('chromium_git') + '/chromium/deps/gperf.git' + '@' + 'd892d79f64f9449770443fb06da49b5a1e5d33c1',
      'condition': 'checkout_win',
  },

  'src/third_party/cardboard/src' : {
      'url': Var('chromium_git') + '/external/github.com/googlevr/cardboard/' + '@' + '596352df971aacede278a50f55ff1fecc4e81afc',
      'condition': 'checkout_android',
  },

  'src/third_party/arcore-android-sdk/src': {
      'url': Var('chromium_git') + '/external/github.com/google-ar/arcore-android-sdk.git' + '@' + '80036a515b38deca1ad080b7c436856b454358f5',
      'condition': 'checkout_android',
  },

  'src/third_party/arcore-android-sdk-client/cipd': {
      'packages': [
        {
          'package': 'chromium/third_party/arcore-android-sdk-client',
          'version': 'gHDxvBRNpM868XTWU9SdfMqtVYTFSvK2tLRAKq4V37wC',
        },
      ],

      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  # Exists for rolling the Fuchsia SDK. Check out of the SDK should always
  # rely on the hook running |update_sdk.py| script below.
  'src/third_party/fuchsia-sdk/sdk': {
      'packages': [
          {
              'package': Var('fuchsia_sdk_cipd_prefix') + '${{platform}}',
              'version': Var('fuchsia_version'),
          },
      ],
      'condition': 'checkout_fuchsia_no_hooks',
      'dep_type': 'cipd',
  },

  'src/third_party/google-java-format/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/google-java-format',
              'version': 'AQn4F5NfPAs_GKX-z3OW_Q7-yJ9N6tPrDnmnDScjkTEC',
          },
      ],
      # Needed on Linux for use on chromium_presubmit.
      'condition': '(checkout_android or checkout_linux) and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/hamcrest/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/hamcrest',
              'version': 'dBioOAmFJjqAr_DY7dipbXdVfAxUQwjOBNibMPtX8lQC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/hunspell_dictionaries':
    Var('chromium_git') + '/chromium/deps/hunspell_dictionaries.git' + '@' + '41cdffd71c9948f63c7ad36e1fb0ff519aa7a37e',

# Cobalt: imported
#  'src/third_party/icu':
#    Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'c9fb4b3a6fb54aa8c20a03bbcaa0a4a985ffd34b',

  'src/third_party/icu4j/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/icu4j',
              'version': '8dV7WRVX0tTaNNqkLEnCA_dMofr2MJXFK400E7gOFygC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/jacoco/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/jacoco',
              'version': 'DWx1sUw2_F3SN9e21bI3W5vGT08eR3HQpXLZy6f-AnwC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/javalang/src': {
      'url': Var('chromium_git') + '/external/github.com/c2nes/javalang.git' + '@' + '0664afb7f4d40254312693f2e833c1ed4ac551c7',
      'condition': 'checkout_android',
  },

  'src/third_party/jdk/current': {
      'packages': [
          {
              'package': 'chromium/third_party/jdk',
              'version': 'U-h0kbYoIUjL__n_sfiudzFLH7-C9vKVsLSOA2XHkBEC',
          },
      ],
      # Needed on Linux for use on chromium_presubmit (for checkstyle).
      'condition': '(checkout_android or checkout_linux) and non_git_source',
      'dep_type': 'cipd',
  },

  # Deprecated - only use for tools which are broken our real JDK.
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
    Var('chromium_git') + '/external/github.com/open-source-parsers/jsoncpp.git'
      + '@' + '42e892d96e47b1f6e29844cc705e148ec4856448', # release 1.9.4

  'src/third_party/junit/src': {
      'url': Var('chromium_git') + '/external/junit.git' + '@' + '0eb5ce72848d730da5bd6d42902fdd6a8a42055d',
      'condition': 'checkout_android',
  },

  'src/third_party/kotlin_stdlib/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlin_stdlib',
              'version': 'CFXTjDOsl73rwvFkJh_CDI0rdOfcGd0-9L7w4cc8Or8C',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/kotlinc/current': {
      'packages': [
          {
              'package': 'chromium/third_party/kotlinc',
              'version': 'oYVf7iZSwOgNvQfk3t2C29BC8rWmsU7709igdSmoTZQC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/leveldatabase/src':
    Var('chromium_git') + '/external/leveldb.git' + '@' + '23e35d792b9154f922b8b575b12596a4d8664c65',

  'src/third_party/libFuzzer/src':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git' + '@' +  Var('libfuzzer_revision'),

  'src/third_party/fuzztest/src':
    Var('chromium_git') + '/external/github.com/google/fuzztest.git' + '@' + Var('fuzztest_revision'),

  'src/third_party/domato/src':
    Var('chromium_git') + '/external/github.com/googleprojectzero/domato.git' + '@' + Var('domato_revision'),

  'src/third_party/libaddressinput/src':
    Var('chromium_git') + '/external/libaddressinput.git' + '@' + 'e8712e415627f22d0b00ebee8db99547077f39bd',

  'src/third_party/libaom/source/libaom':
    Var('aomedia_git') + '/aom.git' + '@' +  'be60f06ab420d6a65c477213f04c8b0f2e12ba2e',

  'src/third_party/libavif/src':
    Var('chromium_git') + '/external/github.com/AOMediaCodec/libavif.git' + '@' + Var('libavif_revision'),

  'src/third_party/crabbyavif/src':
    Var('chromium_git') + '/external/github.com/webmproject/CrabbyAvif.git' + '@' + Var('crabbyavif_revision'),

  'src/third_party/nearby/src':
    Var('chromium_git') + '/external/github.com/google/nearby-connections.git' + '@' + Var('nearby_revision'),

  'src/third_party/beto-core/src':
    Var('betocore_git') + '/beto-core.git' + '@' + Var('betocore_revision'),

  'src/third_party/securemessage/src':
    Var('chromium_git') + '/external/github.com/google/securemessage.git' + '@' + Var('securemessage_revision'),

  'src/third_party/speedometer/v3.0':
    Var('chromium_git') + '/external/github.com/WebKit/Speedometer.git' + '@' + Var('speedometer_3.0_revision'),

  'src/third_party/ukey2/src':
    Var('chromium_git') + '/external/github.com/google/ukey2.git' + '@' + Var('ukey2_revision'),

  'src/third_party/cros-components/src':
    Var('chromium_git') + '/external/google3/cros_components.git' + '@' + Var('cros_components_revision'),

  # Userspace interface to kernel DRM services.
  'src/third_party/libdrm/src': {
      'url': Var('chromium_git') + '/chromiumos/third_party/libdrm.git' + '@' + 'ad78bb591d02162d3b90890aa4d0a238b2a37cde',
      'condition': 'checkout_linux',
  },

  'src/third_party/expat/src':
    Var('chromium_git') + '/external/github.com/libexpat/libexpat.git' + '@' + Var('libexpat_revision'),

  # The library for IPP protocol (Chrome OS).
  'src/third_party/libipp/libipp': {
      'url': Var('chromium_git') + '/chromiumos/platform2/libipp.git' + '@' + '2209bb84a8e122dab7c02fe66cc61a7b42873d7f',
      'condition': 'checkout_linux',
  },

  'src/third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git' + '@' + '927aabfcd26897abb9776ecf2a6c38ea5bb52ab6',

  'src/third_party/liblouis/src': {
      'url': Var('chromium_git') + '/external/liblouis-github.git' + '@' + '9700847afb92cb35969bdfcbbfbbb74b9c7b3376',
      'condition': 'checkout_linux',
  },

  'src/third_party/libphonenumber/dist':
    Var('chromium_git') + '/external/libphonenumber.git' + '@' + '140dfeb81b753388e8a672900fb7a971e9a0d362',

  'src/third_party/libprotobuf-mutator/src':
    Var('chromium_git') + '/external/github.com/google/libprotobuf-mutator.git' + '@' +  Var('libprotobuf-mutator'),

  'src/third_party/libsrtp':
    Var('chromium_git') + '/chromium/deps/libsrtp.git' + '@' + '000edd791434c8738455f10e0dd6b268a4852c0b',

  # Android Explicit Synchronization.
  'src/third_party/libsync/src': {
      'url': Var('chromium_git') + '/aosp/platform/system/core/libsync.git' + '@' + 'f4f4387b6bf2387efbcfd1453af4892e8982faf6',
      'condition': 'checkout_linux or checkout_android',
  },

  'src/third_party/libunwindstack': {
      'url': Var('chromium_git') + '/chromium/src/third_party/libunwindstack.git' + '@' + '0c3e395835b0f6cb7a77eba94e23e5d8b9659f71',
      'condition': 'checkout_android',
  },

  'src/third_party/libvpx/source/libvpx':
    Var('chromium_git') + '/webm/libvpx.git' + '@' +  '727319a77ffe68e9aacb08e09ae7151b3a8f70a3',

  'src/third_party/libwebm/source':
    Var('chromium_git') + '/webm/libwebm.git' + '@' + '26d9f667170dc75e8d759a997bb61c64dec42dda',

  'src/third_party/libwebp/src':
    Var('chromium_git') + '/webm/libwebp.git' + '@' +  '845d5476a866141ba35ac133f856fa62f0b7445f',

  'src/third_party/libyuv':
    Var('chromium_git') + '/libyuv/libyuv.git' + '@' + '6ac7c8f25170c85265fca69fd1fe5d31baf3344f',

  'src/third_party/lighttpd': {
      'url': Var('chromium_git') + '/chromium/deps/lighttpd.git' + '@' + Var('lighttpd_revision'),
      'condition': 'checkout_mac or checkout_win',
  },

  'src/third_party/lss': {
      'url': Var('chromium_git') + '/linux-syscall-support.git' + '@' + Var('lss_revision'),
      'condition': 'checkout_android or checkout_linux',
  },

  'src/third_party/lzma_sdk/bin/host_platform': {
      'packages': [
          {
              'package': 'infra/3pp/tools/7z/${{platform}}',
              'version': 'version:3@24.09',
          },
      ],
      'condition': 'checkout_win',
      'dep_type': 'cipd',
  },

  'src/third_party/lzma_sdk/bin/win64': {
      'packages': [
          {
              'package': 'infra/3pp/tools/7z/windows-amd64',
              'version': 'version:3@24.09',
          },
      ],
      'condition': 'checkout_win',
      'dep_type': 'cipd',
  },

  'src/third_party/material_color_utilities/src': {
      'url': Var('chromium_git') + '/external/github.com/material-foundation/material-color-utilities.git' + '@' + '13434b50dcb64a482cc91191f8cf6151d90f5465',
  },

  'src/third_party/material_design_icons/src': {
      'url': Var('chromium_git') + '/external/github.com/google/material-design-icons.git' + '@' +
          '5ab428852e35dc177a8c37a2df9dc9ccf768c65a',
      'condition': 'checkout_ios',
  },

  'src/third_party/microsoft_dxheaders/src': {
    'url': Var('chromium_git') + '/external/github.com/microsoft/DirectX-Headers.git' + '@' + '9be295b3b81ce1d0ff2b44f18d0eb86ea54c5122',
    'condition': 'checkout_win',
  },

  'src/third_party/mig/bin': {
      'packages': [
          {
              'package': 'chromium/third_party/mig/${{platform}}',
              'version': '4wxov_ILjFdgBumBqgUgOgIcr4kcMh7i4b4oJi_cLjcC',
          },
      ],
      'condition': 'host_os == "linux" and checkout_mac',
      'dep_type': 'cipd',
  },

  # Graphics buffer allocator for Chrome OS.
  'src/third_party/minigbm/src': {
      'url': Var('chromium_git') + '/chromiumos/platform/minigbm.git' + '@' + '3018207f4d89395cc271278fb9a6558b660885f5',
      'condition': 'checkout_linux',
  },

  'src/third_party/nasm': {
      'url': Var('chromium_git') + '/chromium/deps/nasm.git' + '@' +
      'f477acb1049f5e043904b87b825c5915084a9a29'
  },

  'src/third_party/neon_2_sse/src':
    Var('chromium_git') + '/external/github.com/intel/ARM_NEON_2_x86_SSE.git' + '@' + 'a15b489e1222b2087007546b4912e21293ea86ff',

  'src/third_party/netty-tcnative/src': {
      'url': Var('chromium_git') + '/external/netty-tcnative.git' + '@' + '035726f76293d142ec3c4464be0703605feb4d02',
      'condition': 'checkout_android',
  },

  'src/third_party/netty4/src': {
      'url': Var('chromium_git') + '/external/netty4.git' + '@' + 'cc4420b13bb4eeea5b1cf4f93b2755644cd3b120',
      'condition': 'checkout_android',
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

  'src/third_party/openh264/src':
    Var('chromium_git') + '/external/github.com/cisco/openh264' + '@' + '478e5ab3eca30e600006d5a0a08b176fd34d3bd1',

  'src/third_party/openscreen/src':
    Var('chromium_git') + '/openscreen' + '@' + 'cb6fd42532fc3a831d6863d5006217e32a67c417',

  'src/third_party/openxr/src': {
    'url': Var('chromium_git') + '/external/github.com/KhronosGroup/OpenXR-SDK' + '@' + '288d3a7ebc1ad959f62d51da75baa3d27438c499',
    'condition': 'checkout_openxr',
  },

  'src/third_party/opus/tests/resources': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-webrtc-resources',
      'objects': [
          {
              'object_name': '009a3ee778767c2402b1d2c920bc2449265f5a2c',
              'sha256sum': '34de3161f242895a682d9cdcbbf4ad50246742b6db46873386104cfde8a24332',
              'size_bytes': 26889600,
              'generation': 1392811661954000,
              'output_file': 'speech_mono_32_48kHz.pcm',
          },
      ],
  },

  'src/third_party/pdfium':
    Var('pdfium_git') + '/pdfium.git' + '@' +  Var('pdfium_revision'),

# Cobalt: imported
#  'src/third_party/perfetto':
#    Var('android_git') + '/platform/external/perfetto.git' + '@' + 'ea011a2c2d3aecdc4f1674887e107a56d2905edd',

  'src/base/tracing/test/data': {
    'bucket': 'perfetto',
    'objects': [
      {
        'object_name': 'test_data/chrome_fcp_lcp_navigations.pftrace-ae01d849fbd75a98be1b7ddd5a8873217c377b393a1d5bbd788ed3364f7fefc3',
        'sha256sum': 'ae01d849fbd75a98be1b7ddd5a8873217c377b393a1d5bbd788ed3364f7fefc3',
        'size_bytes': 2398645,
        'generation': 1697714434866488,
        'output_file': 'chrome_fcp_lcp_navigations.pftrace'
      },
      {
        'object_name': 'test_data/chrome_input_with_frame_view.pftrace-a93548822e481508c728ccc5da3ad34afcd0aec02ca7a7a4dad84ff340ee5975',
        'sha256sum': 'a93548822e481508c728ccc5da3ad34afcd0aec02ca7a7a4dad84ff340ee5975',
        'size_bytes': 6392331,
        'generation': 1711402389089075,
        'output_file': 'chrome_input_with_frame_view.pftrace'
      },
      {
        'object_name': 'test_data/scroll_offsets_trace_2.pftrace-2ddd9f78d91d51e39c72c520bb54fdc9dbf1333ae722e87633fc345159296289',
        'sha256sum': '2ddd9f78d91d51e39c72c520bb54fdc9dbf1333ae722e87633fc345159296289',
        'size_bytes': 1496388,
        'generation': 1712592637141461,
        'output_file': 'scroll_offsets_trace_2.pftrace'
      },
      {
        'object_name': 'test_data/top_level_java_choreographer_slices-8001e73b2458e94f65a606bb558a645ba5bca553b57fe416001f6c2175675a8a',
        'sha256sum': '8001e73b2458e94f65a606bb558a645ba5bca553b57fe416001f6c2175675a8a',
        'size_bytes': 5323017,
        'generation': 1671708979893186,
        'output_file': 'top_level_java_choreographer_slices'
      },
      {
        'object_name': 'test_data/chrome_page_load_all_categories_not_extended.pftrace.gz-6586e9e2bbc0c996caddb321a0374328654983733e6ffd7f4635ac07db32a493',
        'sha256sum': '6586e9e2bbc0c996caddb321a0374328654983733e6ffd7f4635ac07db32a493',
        'size_bytes': 1277750,
        'generation': 1652442088902445,
        'output_file': 'chrome_page_load_all_categories_not_extended.pftrace.gz'
      },
      {
        'object_name': 'test_data/speedometer_21.perfetto_trace.gz-8a159b354d74a3ca0d38ce9cd071ef47de322db4261ee266bfafe04d70310529',
        'sha256sum': '8a159b354d74a3ca0d38ce9cd071ef47de322db4261ee266bfafe04d70310529',
        'size_bytes': 891088,
        'generation': 1716566741068306,
        'output_file': 'speedometer_21.perfetto_trace.gz'
      },
      {
       'object_name': 'test_data/speedometer_3.perfetto_trace.gz-b2c77fbe2c17363432a1ad0c05c1c1c20d3ebc62bda92c041d39918011af6f02',
        'sha256sum': 'b2c77fbe2c17363432a1ad0c05c1c1c20d3ebc62bda92c041d39918011af6f02',
        'size_bytes': 1301036,
        'generation': 1716566914245446,
        'output_file': 'speedometer_3.perfetto_trace.gz'
      },
      {
        'object_name': 'test_data/scroll_jank_with_pinch.pftrace-8587d2016fdb5d39b5f852704b6e3925e9e6527af01696396be767bed04d4a45',
        'sha256sum': '8587d2016fdb5d39b5f852704b6e3925e9e6527af01696396be767bed04d4a45',
        'size_bytes': 3914720,
        'generation': 1717497788778335,
        'output_file': 'scroll_jank_with_pinch.pftrace'
      },
      {
        'object_name': 'test_data/cpu_powerups_1.pb-70f5511ba0cd6ce1359e3cadb4d1d9301fb6e26be85158e3384b06f41418d386',
        'sha256sum': '70f5511ba0cd6ce1359e3cadb4d1d9301fb6e26be85158e3384b06f41418d386',
        'size_bytes': 2033064,
        'generation': 1669652389509708,
        'output_file': 'cpu_powerups_1.pb'
      },
      {
        'object_name': 'test_data/chrome_5672_histograms.pftrace.gz-a09bd44078ac71bcfbc901b0544750e8344d0d0f6f96e220f700a5a53fa932ee',
        'sha256sum': 'a09bd44078ac71bcfbc901b0544750e8344d0d0f6f96e220f700a5a53fa932ee',
        'size_bytes': 1127472,
        'generation': 1684946598804577,
        'output_file': 'chrome_5672_histograms.pftrace.gz'
      },
      {
        'object_name': 'test_data/chrome_custom_navigation_trace.gz-ff68279e3cec94076b69259d756eed181a63eaf834d8b956a7f4ba665fabf939',
        'sha256sum': 'ff68279e3cec94076b69259d756eed181a63eaf834d8b956a7f4ba665fabf939',
        'size_bytes': 7572484,
        'generation': 1666713705258900,
        'output_file': 'chrome_custom_navigation_trace.gz'
      },
      {
        'object_name': 'test_data/scroll_offsets.pftrace-62101edb5204fec8bea30124f65d4e49bda0808d7b036e95f89445aaad6d8d98',
        'sha256sum': '62101edb5204fec8bea30124f65d4e49bda0808d7b036e95f89445aaad6d8d98',
        'size_bytes': 769741,
        'generation': 1693402148909129,
        'output_file': 'scroll_offsets.pftrace'
      },
      {
        'object_name': 'test_data/chrome_input_with_frame_view_new.pftrace-e901ad9577088e62c921dd8bfcb43d652ecf49fa69b5b57f81bb3d27dbe94e12',
        'sha256sum': 'e901ad9577088e62c921dd8bfcb43d652ecf49fa69b5b57f81bb3d27dbe94e12',
        'size_bytes': 1967821,
        'generation': 1719520814352733,
        'output_file': 'chrome_input_with_frame_view_new.pftrace'
      },
      {
        'object_name': 'test_data/scroll_m131.pftrace-14171c9e502a65a454f39fe14fce8b313c7012a2c14394bed496fc93b1644b0d',
        'sha256sum': '14171c9e502a65a454f39fe14fce8b313c7012a2c14394bed496fc93b1644b0d',
        'size_bytes': 1247290,
        'generation': 1729619115836875,
        'output_file': 'scroll_m131.pftrace'
      }
    ],
    'dep_type': 'gcs'
  },

  'src/third_party/perl': {
      'url': Var('chromium_git') + '/chromium/deps/perl.git' + '@' + '8ef97ff3b7332e38e61b347a2fbed425a4617151',
      'condition': 'checkout_win',
  },

  'src/third_party/protobuf-javascript/src':
    Var('chromium_git') + '/external/github.com/protocolbuffers/protobuf-javascript' + '@' + 'e34549db516f8712f678fcd4bc411613b5cc5295',

  'src/third_party/pthreadpool/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/pthreadpool.git' + '@' + '560c60d342a76076f0557a3946924c6478470044',

  # Dependency of skia.
  'src/third_party/pyelftools': {
      'url': Var('chromium_git') + '/chromiumos/third_party/pyelftools.git' + '@' + '19b3e610c86fcadb837d252c794cb5e8008826ae',
      'condition': 'checkout_linux',
  },

  'src/third_party/quic_trace/src':
    Var('chromium_git') + '/external/github.com/google/quic-trace.git' + '@' + '413da873d93a03d3662f24b881ea459a79f9c589',

  'src/third_party/pywebsocket3/src':
    Var('chromium_git') + '/external/github.com/GoogleChromeLabs/pywebsocket3.git' + '@' + '50602a14f1b6da17e0b619833a13addc6ea78bc2',

  'src/third_party/qemu-linux-arm64': {
      'packages': [
          {
              'package': 'fuchsia/third_party/qemu/linux-arm64',
              'version': 'hOpuGIMj1FAtBWGDlXARkCm2srxY4enn8iI3AgrDna4C'
          },
      ],
      # TODO(b/351926334): Do not add `non_git_source` to this condition until the bug is fixed.
      'condition': 'host_os == "linux" and checkout_fuchsia and checkout_fuchsia_for_arm64_host',
      'dep_type': 'cipd',
  },

  'src/third_party/edk2': {
      'packages': [
          {
              'package': 'fuchsia/third_party/edk2',
              'version': 'TfGjbhGrxzU0x2fYk8elEgwMTrvwe-3DSPTQe4gb0tMC'
          },
      ],
      # TODO(b/351926334): Do not add `non_git_source` to this condition until the bug is fixed.
      'condition': 'host_os == "linux" and checkout_fuchsia and checkout_fuchsia_for_arm64_host',
      'dep_type': 'cipd',
  },

  'src/third_party/re2/src':
    Var('chromium_git') + '/external/github.com/google/re2.git' + '@' + '6dcd83d60f7944926bfd308cc13979fc53dd69ca',

  'src/third_party/r8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': '6tS5W0W6vy5TPDxAc-wTObNng6QBVt7cQr3lazvrbpEC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  # This duplication is intentional, so we avoid updating the r8.jar used by
  # dexing unless necessary, since each update invalidates all incremental
  # dexing and unnecessarily slows down all bots.
  'src/third_party/r8/d8/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/r8',
              'version': 'MnTG4zEzUB1GchkIsnkYCRziw_tkkWGJ7v_T6NUJHksC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/requests/src': {
      'url': Var('chromium_git') + '/external/github.com/kennethreitz/requests.git' + '@' + 'c7e0fc087ceeadb8b4c84a0953a422c474093d6d',
      'condition': 'checkout_android',
  },

  'src/third_party/robolectric/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/robolectric',
              'version': 'Y1B0M_fCpPZ058xErMX6GQOJEVRBWR342juuxNLpVnkC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/ruy/src':
    Var('chromium_git') + '/external/github.com/google/ruy.git' + '@' + 'c08ec529fc91722bde519628d9449258082eb847',

  'src/third_party/skia':
    Var('skia_git') + '/skia.git' + '@' +  Var('skia_revision'),

  'src/third_party/smhasher/src':
    Var('chromium_git') + '/external/smhasher.git' + '@' + 'e87738e57558e0ec472b2fc3a643b838e5b6e88f',

  'src/third_party/snappy/src':
    Var('chromium_git') + '/external/github.com/google/snappy.git' + '@' + 'c9f9edf6d75bb065fa47468bf035e051a57bec7c',

  'src/third_party/sqlite/src':
    Var('chromium_git') + '/chromium/deps/sqlite.git' + '@' + '567495a62a62dc013888500526e82837d727fe01',

  'src/third_party/sqlite4java/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/sqlite4java',
              'version': 'LofjKH9dgXIAJhRYCPQlMFywSwxYimrfDeBmaHc-Z5EC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/subresource-filter-ruleset/data': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-ads-detection',
      'objects': [
          {
              'object_name': '782009d9765c6104a1b4d1eac553834e7e399d749a082ead42bb47abb42895b5',
              'sha256sum': '782009d9765c6104a1b4d1eac553834e7e399d749a082ead42bb47abb42895b5',
              'size_bytes': 74598,
              'generation': 1728068518927575,
              'output_file': 'UnindexedRules',
          },
      ],
  },

  'src/third_party/swift-format': {
      'packages': [
          {
              'package': 'infra/3pp/tools/swift-format/mac-${{arch}}',
              'version': 'version:2@505.chromium.1',
          },
      ],
      'condition': 'host_os == mac',
      'dep_type': 'cipd',
  },

  'src/third_party/swiftshader':
    Var('swiftshader_git') + '/SwiftShader.git' + '@' +  Var('swiftshader_revision'),

  'src/third_party/swift-toolchain': {
      'packages': [
          {
              'package': 'chromium/tools/swift-toolchain/mac-amd64',
              'version': 'version:2@5.8-release',
          },
      ],
      'condition': 'host_os == mac',
      'dep_type': 'cipd',
  },

  'src/third_party/test_fonts/test_fonts': {
      'dep_type': 'gcs',
      'condition': 'non_git_source',
      'bucket': 'chromium-fonts',
      'objects': [
          {
              'object_name': 'f26f29c9d3bfae588207bbc9762de8d142e58935c62a86f67332819b15203b35',
              'sha256sum': 'f26f29c9d3bfae588207bbc9762de8d142e58935c62a86f67332819b15203b35',
              'size_bytes': 32750602,
              'generation': 1717109450425063,
          },
      ],
  },

  'src/third_party/text-fragments-polyfill/src':
    Var('chromium_git') + '/external/github.com/GoogleChromeLabs/text-fragments-polyfill.git' + '@' + 'c036420683f672d685e27415de0a5f5e85bdc23f',

  'src/third_party/tflite/src':
    Var('chromium_git') + '/external/github.com/tensorflow/tensorflow.git' + '@' + '658227d3b535287dc6859788bde6076c4fe3fe7c',

  'src/third_party/turbine/cipd': {
      'packages': [
          {
              'package': 'chromium/third_party/turbine',
              'version': '1iVaPZ0ts_R6ymFUftgOKVE50dXSx6duVZEOP891WCMC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/vulkan-deps': '{chromium_git}/vulkan-deps@0b56dd5952b25fad65139b64096fcd187048ed38',
  'src/third_party/glslang/src': '{chromium_git}/external/github.com/KhronosGroup/glslang@9c644fcb5b9a1a9c975c50a790fd14c5451292b0',
  'src/third_party/spirv-cross/src': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',
  'src/third_party/spirv-headers/src': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@996c728cf7dcfb29845cfa15222822318f047810',
  'src/third_party/spirv-tools/src': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@9117e042b93d4ff08d2406542708170f77aaa2a3',
  'src/third_party/vulkan-headers/src': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@cbcad3c0587dddc768d76641ea00f5c45ab5a278',
  'src/third_party/vulkan-loader/src': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@b0177a972b8d47e823a4500cf88df88a8c27add7',
  'src/third_party/vulkan-tools/src': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@15f2de809304aba619ee327f3273425418ca83de',
  'src/third_party/vulkan-utility-libraries/src': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Utility-Libraries@87ab6b39a97d084a2ef27db85e3cbaf5d2622a09',
  'src/third_party/vulkan-validation-layers/src': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@bc2c38412f739c298d6f5c076c064e6b5696959f',

  'src/third_party/vulkan_memory_allocator':
    Var('chromium_git') + '/external/github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git' + '@' + '56300b29fbfcc693ee6609ddad3fdd5b7a449a21',

  # Display server protocol for Linux.
  'src/third_party/wayland/src': {
      'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland.git' + '@' + 'a156431ea66fe67d69c9fbba8a8ad34dabbab81c',
      'condition': 'checkout_linux',
  },

  # Wayland protocols that add functionality not available in the core protocol.
  'src/third_party/wayland-protocols/src': {
      'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/wayland-protocols.git' + '@' + '7d5a3a8b494ae44cd9651f9505e88a250082765e',
      'condition': 'checkout_linux',
  },

  # Additional Wayland protocols specific for KDE Plasma desktop environment.
  'src/third_party/wayland-protocols/kde': {
      'url': Var('chromium_git') + '/external/github.com/KDE/plasma-wayland-protocols.git' + '@' + '0b07950714b3a36c9b9f71fc025fc7783e82926e',
      'condition': 'checkout_linux',
  },

  # Additional Wayland protocols specific for GNOME desktop environment.
  'src/third_party/wayland-protocols/gtk': {
      'url': Var('chromium_git') + '/external/github.com/GNOME/gtk.git' + '@' + '40ebed3a03aef096addc0af09fec4ec529d882a0',
      'condition': 'checkout_linux',
  },

  # Keep this to the same revision as the one .vpython3.
  'src/third_party/webdriver/pylib':
    Var('chromium_git') + '/external/github.com/SeleniumHQ/selenium/py.git' + '@' + 'fc5e7e70c098bfb189a9a74746809ad3c5c34e04',

  'src/third_party/webgl/src':
    Var('chromium_git') + '/external/khronosgroup/webgl.git' + '@' + '450cceb587613ac1469c5a131fac15935c99e0e7',

  'src/third_party/webgpu-cts/src':
    Var('chromium_git') + '/external/github.com/gpuweb/cts.git' + '@' + 'b9f32fd2943dd2b3d0033bf938c9d843f4b5c9a9',

# Cobalt: imported
#  'src/third_party/webrtc':
#    Var('webrtc_git') + '/src.git' + '@' + 'afaf497805cbb502da89991c2dcd783201efdd08',

  # Wuffs' canonical repository is at github.com/google/wuffs, but we use
  # Skia's mirror of Wuffs, the same as in upstream Skia's DEPS file.
  #
  # The local directory is called "third_party/wuffs" (matching upstream Skia
  # and other non-Chromium, Skia-using projects) even though the git repo we
  # clone is called "wuffs-mirror-release-c". The reasons for using w-m-r-c are
  # listed in the https://crrev.com/c/3086053 commit message. One reason is
  # that the w-m-r-c subset is much smaller and changes much less frequently.
  'src/third_party/wuffs/src':
    Var('skia_git') + '/external/github.com/google/wuffs-mirror-release-c.git' + '@' +  Var('wuffs_revision'),

  'src/third_party/weston/src': {
      'url': Var('chromium_git') + '/external/anongit.freedesktop.org/git/wayland/weston.git' + '@' + 'ccf29cb237c3ed09c5f370f35239c93d07abfdd7',
      'condition': 'checkout_linux',
  },

  # A conformance-suite developed by canonical for the mir wayland server.
  # Required to compile exo_wlcs on chromeos checkouts.
  'src/third_party/wlcs/src': {
      'url': Var('chromium_git') + '/external/github.com/MirServer/wlcs.git' + '@' + '2930ad4b5ca602446ad211b49fb1827303ce9f4b',
      'condition': 'checkout_chromeos',
  },

  'src/third_party/xdg-utils': {
      'url': Var('chromium_git') + '/chromium/deps/xdg-utils.git' + '@' + 'cb54d9db2e535ee4ef13cc91b65a1e2741a94a44',
      'condition': 'checkout_linux',
  },

  'src/third_party/xnnpack/src':
    Var('chromium_git') + '/external/github.com/google/XNNPACK.git' + '@' + 'd1d33679661a34f03a806af2b813f699db3004f9',

  'src/tools/page_cycler/acid3':
    Var('chromium_git') + '/chromium/deps/acid3.git' + '@' + 'a926d0a32e02c4c03ae95bb798e6c780e0e184ba',

  'src/third_party/libei/cipd': {

    'packages': [
      {
        'package': 'chromium/third_party/libei/linux-amd64',
        'version': 'PQz4zG5Q3SXoAaCYq3RK99W3wg_v0NoOu1OzTSvA_oIC',
      },
    ],

    'condition': 'checkout_linux and non_git_source',
    'dep_type': 'cipd',
  },

  'src/third_party/zstd/src':
    Var('chromium_git') + '/external/github.com/facebook/zstd.git' + '@' + '7fb5347e88f10472226c9aa1962a148e55d8c480',

  'src/tools/skia_goldctl/linux': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/linux-amd64',
          'version': 'AiY7CO223oVpSMe9b7SwHVhQrF-ijgHEDzjsAV8SUKoC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_linux and non_git_source',
  },
  'src/tools/skia_goldctl/win': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/windows-amd64',
          'version': 'EK_tQRk4h7ikvzyk5kFX_pkGw3gjHkP4QPmEjN8D54EC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_win',
  },

  'src/tools/skia_goldctl/mac_amd64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-amd64',
          'version': 'aWw1JVWpKckecy3rBuN-5Y_osHIckC0ganu7xQ28f4UC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac',
  },

  'src/tools/skia_goldctl/mac_arm64': {
      'packages': [
        {
          'package': 'skia/tools/goldctl/mac-arm64',
          'version': 'WnSB4H9RkKT6MKFpLZ9yfWPnBUeECncOVa3XuRnyTEEC',
        },
      ],
      'dep_type': 'cipd',
      'condition': 'checkout_mac',
  },

# Cobalt: imported
#  'src/v8':
#    Var('chromium_git') + '/v8/v8.git' + '@' +  Var('v8_revision'),

  'src/internal': {
    'url': Var('chrome_git') + '/chrome/src-internal.git' + '@' + Var('src_internal_revision'),
    'condition': 'checkout_src_internal',
  },

  'src/ash/ambient/resources': {
    'packages': [
      {
        'package': 'chromeos_internal/assistant/ambient',
        'version': 'version:feel_the_breeze_with_frame_rate_markers',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/ash/webui/eche_app_ui/resources/prod': {
    'packages': [
      {
        'package': 'chromeos_internal/apps/eche_app/app',
        'version': 'g2qgVbVmWuLNtrl4K0dJFPWk48zNZxWOT4-5bLFzboUC',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/ash/webui/boca_ui/resources/prod': {
    'packages': [
      {
        'package': 'chromeos_internal/apps/boca_app/app',
        'version': '2dbOK28kFj1aMiBL5ZdjS-azTkgCmcWzh2M1QLLqxhcC',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/ash/webui/help_app_ui/resources/prod': {
    'packages': [
      {
        'package': 'chromeos_internal/apps/help_app/app',
        'version': 'qyixOjRLu0e0duF0cpNUnTvQXjvE85NTbMcuY5SCpXUC',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/ash/webui/media_app_ui/resources/prod': {
    'packages': [
      {
        'package': 'chromeos_internal/apps/media_app/app',
        'version': 'tgsX8wV0auv9Kzp6jHSMbboUTXUe6JmXPWAAzLzjuJcC',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/ash/webui/personalization_app/resources': {
    'packages': [
      {
        'package': 'chromeos_internal/assistant/time_of_day',
        'version': '7okw0Y1HdRp76vhM8AGsWOloCQ83hwMd7Y1k2sDYMJcC',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/ash/webui/shimless_rma/resources': {
    'packages': [
      {
        'package': 'chromeos_internal/ash/peripherals-and-serviceability/shimless-rma-project-simon-strings',
        'version': '-uRXiZeA4Yl-Nv-6jP69DyDs5cGroZgGsa1NHnVySQwC',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/ash/webui/projector_app/resources/prod': {
    'packages': [
      {
        'package': 'chromeos_internal/apps/projector_app/app',
        'version': 'hwy--OoqyMlYGskuNUsVDbSNNVcUk8Km72Xt-Prfa8gC',
      },
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/third_party/webpagereplay/cipd': {
      'packages' : [
          {
              'package': 'infra/tools/wpr/linux_x86_64',
              'version': 'y28SfbEF6nHSkZ1eHysM1t711zpOCmtk7jUdxZB-QSMC',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_prebuilts/build_tools': {
      'url': Var('android_git') + '/platform/prebuilts/build-tools.git' + '@' + '3fbcdf38727f967badc13f9537bda0b4392c1f74',
      'condition': 'checkout_android_prebuilts_build_tools',
  },

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

  'src/third_party/android_deps/cipd/libs/com_google_android_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_annotations',
              'version': 'version:2@4.1.1.4.cr1',
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

  'src/third_party/android_deps/cipd/libs/com_google_android_datatransport_transport_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_datatransport_transport_api',
              'version': 'version:2@2.2.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_ads_identifier': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_ads_identifier',
              'version': 'version:2@18.2.0.cr1',
          },
      ],
      'condition': 'checkout_android',
      'dep_type': 'cipd',
  },
  
  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_auth': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth',
              'version': 'version:2@21.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_auth_api_phone': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_api_phone',
              'version': 'version:2@18.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_auth_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_base',
              'version': 'version:2@18.0.10.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_auth_blockstore': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_auth_blockstore',
              'version': 'version:2@16.4.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_base': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_base',
              'version': 'version:2@18.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_basement': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_basement',
              'version': 'version:2@18.4.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_cast': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_cast_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cast_framework',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_clearcut': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_clearcut',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_cloud_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_cloud_messaging',
              'version': 'version:2@16.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_fido': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_fido',
              'version': 'version:2@21.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_flags': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_flags',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_gcm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_gcm',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_identity_credentials': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_identity_credentials',
              'version': 'version:2@16.0.0-alpha02.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_iid',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_instantapps': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_instantapps',
              'version': 'version:2@18.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_location': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_location',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_phenotype': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_phenotype',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_stats': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_stats',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_tasks': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_tasks',
              'version': 'version:2@18.2.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_vision': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision',
              'version': 'version:2@20.1.3.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_gms_play_services_vision_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_gms_play_services_vision_common',
              'version': 'version:2@19.1.3.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_libraries_identity_googleid_googleid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_libraries_identity_googleid_googleid',
              'version': 'version:2@1.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_material_material': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_material_material',
              'version': 'version:2@1.13.0-alpha05.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_play_core_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_play_core_common',
              'version': 'version:2@2.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_android_play_feature_delivery': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_android_play_feature_delivery',
              'version': 'version:2@2.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_auto_service_auto_service_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_service_auto_service_annotations',
              'version': 'version:2@1.0-rc6.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_auto_value_auto_value_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_auto_value_auto_value_annotations',
              'version': 'version:2@1.10.4.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_code_findbugs_jsr305': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_findbugs_jsr305',
              'version': 'version:2@3.0.2.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_code_gson_gson': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_code_gson_gson',
              'version': 'version:2@2.9.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_dagger_dagger': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_dagger',
              'version': 'version:2@2.52.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_dagger_hilt_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_dagger_hilt_core',
              'version': 'version:2@2.52.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_errorprone_error_prone_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_errorprone_error_prone_annotations',
              'version': 'version:2@2.30.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_annotations',
              'version': 'version:2@16.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_common': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_common',
              'version': 'version:2@19.5.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_components': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_components',
              'version': 'version:2@16.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_encoders': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders',
              'version': 'version:2@16.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_encoders_json': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_encoders_json',
              'version': 'version:2@17.1.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_iid': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_iid_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_iid_interop',
              'version': 'version:2@17.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_installations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations',
              'version': 'version:2@16.3.5.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_installations_interop': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_installations_interop',
              'version': 'version:2@16.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_measurement_connector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_measurement_connector',
              'version': 'version:2@18.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_firebase_firebase_messaging': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_firebase_firebase_messaging',
              'version': 'version:2@21.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_guava_failureaccess': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_failureaccess',
              'version': 'version:2@1.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_guava_guava': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava',
              'version': 'version:2@32.1.3-jre.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_guava_guava_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_guava_guava_android',
              'version': 'version:2@32.1.3-android.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_j2objc_j2objc_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_j2objc_j2objc_annotations',
              'version': 'version:2@2.8.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_google_protobuf_protobuf_javalite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_google_protobuf_protobuf_javalite',
              'version': 'version:2@4.28.0.cr1',
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

  'src/third_party/android_deps/cipd/libs/com_squareup_javawriter': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_javawriter',
              'version': 'version:2@2.1.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_squareup_moshi_moshi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_moshi_moshi',
              'version': 'version:2@1.15.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_squareup_moshi_moshi_adapters': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_moshi_moshi_adapters',
              'version': 'version:2@1.15.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_squareup_okio_okio_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_okio_okio_jvm',
              'version': 'version:2@3.9.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/com_squareup_wire_wire_runtime_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/com_squareup_wire_wire_runtime_jvm',
              'version': 'version:2@5.0.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/io_grpc_grpc_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_api',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/io_grpc_grpc_binder': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_binder',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/io_grpc_grpc_context': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_context',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/io_grpc_grpc_core': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_core',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/io_grpc_grpc_protobuf_lite': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_protobuf_lite',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/io_grpc_grpc_stub': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_grpc_grpc_stub',
              'version': 'version:2@1.49.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/io_perfmark_perfmark_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/io_perfmark_perfmark_api',
              'version': 'version:2@0.25.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/jakarta_inject_jakarta_inject_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/jakarta_inject_jakarta_inject_api',
              'version': 'version:2@2.0.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/javax_annotation_javax_annotation_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_javax_annotation_api',
              'version': 'version:2@1.3.2.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/javax_annotation_jsr250_api': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_annotation_jsr250_api',
              'version': 'version:2@1.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/javax_inject_javax_inject': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/javax_inject_javax_inject',
              'version': 'version:2@1.cr1',
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

  'src/third_party/android_deps/cipd/libs/org_bouncycastle_bcprov_jdk18on': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_bouncycastle_bcprov_jdk18on',
              'version': 'version:2@1.77.cr1',
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

  'src/third_party/android_deps/cipd/libs/org_checkerframework_checker_qual': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_qual',
              'version': 'version:2@3.37.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_checkerframework_checker_util': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_checkerframework_checker_util',
              'version': 'version:2@3.25.0.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_codehaus_mojo_animal_sniffer_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_codehaus_mojo_animal_sniffer_annotations',
              'version': 'version:2@1.21.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_conscrypt_conscrypt_openjdk_uber': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_conscrypt_conscrypt_openjdk_uber',
              'version': 'version:2@2.5.2.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_hamcrest_hamcrest': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_hamcrest_hamcrest',
              'version': 'version:2@2.2.cr1',
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

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_android',
              'version': 'version:2@1.8.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_coroutines_core_jvm',
              'version': 'version:2@1.8.1.cr1',
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

  'src/third_party/android_deps/cipd/libs/org_jetbrains_kotlinx_kotlinx_serialization_core_jvm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jetbrains_kotlinx_kotlinx_serialization_core_jvm',
              'version': 'version:2@1.7.2.cr1',
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

  'src/third_party/android_deps/cipd/libs/org_jspecify_jspecify': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_jspecify_jspecify',
              'version': 'version:2@1.0.0.cr1',
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

  'src/third_party/android_deps/cipd/libs/org_ow2_asm_asm': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_ow2_asm_asm_analysis': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_analysis',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_ow2_asm_asm_commons': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_commons',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_ow2_asm_asm_tree': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_tree',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_ow2_asm_asm_util': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_ow2_asm_asm_util',
              'version': 'version:2@9.7.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_annotations': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_annotations',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_junit': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_junit',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_nativeruntime': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_nativeruntime',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_nativeruntime_dist_compat': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_nativeruntime_dist_compat',
              'version': 'version:2@1.0.9.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_pluginapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_pluginapi',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_plugins_maven_dependency_resolver': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_plugins_maven_dependency_resolver',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_resources': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_resources',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_robolectric': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_robolectric',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_sandbox': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_sandbox',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_shadowapi': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadowapi',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_shadows_framework': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_framework',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_shadows_versioning': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_shadows_versioning',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_utils': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/android_deps/cipd/libs/org_robolectric_utils_reflector': {
      'packages': [
          {
              'package': 'chromium/third_party/android_deps/libs/org_robolectric_utils_reflector',
              'version': 'version:2@4.12.1.cr1',
          },
      ],
      'condition': 'checkout_android and non_git_source',
      'dep_type': 'cipd',
  },

  # === ANDROID_DEPS Generated Code End ===

  'src/tools/resultdb': {
      'packages': [
        {
          'package': 'infra/tools/result_adapter/${{platform}}',
          'version': Var('resultdb_version'),
        },
      ],
      'condition': 'non_git_source',
      'dep_type': 'cipd',
  },

  # Dependencies from src_internal
  'src/chromeos/ash/resources/internal': {
      'url': Var('chrome_git') + '/chrome/chromeos/ash/resources/internal.git' + '@' +
        '35dd3a45e078691e7cbf973d155c4f235eaafc3c',
      'condition': 'checkout_src_internal and checkout_chromeos',
  },

  'src/chromeos/assistant/internal': {
      'url': Var('chrome_git') + '/chrome/assistant.git' + '@' +
        '99deb76682f8bf5fb1d0b7229f18ef110dbba92d',
      'condition': 'checkout_src_internal and checkout_chromeos',
    },

  'src/ui/gl/resources/angle-metal': {
    'packages': [{
       'package': 'chromium/gpu/angle-metal-shader-libraries',
       'version': 'S0FPOVKrgaiqyuR20SSHiPorLgYez29bfwEdKBobUMMC',
    }],
    'dep_type': 'cipd',
    'condition': 'checkout_mac or checkout_ios',
  },
  'src/buildtools/android/doclava': {
    'dep_type': 'gcs',
    'bucket': 'chromium-doclava',
    'condition': 'checkout_android and non_git_source',
    'objects': [
      {
      'object_name': '1931becb8a8e21685f39c62854e9e814d64ccf1a',
      'sha256sum': '935a602c0dda7f769fc48fc777697ea23ccead170fbc70d2d41af5713ef50bbf',
      'size_bytes': 3517760,
      'generation': 1516199210583319,
      },
    ],
  },
  # Repositories from src_internal
  'src/build/fuchsia/internal': {
      'url': Var('chrome_git') + '/fuchsia/build.git' + '@' +
        '16da074bda38d989dbcbee0c7c75e2aa83783bb2',
      'condition': 'checkout_fuchsia_internal and checkout_src_internal',
  },

  'src/chrome/app/theme/default_100_percent/google_chrome': {
      'url': Var('chrome_git') + '/chrome/theme/default_100_percent/google_chrome.git' + '@' +
        'c5fa2fa6eebebdd7a8f6886cff6968b59b63284f',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/app/theme/default_200_percent/google_chrome': {
      'url': Var('chrome_git') + '/chrome/theme/default_200_percent/google_chrome.git' + '@' +
        '323e8f4ce9be0212708cce6b765a632a1a9d824c',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/app/theme/google_chrome': {
      'url': Var('chrome_git') + '/chrome/theme/google_chrome.git' + '@' +
        '50ebc6a7fdc7a9e2b65119d5e4f7e866e265eb70',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/enterprise/connectors/internal': {
      'url': Var('chrome_git') + '/chrome/browser/enterprise/connectors/internal.git' + '@' +
        '7fd7c8dd496740724d0024408ae7a96298e34aa2',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/google/linkdoctor_internal': {
      'url': Var('chrome_git') + '/chrome/linkdoctor.git' + '@' +
        'fe28a8f90c5471f20f8fee9ff7f6c6f8b8d02bed', # from svn revision 32577
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/internal': {
      'url': Var('chrome_git') + '/chrome/browser_internal.git' + '@' +
        '5649dd0237ac7f75c2779ff757e8666bdb9d2674',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/media/engagement_internal': {
      'url': Var('chrome_git') + '/chrome/browser/media/engagement_internal.git' + '@' +
        '14b00ddbb904612ec8805f00718ae3f95c02a076',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/nearby_sharing/internal': {
      'url': Var('chrome_git') + '/chrome/browser/nearby_sharing/internal.git' + '@' +
        '5fe4183de2df3467add81f4610f79ea1dde41a48',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/platform_experience/win': {
      'url': Var('chrome_git') + '/chrome/browser/platform_experience/win.git' + '@' +
        '89ea316ccb1d7b612a37f1d53399ef3d14fc9be3',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/request_header_integrity/internal': {
      'url': Var('chrome_git') + '/chrome/browser/request_header_integrity/internal.git' + '@' +
        '1592aa7ba598a048f918f54735bcf056556532c3',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/resources/downloads/internal': {
      'url': Var('chrome_git') + '/chrome/browser/resources/downloads_internal.git' + '@' +
        '4fefb8a24ae6c421f640b9ef028e4b4ca75df729',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/resources/settings/internal': {
      'url': Var('chrome_git') + '/chrome/browser/resources/settings_internal.git' + '@' +
        'bc502cc43fa3999514c63c96aa75239c9d1acf91', # from svn revision 41419
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/spellchecker/internal': {
      'url': Var('chrome_git') + '/chrome/spellchecker/internal.git' + '@' +
        'a22002a5b3cf7c6b872b25712af97a5664a350e2', # from svn revision 24388
      'condition': 'checkout_src_internal',
  },

  'src/chrome/browser/resources/chromeos/mako/resources': {
    'packages' : [
      {
        'package': 'chromeos_internal/inputs/orca',
        'version': 'nHZAB6k_K5IIS-x4cuGYQWvKRu_O6TT9WmmwT2jer4YC'
      }
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/chrome/browser/resources/chromeos/seal/resources': {
    'packages' : [
      {
        'package': 'chromeos_internal/inputs/seal',
        'version': '11AdGL1RBEo2LflLT5Vc8Q3vBfjsHQAuH5jAhUBxL9QC'
      }
    ],
    'condition': 'checkout_chromeos and checkout_src_internal',
    'dep_type': 'cipd',
  },

  'src/chrome/elevation_service/internal': {
    'url': Var('chrome_git') + '/chrome/elevation_service/internal.git' + '@' +
        '375eccc61c37c3b5026bcb704fc6c5d36d65a0fd',
    'condition': 'checkout_src_internal and checkout_win',
  },

  # Installer bits used only by Mac, but mapped for all OSes to ease source
  # grepping.
  'src/chrome/installer/mac/internal': {
      'url': Var('chrome_git') + '/chrome/installer/mac/internal.git' + '@' +
        '85c282fe6f2763822964028cb89be7b808b07169',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/test/data/firefox3_profile/searchplugins': {
      'url': Var('chrome_git') + '/chrome/data/osdd/firefox3_profile_searchplugins.git' + '@' +
        '6cf09b86fb9d058453e7d05978ff8e91b5e8e749',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/test/data/firefox3_searchplugins': {
      'url': Var('chrome_git') + '/chrome/data/osdd/firefox3_searchplugins.git' + '@' +
        '490580801915834d72dd8a1e943924c35df45673',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/test/data/gpu/vt': {
      'url': Var('chrome_git') + '/chrome/data/vectortown_endurance/vectortownstatic-20121022.git' + '@' +
        'c34f30f909a414d378a1678eba921e58940708c4',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/test/data/perf/frame_rate/private': {
      'url': Var('chrome_git') + '/chrome/data/frame_rate_tests.git' + '@' +
        '6394c925a272b92a4e9e16d929af049b7aa6e4f8',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/test/data/perf/private': {
      'url': Var('chrome_git') + '/chrome/data/perf_tests.git' + '@' +
        '6f3e320b1fa10910eb1dcbad36afdd1ad00b2c5a',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/test/data/pdf_private': {
      'url': Var('chrome_git') + '/chrome/data/pdf_private.git' + '@' +
        '23b64c03647779d193ee8ccb3f2a1a5560da9c94',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/test/media_router/internal': {
      'url': Var('chrome_git') + '/chrome/test/media_router/internal.git' + '@' +
        '99937b3180700d9fb63eace8c762c7a5977a301f',
      'condition': 'checkout_src_internal',
  },

  'src/chrome/tools/memory': {
      'url': Var('chrome_git') + '/chrome/tools/memory.git' + '@' +
        '3c9359382236f6d57c91505234a2bc7fd635ba6c',
      'condition': 'checkout_win and checkout_src_internal',
  },

  'src/chrome/services/speech/internal': {
      'url': Var('chrome_git') + '/chromeos/speech.git' + '@' + '917c83b7c79126906c5d19668256b9d9139a0e71',
      'condition': 'checkout_chromeos and checkout_src_internal',
   },

  'src/chrome/updater/internal': {
    'url': Var('chrome_git') + '/chrome/updater/internal.git' + '@' +
        'e8e0e06cc5b990d06b4583886f0f8946241fb221',
    'condition': 'checkout_src_internal',
  },

  'src/components/accessibility/internal': {
      'url': Var('chrome_git') + '/chrome-accessibility.git' + '@' +
        '2e6c405fd5f09ad9d8933bc531af8c5dd24f348c',
      'condition': 'checkout_src_internal',
  },

  'src/components/autofill/core/browser/form_parsing/internal_resources': {
      'url': Var('chrome_git') + '/chrome/components/autofill_regex_patterns.git' + '@' +
        'c751dd94cd94ac5c6b0e9e7a8be5853f254c3270',
      'condition': 'checkout_src_internal',
  },

  'src/components/crash/core/app/internal': {
    'url': Var('chrome_git') + '/chrome/components/crash.git' + '@' + '977755983e64eb91813875407050afcc7c1b6683',
    'condition': 'checkout_src_internal',
  },

  'src/components/metrics/internal': {
      'url': Var('chrome_git') + '/chrome/components/metrics/internal.git' + '@' +
        '8e91fd73c988a39117cb6cca252af1d8bb08129d',
      'condition': 'checkout_src_internal',
  },

  'src/components/ntp_tiles/resources/internal': {
      'url': Var('chrome_git') + '/chrome/components/ntp_tiles/resources.git' + '@' +
        '48c257ae331a9c642af38b8f62cb2c789e2a7da6',
      'condition': 'checkout_src_internal',
  },

  'src/components/optimization_guide/internal': {
      'url': Var('chrome_git') + '/chrome/components/optimization_guide.git' + '@' +
        '0e0e274f159157125eca68faef8c8257211a8ea8',
      'condition': 'checkout_src_internal',
  },

  'src/components/plus_addresses/resources/internal': {
      'url': Var('chrome_git') + '/chrome/components/plus_addresses/resources.git' + '@' +
        '896889722227d6b6a1b90187fcaffc084ada5932',
      'condition': 'checkout_src_internal',
  },

  'src/components/resources/default_100_percent/google_chrome': {
      'url': Var('chrome_git') + '/chrome/components/default_100_percent/google_chrome.git' + '@' +
        'e147db95eb4f7baa20207a513485e019af0e18bb',
      'condition': 'checkout_src_internal',
  },

  'src/components/resources/default_200_percent/google_chrome': {
      'url': Var('chrome_git') + '/chrome/components/default_200_percent/google_chrome.git' + '@' +
        '22f067172eb11f6b7191aa1ec1aec38b0045839b',
      'condition': 'checkout_src_internal',
  },

  'src/components/resources/default_300_percent/google_chrome': {
      'url': Var('chrome_git') + '/chrome/components/default_300_percent/google_chrome.git' + '@' +
        'd01fae2b485ce5571b9d2e9d766cbbda687c21ad',
      'condition': 'checkout_src_internal',
  },

  'src/components/site_isolation/internal': {
      'url': Var('chrome_git') + '/chrome/components/site_isolation.git' + '@' +
        'e0d8a7769c1daabb974bf0d229970534a0aede77',
      'condition': 'checkout_src_internal',
  },

  'src/components/test/data/autofill/heuristics-json/internal': {
      'url': Var('chrome_git') + '/chrome/test/autofill/structured_forms.git' + '@' +
        '482fcb02e341795240ec419db144ce4f344c1160',
      'condition': 'checkout_chromium_autofill_test_dependencies',
  },

  'src/components/test/data/autofill/label-doms/internal': {
      'url': Var('chrome_git') + '/chrome/test/autofill/field_labels.git' + '@' +
        'c67083dbb39d4bd87d86c083fecbe35aa562c777',
      'condition': 'checkout_chromium_autofill_test_dependencies',
  },

  'src/components/vector_icons/google_chrome': {
      'url': Var('chrome_git') + '/chrome/vector_icons/google_chrome.git' + '@' +
        'ecad7931572c1f673bf582b785838daadbaadfab',
      'condition': 'checkout_src_internal',
  },

  'src/content/test/data/plugin': {
      'url': Var('chrome_git') + '/chrome/data/chrome_plugin_tests.git' + '@' +
        '3e80d4d08f5421d6bc9340964834ebc903a318aa',
      'condition': 'checkout_src_internal',
  },

  'src/google_apis/internal': {
      'url': Var('chrome_git') + '/chrome/google_apis/internal.git' + '@' +
        '6279cdf8399f2083ae476c3d2447734a453e9552',
      'condition': 'checkout_src_internal',
  },

  'src/ios_internal':  {
      'url': Var('chrome_git') + '/chrome/ios_internal.git' + '@' +
        '05a343f947c7b5f4dfe1c0cbf88151434da289c7',
      'condition': 'checkout_ios and checkout_src_internal',
  },

  'src/remoting/host/installer/linux/internal': {
      'url': Var('chrome_git') + '/chrome/remoting/host/installer/linux/internal.git' + '@' +
        'e190816de75b14897f1af785eb37d237750460e2',
      'condition': 'checkout_linux and checkout_src_internal',
  },

  'src/remoting/internal': {
      'url': Var('chrome_git') + '/chrome/remoting/internal.git' + '@' +
        'bc8e5dbe2ea801d77d77ed8400e752f631f352a8',
      'condition': 'checkout_src_internal',
  },

  'src/remoting/test/internal': {
      'url': Var('chrome_git') + '/chrome/remoting/test/internal.git' + '@' +
        '34ff3657e2176fc48a57fad555b076a50a409de6',
      'condition': 'checkout_src_internal',
  },

  'src/remoting/tools/internal': {
      'url': Var('chrome_git') + '/chrome/remoting/tools/internal.git' + '@' +
        'acfed9c3a363694f37aadfb5cda4c31109661eb8',
      'condition': 'checkout_src_internal',
  },

  'src/signing_keys': {
      'url': Var('chrome_git') + '/clank/apptestkey.git' + '@' +
        '5138e684915721cbccbb487ec0764ed05650fcd0',
      'condition': 'checkout_android and checkout_google_internal and checkout_src_internal',
  },

  'src/skia/tools/clusterfuzz-data':{
      'url': Var('chrome_git') + '/chrome/tools/clusterfuzz-data.git' + '@' +
        'fa1fc4acacddd8d655cfca0bcadef5f7e2259bed',
      'condition': 'checkout_clusterfuzz_data and checkout_src_internal',
  },

  'src/third_party/amd': {
      'url': Var('chrome_git') + '/chrome/deps/amd.git' + '@' +
        'cbd9811acb6d09f19b880fdbc6f0fc62901c9a5c',
      'condition': 'checkout_win and checkout_src_internal',
  },

  'src/third_party/android_tools_internal': {
      'url': Var('chrome_git') + '/clank/third_party/android_tools.git' + '@' +
        'ab59dfd133386420a319a194c9ac6f5cae802471',
      'condition': 'checkout_android and checkout_src_internal',
  },

  'src/third_party/libassistant/x64': {
    'packages': [
        {
            'package': 'chromeos_internal/assistant/libassistant/libassistant_cros_device/x86_64/internal',
            'version': 'CmuG4T_84hgqYyVPLppHO_cYpXPoYIFpJNT5wC5iGZgC',
        },
    ],
    'condition': 'checkout_src_internal and checkout_chromeos',
    'dep_type': 'cipd',
  },

 'src/third_party/libassistant/arm64': {
    'packages': [
        {
            'package': 'chromeos_internal/assistant/libassistant/libassistant_cros_device/arm64/internal',
            'version': '0gwaQvw-4Jne1dvCdVsGRVHcADdvSLBy172ar-FFyIoC',
        },
    ],
    'condition': 'checkout_src_internal and checkout_chromeos',
    'dep_type': 'cipd',
  },

 'src/third_party/libassistant/arm': {
    'packages': [
        {
            'package': 'chromeos_internal/assistant/libassistant/libassistant_cros_device/arm/internal',
            'version': 'bLW45XE7O8kCndjxEYtqdupr0tV4mgB7xKkcETBOsskC',
        },
    ],
    'condition': 'checkout_src_internal and checkout_chromeos',
    'dep_type': 'cipd',
  },

 'src/third_party/libassistant/glinux': {
    'packages': [
        {
            'package': 'chromeos_internal/assistant/libassistant/libassistant_cros_glinux/x64/internal',
            'version': '3Opw5sw239P8B6hdZCofFV_16gGsW5nJbuoA93doiAkC',
        },
    ],
    'condition': 'checkout_src_internal and checkout_chromeos',
    'dep_type': 'cipd',
  },

 'src/third_party/libassistant/fake_s3_server': {
    'packages': [
        {
            'package': 'chromeos_internal/assistant/libassistant/fake_s3_server_cros_glinux/x64/internal',
            'version': '7BVkGvAvW0XDvbHj3P4-e6TAxssx6_PC2L0eQBLWyP8C',
        },
    ],
    'condition': 'checkout_src_internal and checkout_chromeos',
    'dep_type': 'cipd',
  },

  'src/third_party/screen-ai/linux': {
      'packages': [
          {
              'package': 'chromium/third_party/screen-ai/linux',
              'version': Var('screen_ai_linux'),
          },
      ],
      'condition': 'checkout_linux and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/screen-ai/macos_amd64': {
      'packages': [
          {
              'package': 'chromium/third_party/screen-ai/mac-amd64',
              'version': Var('screen_ai_macos_amd64'),
          },
      ],
      'condition': 'checkout_mac',
      'dep_type': 'cipd',
  },

  'src/third_party/screen-ai/macos_arm64': {
      'packages': [
          {
              'package': 'chromium/third_party/screen-ai/mac-arm64',
              'version': Var('screen_ai_macos_arm64'),
          },
      ],
      'condition': 'checkout_mac',
      'dep_type': 'cipd',
  },

  'src/third_party/screen-ai/windows_amd64': {
      'packages': [
          {
              'package': 'chromium/third_party/screen-ai/windows-amd64',
              'version': Var('screen_ai_windows_amd64'),
          },
      ],
      'condition': 'checkout_win',
      'dep_type': 'cipd',
  },

  'src/third_party/screen-ai/windows_386': {
      'packages': [
          {
              'package': 'chromium/third_party/screen-ai/windows-386',
              'version': Var('screen_ai_windows_386'),
          },
      ],
      'condition': 'checkout_win',
      'dep_type': 'cipd',
  },

  'src/third_party/soda': {
      'packages': [
          {
              'package': 'chrome_internal/third_party/soda',
              'version': 'StdK8khsivYZXVo2wZuVMnDN_xrVO2a8HV8kvfJ3emwC',
          },
      ],
      'condition': 'checkout_linux and checkout_soda and checkout_src_internal and non_git_source',
      'dep_type': 'cipd',
  },

  'src/third_party/soda-mac64': {
      'packages': [
          {
              'package': 'chrome_internal/third_party/soda-mac64',
              'version': 'bJ-qwdYVguWT0V24YjNZ7Nw_toipv0YnVuadeX5xozEC',
          },
      ],
      'condition': 'checkout_mac and checkout_soda and checkout_src_internal',
      'dep_type': 'cipd',
  },

  'src/third_party/soda-win32': {
      'packages': [
          {
              'package': 'chrome_internal/third_party/soda-win32',
              'version': '977qxBGTKbe2kY9aQch9OkNJ3YE8Nt2mVjetdpWeM0IC',
          },
      ],
      'condition': 'checkout_win and checkout_soda and checkout_src_internal',
      'dep_type': 'cipd',
  },

  'src/third_party/soda-win64': {
      'packages': [
          {
              'package': 'chrome_internal/third_party/soda-win64',
              'version': '1elz1jfCAzy5tZUNBr8FsovjgFxmtu8jdyA8ay9Ta8UC',
          },
      ],
      'condition': 'checkout_win and checkout_soda and checkout_src_internal',
      'dep_type': 'cipd',
    },

  'src/third_party/widevine/cdm/chromeos': {
      'url': Var('chrome_git') + '/chrome/deps/widevine/cdm/chromeos.git' + '@' +
        'b3c0f132204e9732422075787138ce2cb60faa4a',
      'condition': '(checkout_chromeos or checkout_linux) and checkout_src_internal',
  },

  'src/third_party/widevine/cdm/linux': {
      'url': Var('chrome_git') + '/chrome/deps/widevine/cdm/linux.git' + '@' +
        'a7112e9c9e97711f5f8ec5d2ec2ad21fd5ab88e0',
      'condition': 'checkout_linux and checkout_src_internal',
  },

  'src/third_party/widevine/cdm/mac': {
      'url': Var('chrome_git') + '/chrome/deps/widevine/cdm/mac.git' + '@' +
        '2d33c009716adffeab29445a1d237bf54d0de6eb',
      'condition': 'checkout_mac and checkout_src_internal',
    },

  'src/third_party/widevine/cdm/win': {
      'url': Var('chrome_git') + '/chrome/deps/widevine/cdm/win.git' + '@' +
        '0d0174461e77947dc68bd96016cbccf41c9754c6',
      'condition': 'checkout_win and checkout_src_internal',
  },

  'src/third_party/widevine/scripts': {
      'url': Var('chrome_git') + '/chrome/deps/widevine/scripts.git' + '@' +
        '6ae793a606aeed0d0f1c6f688117653710137744',
      'condition': 'checkout_src_internal',
  },

  # Only Linux test license server is available.
  'src/third_party/widevine/test/license_server': {
      'url': Var('chrome_git') + '/chrome/deps/widevine/test/license_server.git' + '@' +
        '8b195ed15e73e2ecc9861afb05e6af0b4bdf7413',
      'condition': 'checkout_linux and checkout_src_internal',
  },

  'src/third_party/wix': {
      'url': Var('chrome_git') + '/chrome/deps/wix/v3_5_2519.git' + '@' +
        '1cda03778b09bee24389da73daef3de862da37fc',
      'condition': 'checkout_win and checkout_src_internal',
  },

  'src/tools/perf/data': {
      'url': Var('chrome_git') + '/chrome/tools/perf/data.git' + '@' +
        'c7eaf497f690ee69e832b1530e19877602e65b18',
      'condition': 'checkout_src_internal',
  },

  'src/ui/file_manager/internal': {
      'url': Var('chrome_git') + '/chrome/file_manager.git' + '@' +
        'a84801be1d5ef906cc03db7eeadd25ce0245ce44',
      'condition': '(checkout_chromeos or checkout_linux) and checkout_src_internal',
  },

  'src/ui/webui/internal': {
      'url': Var('chrome_git') + '/chrome/ui-webui-internal.git' + '@' +
        '4afc450a9363ab44f45c3639d0661daa7dbe5bda',
      'condition': 'checkout_chromeos and checkout_src_internal',
  },

  'src/webkit/data/bmp_decoder': {
      'url': Var('chrome_git') + '/chrome/data/bmp_decoder.git' + '@' +
        '5a3232a478b8afd0e8403fb8c668baf8c9e25ea3',
      'condition': 'checkout_src_internal',
  },

  'src/webkit/data/ico_decoder': {
      'url': Var('chrome_git') + '/chrome/data/ico_decoder.git' + '@' +
        'aba38604e037bdbeedca9c2780c94502a8a6034d',
      'condition': 'checkout_src_internal',
  },

  'src/webkit/data/test_shell/plugins': {
      'url': Var('chrome_git') + '/chrome/data/webkit_plugin_tests.git' + '@' +
        'e4bd19f95afa6483a54906c2a3e5d329d2d81690',
      'condition': 'checkout_src_internal',
  },
}


include_rules = [
  # Everybody can use some things.
  # NOTE: THIS HAS TO STAY IN SYNC WITH third_party/DEPS which disallows these.
  '+base',
  '+build',
  '+ipc',
  # perfetto is base's public dependency.
  '+third_party/perfetto/include/perfetto/tracing',
  '+third_party/perfetto/include/perfetto/test',

  # PartitionAlloc is located at `base/allocator/partition_allocator` but
  # prefers its own include path:
  # `#include "partition_alloc/..."` is prefered to
  # `#include "base/allocator/partition_allocator/src/partition_alloc/..."`.
  "+partition_alloc",
  "-base/allocator/partition_allocator",

  # Everybody can use headers generated by tools/generate_library_loader.
  '+library_loaders',

  '+testing',
  '+third_party/google_benchmark/src/include/benchmark/benchmark.h',
  '+third_party/icu/source/common/unicode',
  '+third_party/icu/source/i18n/unicode',
  '+url',

  # Abseil is allowed by default, but some features are banned. See
  # //styleguide/c++/c++-features.md.
  '+third_party/abseil-cpp',
  '-third_party/abseil-cpp/absl/algorithm/container.h',
  '-third_party/abseil-cpp/absl/base/attributes.h',
  '-third_party/abseil-cpp/absl/base/no_destructor.h',
  '-third_party/abseil-cpp/absl/base/nullability.h',
  '-third_party/abseil-cpp/absl/container',
  '+third_party/abseil-cpp/absl/container/inlined_vector.h',
  '-third_party/abseil-cpp/absl/crc',
  '-third_party/abseil-cpp/absl/flags',
  '-third_party/abseil-cpp/absl/functional/any_invocable.h',
  '-third_party/abseil-cpp/absl/functional/bind_front.h',
  '-third_party/abseil-cpp/absl/functional/function_ref.h',
  '-third_party/abseil-cpp/absl/functional/overload.h',
  '-third_party/abseil-cpp/absl/hash',
  '-third_party/abseil-cpp/absl/log',
  '-third_party/abseil-cpp/absl/random',
  '-third_party/abseil-cpp/absl/status/statusor.h',
  '-third_party/abseil-cpp/absl/strings',
  '+third_party/abseil-cpp/absl/strings/ascii.h',
  '+third_party/abseil-cpp/absl/strings/cord.h',
  '+third_party/abseil-cpp/absl/strings/str_format.h',
  '-third_party/abseil-cpp/absl/synchronization',
  '-third_party/abseil-cpp/absl/time',
  '-third_party/abseil-cpp/absl/types/any.h',
  '-third_party/abseil-cpp/absl/types/optional.h',
  '-third_party/abseil-cpp/absl/types/span.h',
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
  'native_client_sdk',
  'out',
  'skia',
  'testing',
  'third_party/abseil-cpp',
  'v8',
]


hooks = [
  # Download and initialize "vpython" VirtualEnv environment packages for
  # Python3. We do this before running any other hooks so that any other
  # hooks that might use vpython don't trip over unexpected issues and
  # don't run slower than they might otherwise need to.
  {
    'name': 'vpython3_common',
    'pattern': '.',
    'action': [ 'vpython3',
                '-vpython-spec', 'src/.vpython3',
                '-vpython-tool', 'install',
    ],
  },
  {
    # This clobbers when necessary (based on get_landmines.py). This should
    # run as early as possible so that other things that get/generate into the
    # output directory will not subsequently be clobbered.
    'name': 'landmines',
    'pattern': '.',
    'action': [
        'python3',
        'src/build/landmines.py',
    ],
  },
  {
    # This clobbers when necessary (based on the internal ios version of
    # get_landmines.py). This should run as early as possible so that
    # other things that get/generate into the output directory will not
    # subsequently be clobbered. This script is only run# for iOS build
    # with src_internal.
    'name': 'landmines_ios_internal',
    'pattern': '.',
    'condition': 'checkout_ios and checkout_src_internal',
    'action': [
        'python3',
        'src/build/landmines.py',
        '--landmine-scripts',
        'src/ios_internal/build/get_landmines.py',
        '--landmines-path',
        'src/ios_internal/.landmines',
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
    # Ensure we remove any file from disk that is no longer needed (e.g. after
    # hooks to native GCS deps migration).
    'name': 'remove_stale_files',
    'pattern': '.',
    'action': [
        'python3',
        'src/tools/remove_stale_files.py',
        'src/third_party/test_fonts/test_fonts.tar.gz', # Remove after 20240901
        'src/third_party/node/node_modules.tar.gz', # TODO: Remove after 20241201, see https://crbug.com/351092787
        'src/third_party/tfhub_models', # TODO: Remove after 20241211
    ],
  },
  {
    # Ensure that we don't accidentally reference any .pyc files whose
    # corresponding .py files have since been deleted.
    # We could actually try to avoid generating .pyc files, crbug.com/500078.
    'name': 'remove_stale_pyc_files',
    'pattern': '.',
    'action': [
        'python3',
        'src/tools/remove_stale_pyc_files.py',
        'src/android_webview/tools',
        'src/build/android',
        'src/gpu/gles2_conform_support',
        'src/infra',
        'src/ppapi',
        'src/printing',
        'src/third_party/blink/renderer/build/scripts',
        'src/third_party/blink/tools',  # See http://crbug.com/625877.
        'src/third_party/catapult',
        'src/third_party/mako', # Some failures triggered by crrev.com/c/3686969
        'src/tools',
    ],
  },
  {
    # This downloads binaries for Native Client's newlib toolchain.
    # Done in lieu of building the toolchain from scratch as it can take
    # anywhere from 30 minutes to 4 hours depending on platform to build.
    'name': 'nacltools',
    'pattern': '.',
    'condition': 'checkout_nacl',
    'action': [
        'python3',
        'src/build/download_nacl_toolchains.py',
        '--mode', 'nacl_core_sdk',
        'sync', '--extract',
    ],
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
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', 'src/build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac or checkout_ios',
    'action': ['python3', 'src/build/mac_toolchain.py'],
  },
  {
    # Build the clang toolchain from tip-of-tree.
    # Note: On Win, this should run after win_toolchain, as it may use it.
    'name': 'clang_tot',
    'pattern': '.',
    'condition': 'llvm_force_head_revision',
    'action': ['vpython3', 'src/tools/clang/scripts/build.py',
               '--llvm-force-head-revision',
               '--with-android={checkout_android}',
               '--with-fuchsia={checkout_fuchsia}'],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python3', 'src/build/util/lastchange.py',
               '-o', 'src/build/util/LASTCHANGE',
               # Cobalt addition, don't look for Change-Id in commits.
               '--filter=\(#[1-9][0-9]*\)'
               ],
  },
  {
    # Update GPU lists version string (for gpu/config).
    'name': 'gpu_lists_version',
    'pattern': '.',
    'action': ['python3', 'src/build/util/lastchange.py',
               '-m', 'GPU_LISTS_VERSION',
               '--revision-id-only',
               '--header', 'src/gpu/config/gpu_lists_version.h'],
  },
  {
    # Update skia_commit_hash.h.
    'name': 'lastchange_skia',
    'pattern': '.',
    'action': ['python3', 'src/build/util/lastchange.py',
               '-m', 'SKIA_COMMIT_HASH',
               '-s', 'src/third_party/skia',
               '--header', 'src/skia/ext/skia_commit_hash.h'],
  },
  {
    # Update dawn_version.h.
    'name': 'lastchange_dawn',
    'pattern': '.',
    'action': ['python3', 'src/build/util/lastchange.py',
               '-s', 'src/third_party/dawn',
               '--revision', 'src/gpu/webgpu/DAWN_VERSION'],
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
    ]
  },
  {
    'name': 'apache_win32',
    'pattern': '\\.sha1',
    'condition': 'checkout_win',
    'action': [ 'python3',
                'src/third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--directory',
                '--recursive',
                '--no_auth',
                '--num_threads=16',
                '--bucket', 'chromium-apache-win32',
                'src/third_party/apache-win32',
    ],
  },

  # Download Telemetry's binary dependencies via conditionals
  {
    'name': 'checkout_telemetry_binary_dependencies',
    'condition': 'checkout_telemetry_dependencies',
    'pattern': '.',
    'action': [ 'vpython3',
                'src/third_party/catapult/telemetry/bin/fetch_telemetry_binary_dependencies',
    ],
  },

  # Download Telemetry's benchmark binary dependencies via conditionals
  {
    'name': 'checkout_telemetry_benchmark_deps',
    'condition': 'checkout_telemetry_dependencies and checkout_linux and not checkout_android and not skip_wpr_archives_download',
    'pattern': '.',
    'action': [ 'vpython3',
                'src/tools/perf/fetch_benchmark_deps.py',
                '-f',
                '-p',
                'linux'
    ],
  },
  {
    'name': 'checkout_telemetry_benchmark_deps',
    'condition': 'checkout_telemetry_dependencies and checkout_win and not skip_wpr_archives_download',
    'pattern': '.',
    'action': [ 'vpython3',
                'src/tools/perf/fetch_benchmark_deps.py',
                '-f',
                '-p',
                'win'
    ],
  },
  {
    'name': 'checkout_telemetry_benchmark_deps',
    'condition': 'checkout_telemetry_dependencies and checkout_mac and not skip_wpr_archives_download',
    'pattern': '.',
    'action': [ 'vpython3',
                'src/tools/perf/fetch_benchmark_deps.py',
                '-f',
                '-p',
                'mac'
    ],
  },
  {
    'name': 'checkout_telemetry_benchmark_deps',
    'condition': 'checkout_telemetry_dependencies and checkout_android and not skip_wpr_archives_download',
    'pattern': '.',
    'action': [ 'vpython3',
                'src/tools/perf/fetch_benchmark_deps.py',
                '-f',
                '-p',
                'android'
    ],
  },

  # Pull down WPR Archive files
  {
    'name': 'Fetch WPR archive files',
    'pattern': '.',
    'condition': 'checkout_android and checkout_wpr_archives',
    'action': [ 'python3',
                'src/chrome/test/data/android/manage_wpr_archives.py',
                'download',
    ],
  },
  # Download only WPR binary dependencies from Telemetry via conditionals
  {
    'name': 'checkout_wpr_binary_dependencies',
    'condition': 'checkout_chromium_autofill_test_dependencies or checkout_chromium_password_manager_test_dependencies',
    'pattern': '.',
    'action': [ 'vpython3',
                'src/third_party/catapult/telemetry/bin/fetch_wpr_binary_dependencies',
    ],
  },
  {
    'name': 'Fetch Android AFDO profile',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python3',
                'src/tools/download_optimization_profile.py',
                '--newest_state=src/chrome/android/profiles/newest.txt',
                '--local_state=src/chrome/android/profiles/local.txt',
                '--output_name=src/chrome/android/profiles/afdo.prof',
                '--gs_url_base=chromeos-prebuilt/afdo-job/llvm',
    ],
  },
  {
    'name': 'Fetch Android Arm AFDO profile',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python3',
                'src/tools/download_optimization_profile.py',
                '--newest_state=src/chrome/android/profiles/arm.newest.txt',
                '--local_state=src/chrome/android/profiles/arm.local.txt',
                '--output_name=src/chrome/android/profiles/arm.afdo.prof',
                '--gs_url_base=chromeos-prebuilt/afdo-job/llvm',
    ],
  },
  # DOWNLOAD AR test APKs only if the environment variable is set
  {
    'name': 'ar_test_apks',
    'pattern': '.',
    'condition': 'checkout_android',
    'action': [ 'python3',
                'src/third_party/arcore-android-sdk/test-apks/update.py',
    ],
  },
  # Download AFDO profiles for ChromeOS for each architecture.
  {
    'name': 'Fetch ChromeOS AFDO profiles (from Intel Atom cores)',
    'pattern': '.',
    'condition': 'checkout_chromeos or checkout_simplechrome',
    'action': [ 'python3',
                'src/tools/download_optimization_profile.py',
                '--newest_state=src/chromeos/profiles/atom.afdo.newest.txt',
                '--local_state=src/chromeos/profiles/atom.afdo.local.txt',
                '--output_name=src/chromeos/profiles/atom.afdo.prof',
                '--gs_url_base=chromeos-prebuilt/afdo-job/vetted/release',
    ],
  },
  {
    'name': 'Fetch ChromeOS AFDO profiles (from Intel Big cores)',
    'pattern': '.',
    'condition': 'checkout_chromeos or checkout_simplechrome',
    'action': [ 'python3',
                'src/tools/download_optimization_profile.py',
                '--newest_state=src/chromeos/profiles/bigcore.afdo.newest.txt',
                '--local_state=src/chromeos/profiles/bigcore.afdo.local.txt',
                '--output_name=src/chromeos/profiles/bigcore.afdo.prof',
                '--gs_url_base=chromeos-prebuilt/afdo-job/vetted/release',
    ],
  },
  {
    'name': 'Fetch ChromeOS AFDO profiles (from Arm)',
    'pattern': '.',
    'condition': 'checkout_chromeos or checkout_simplechrome',
    'action': [ 'python3',
                'src/tools/download_optimization_profile.py',
                '--newest_state=src/chromeos/profiles/arm.afdo.newest.txt',
                '--local_state=src/chromeos/profiles/arm.afdo.local.txt',
                '--output_name=src/chromeos/profiles/arm.afdo.prof',
                '--gs_url_base=chromeos-prebuilt/afdo-job/vetted/release',
    ],
  },
  {
    'name': 'Download Fuchsia SDK',
    'pattern': '.',
    'condition': 'checkout_fuchsia and not checkout_fuchsia_no_hooks',
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
    'name': 'Download Fuchsia internal system images',
    'pattern': '.',
    'condition': 'checkout_fuchsia_internal and checkout_src_internal',
    'action': ['python3',
               'src/build/fuchsia/update_product_bundles.py',
               '{checkout_fuchsia_internal_images}',
               '--internal'],
  },

  {
    'name': 'Create Fuchsia GN sdk build rules when not running update_sdk.py',
    'pattern': '.',
    'condition': 'checkout_fuchsia_no_hooks',
    'action': [
      'python3',
      'src/build/fuchsia/gen_build_defs.py',
    ],
  },

  {
    'name': 'cros_simplechrome_artifacts_with_vm',
    'pattern': '.',
    'condition': 'checkout_simplechrome_with_vms and not checkout_src_internal',
    'action': [
      'vpython3',
      'src/third_party/chromite/bin/cros',
      'chrome-sdk',
      '--fallback-versions=20',
      '--no-use-remoteexec',
      '--nogn-gen',
      '--no-shell',
      '--log-level=warning',
      '--cache-dir=src/build/cros_cache/',
      '--use-external-config',
      '--boards={cros_boards_with_qemu_images}',
      '--download-vm',
    ],
  },
  {
    'name': 'cros_simplechrome_artifacts_with_no_vm',
    'pattern': '.',
    'condition': 'checkout_simplechrome and not checkout_src_internal',
    'action': [
      'vpython3',
      'src/third_party/chromite/bin/cros',
      'chrome-sdk',
      '--fallback-versions=20',
      '--no-use-remoteexec',
      '--nogn-gen',
      '--no-shell',
      '--log-level=warning',
      '--cache-dir=src/build/cros_cache/',
      '--use-external-config',
      '--boards={cros_boards}',
    ],
  },
  {
    'name': 'cros_simplechrome_artifacts_with_no_vm_internal',
    'pattern': '.',
    'condition': 'checkout_simplechrome and checkout_src_internal',
    'action': [
      'vpython3',
      'src/third_party/chromite/bin/cros',
      'chrome-sdk',
      '--fallback-versions=20',
      '--no-use-remoteexec',
      '--nogn-gen',
      '--no-shell',
      '--log-level=warning',
      '--cache-dir=src/build/cros_cache/',
      '--boards={cros_boards}',
    ],
  },
  {
    'name': 'cros_simplechrome_artifacts_with_vm_internal',
    'pattern': '.',
    'condition': 'checkout_simplechrome_with_vms and checkout_src_internal',
    'action': [
      'vpython3',
      'src/third_party/chromite/bin/cros',
      'chrome-sdk',
      '--fallback-versions=20',
      '--no-use-remoteexec',
      '--nogn-gen',
      '--no-shell',
      '--log-level=warning',
      '--cache-dir=src/build/cros_cache/',
      '--boards={cros_boards_with_qemu_images}',
      '--download-vm',
    ],
  },

  # Download PGO profiles.
  {
    'name': 'Fetch PGO profiles for win-arm64',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_win',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=win-arm64',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for win32',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_win',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=win32',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for win64',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_win',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=win64',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for mac',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and (checkout_mac or checkout_fuchsia)',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=mac',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for mac arm',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and (checkout_mac or checkout_android or checkout_fuchsia or checkout_ios)',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=mac-arm',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for linux',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_linux',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=linux',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for android arm32',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_android',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=android-arm32',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for android arm64',
    'pattern': '.',
    'condition': 'checkout_pgo_profiles and checkout_android',
    'action': [ 'python3',
                'src/tools/update_pgo_profiles.py',
                '--target=android-arm64',
                'update',
                '--gs-url-base=chromium-optimization-profiles/pgo_profiles',
    ],
  },
  {
    'name': 'Fetch PGO profiles for V8 builtins',
    'pattern': '.',
    # Always download profiles on Android builds. The GN arg `is_official_build`
    # is required to consider the profiles during build time.
    'condition': 'checkout_pgo_profiles or checkout_android',
    'action': [ 'python3',
                'src/v8/tools/builtins-pgo/download_profiles.py',
                'download',
                '--depot-tools',
                'src/third_party/depot_tools',
                '--check-v8-revision',
                '--quiet',
    ],
  },

  # Download Cast3p Binaries
  {
    'name': 'cast3p_binaries',
    'pattern': '.',
    'action': [
      'python3',
      'src/tools/cast3p/update_binaries.py',
    ],
    'condition': 'checkout_cast3p',
  },

  {
    'name': 'Generate location tags for tests',
    'pattern': '.',
    'action': [
      'python3',
      'src/testing/generate_location_tags.py',
      '--out',
      'src/testing/location_tags.json',
    ],
    'condition': 'generate_location_tags',
  },

  {
    # Clean up build dirs for crbug.com/1337238.
    # After a libc++ roll and revert, .ninja_deps would get into a state
    # that breaks Ninja on Windows.
    # TODO(crbug.com/1409337): Remove this after updating Ninja 1.12 or newer.
    'name': 'del_ninja_deps_cache',
    'pattern': '.',
    'condition': 'host_os == "win"',
    'action': ['python3', 'src/build/del_ninja_deps_cache.py'],
  },
  # Configure remote exec cfg files
  {
    # Use luci_auth if on windows and using chrome-untrusted project
    'name': 'download_and_configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and host_os == "win"',
    'action': ['python3',
               'src/buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--use_luci_auth_credshelper',
               '--quiet',
               ],
  },  {
    'name': 'download_and_configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and not host_os == "win"',
    'action': ['python3',
               'src/buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--quiet',
               ],
  },
  {
    'name': 'configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'not download_remoteexec_cfg',
    'action': ['python3',
               'src/buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--skip_remoteexec_cfg_fetch',
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
    'name': 'libaom_testdata',
    'pattern': '.',
    'condition': 'download_libaom_testdata',
    'action': ['python3',
               'src/third_party/depot_tools/gsutil.py',
               '-q',
               '-m',
               'rsync',
               'gs://aom-test-data',
               'src/third_party/libaom/testdata']
  },
  {
    'name': 'libvpx_testdata',
    'pattern': '.',
    'condition': 'download_libvpx_testdata',
    'action': ['python3',
               'src/third_party/depot_tools/gsutil.py',
               '-q',
               '-m',
               'rsync',
               'gs://downloads.webmproject.org/test_data/libvpx',
               'src/third_party/libvpx/testdata'],
  },
]

# Add any corresponding DEPS files from this list to chromium.exclusions in
# //testing/buildbot/trybot_analyze_config.json
# ctx: https://crbug.com/1201994
recursedeps = [
  # ANGLE manages DEPS that it also owns the build files for, such as dEQP.
  'src/third_party/angle',
  # Dawn manages DEPS for its copy of the WebGPU CTS as well as GLFW for which
  # it has build files.
  'src/third_party/dawn',
  'src/third_party/devtools-frontend-internal',
  'src/third_party/instrumented_libs',
  'src/third_party/openscreen/src',
  'src/third_party/devtools-frontend/src',
  # clank has its own DEPS file, does not need to be in trybot_analyze_config
  # since the roller does not run tests.
  'src/clank',
  'src/components/optimization_guide/internal',
  'src/ios_internal',
]
