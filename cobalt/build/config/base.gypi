# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This file should be included in all .gyp files used by Cobalt. Normally,
# this should be done automatically by gyp_cobalt.
{
  'variables': {
    # Cobalt variables.

    # Whether Cobalt is being built.
    'cobalt': 1,

    # Similarly to chromium_code, marks the projects that are created by the
    # Cobalt team and thus are held to the highest standards of code health.
    'cobalt_code%': 0,

    # Contains the current build configuration.
    'cobalt_config%': 'gold',
    'cobalt_fastbuild%': 0,
    # Build version number.
    'cobalt_version%': 0,
    # Contains the name of the hosting OS. The value is defined by gyp_cobalt.
    'host_os%': 'win',
    # The "real" target_arch that is used to select the correct delegate source.
    # TODO: Investigate why adding % will break the build on platforms
    # other than Windows
    # TODO: Remove after starboard.
    'actual_target_arch': '<(target_arch)',

    # The target platform id as a string, like 'ps3', 'ps4', etc..
    'sb_target_platform': '',

    # The operating system of the target, separate from the target_arch. In many
    # cases, an 'unknown' value is fine, but, if set to 'linux', then we can
    # assume some things, and it'll save us some configuration time.
    'target_os%': 'unknown',

    # Defines what kind of rasterizer will be used.  This can be adjusted to
    # force a stub graphics implementation or software graphics implementation.
    # It can be one of the following options:
    #   'hardware' -- As much hardware acceleration of graphics commands as
    #                 possible. Required for 360 rendering.
    #   'software' -- Perform most rasterization using the CPU and only interact
    #                 with the GPU to send the final image to the output window.
    #   'stub'     -- Stub graphics rasterization.  A rasterizer object will
    #                 still be available and valid, but it will do nothing.
    'rasterizer_type%': 'hardware',

    # Modify this value to adjust the default rasterizer setting for your
    # platform.
    'default_renderer_options_dependency%': '<(DEPTH)/cobalt/renderer/default_options_starboard.gyp:default_options',

    # The variables allow changing the target type on platforms where the
    # native code may require an additional packaging step (ex. Android).
    'gtest_target_type%': 'executable',
    'final_executable_type%': 'executable',
    'posix_emulation_target_type%': 'static_library',
    'webkit_target_type%': 'static_library',

    # Set to 1 to build with DIAL support.
    'in_app_dial%': 0,

    # Set to 1 to enable H5vccAccountManager.
    'enable_account_manager%': 0,

    # Set to 1 to compile with SPDY support.
    'enable_spdy%': 0,

    # Halt execution on failure to allocate memory.
    'abort_on_allocation_failure%': 1,

    # Used by cobalt/media/media.gyp to pick a proper media platform.
    'sb_media_platform%': 'starboard',

    'sb_has_deploy_step%': 0,

    # Needed for backwards compatibility with lbshell code.
    'lbshell_root%': '<(DEPTH)/lbshell',

    # The relative path from src/ to the directory containing the
    # starboard_platform.gyp file, or the empty string if not an autodiscovered
    # platform.
    'starboard_path%': '',

    # The source of EGL and GLES headers and libraries.
    # Valid values (case and everything sensitive!):
    #   'none'   - No EGL + GLES implementation is available on this platform.
    #   'system_gles3' - Use the system implementation of EGL + GLES3. The
    #                    headers and libraries must be on the system include and
    #                    link paths.
    #   'system_gles2' - Use the system implementation of EGL + GLES2. The
    #                    headers and libraries must be on the system include and
    #                    link paths.
    #   'glimp'  - Cobalt's own EGL + GLES2 implementation. This requires a
    #              valid Glimp implementation for the platform.
    #   'angle'  - A DirectX-to-OpenGL adaptation layer. This requires a valid
    #              ANGLE implementation for the platform.
    # Choosing an unsupported value will result in a GYP error:
    #   "cobalt/renderer/egl_and_gles/egl_and_gles_<gl_type>.gyp not found"
    'gl_type%': 'system_gles2',

    # Cache parameters

    # The following set of parameters define how much memory is reserved for
    # different Cobalt caches.  These caches affect CPU *and* GPU memory usage.
    #
    # The sum of the following caches effectively describes the maximum GPU
    # texture memory usage (though it doesn't consider video textures and
    # display color buffers):
    #   - skia_cache_size_in_bytes (GLES2 rasterizer only)
    #   - scratch_surface_cache_size_in_bytes
    #   - surface_cache_size_in_bytes
    #   - image_cache_size_in_bytes
    #
    # The other caches affect CPU memory usage.

    # Determines the capacity of the skia cache.  The Skia cache is maintained
    # within Skia and is used to cache the results of complicated effects such
    # as shadows, so that Skia draw calls that are used repeatedly across
    # frames can be cached into surfaces.  This setting is only relevant when
    # using the hardware-accelerated Skia rasterizer (e.g. as opposed to the
    # Blitter API).
    'skia_cache_size_in_bytes%': 4 * 1024 * 1024,

    # Determines the capacity of the scratch surface cache.  The scratch
    # surface cache facilitates the reuse of temporary offscreen surfaces
    # within a single frame.  This setting is only relevant when using the
    # hardware-accelerated Skia rasterizer.
    'scratch_surface_cache_size_in_bytes%': 7 * 1024 * 1024,

    # Determines the capacity of the surface cache.  The surface cache tracks
    # which render tree nodes are being re-used across frames and stores the
    # nodes that are most CPU-expensive to render into surfaces.
    'surface_cache_size_in_bytes%': 0,

    # Determines the capacity of the image cache which manages image surfaces
    # downloaded from a web page.  While it depends on the platform, often (and
    # ideally) these images are cached within GPU memory.
    'image_cache_size_in_bytes%': 64 * 1024 * 1024,

    # Determines the capacity of the remote typefaces cache which manages all
    # typefaces downloaded from a web page.
    'remote_typeface_cache_size_in_bytes%': 5 * 1024 * 1024,

    # Modifying this value to be non-1.0f will result in the image cache
    # capacity being cleared and then temporarily reduced for the duration that
    # a video is playing.  This can be useful for some platforms if they are
    # particularly constrained for (GPU) memory during video playback.  When
    # playing a video, the image cache is reduced to:
    # image_cache_size_in_bytes *
    #     image_cache_capacity_multiplier_when_playing_video.
    'image_cache_capacity_multiplier_when_playing_video%': '1.0f',

    # Compiler configuration.

    # The following variables are used to specify compiler and linker
    # flags by build configuration. Platform specific .gypi includes and
    # .gyp targets can use them to add additional flags.

    'compiler_flags%': [],
    'linker_flags%': [],

    'compiler_flags_debug%': [],
    'compiler_flags_c_debug%': [],
    'compiler_flags_cc_debug%': [],
    'linker_flags_debug%': [],

    'compiler_flags_devel%': [],
    'compiler_flags_c_devel%': [],
    'compiler_flags_cc_devel%': [],
    'linker_flags_devel%': [],

    'compiler_flags_qa%': [],
    'compiler_flags_c_qa%': [],
    'compiler_flags_cc_qa%': [],
    'linker_flags_qa%': [],

    'compiler_flags_gold%': [],
    'compiler_flags_c_gold%': [],
    'compiler_flags_cc_gold%': [],
    'linker_flags_gold%': [],

    'compiler_flags_host%': [],
    'compiler_flags_c_host%': [],
    'compiler_flags_cc_host%': [],
    'linker_flags_host%': [],

    'platform_libraries%': [],


    # Supported engine is currently only "javascriptcore".
    # TODO: Figure out how to massage gyp the right way to make this work
    # as expected, rather than requiring it to be set for each platform.
    #'javascript_engine%': 'javascriptcore',

    # Enable jit by default. It can be set to 0 to run in interpreter-only mode.
    # Setting this to 1 on a platform or engine for which there is no JIT
    # implementation is a no-op.
    'cobalt_enable_jit%': 1,

    # Customize variables used by Chromium's build/common.gypi.

    # Disable a check that looks for an official google api key.
    'use_official_google_api_keys': 0,
    # Prevents common.gypi from running a bash script which is not required
    # to compile Cobalt.
    'clang_use_chrome_plugins': 0,
    # Disables treat warnings as errors.
    'werror': '',
    # Cobalt doesn't currently support tcmalloc.
    'linux_use_tcmalloc': 0,

    'enable_webdriver%': 0,
  },

  'target_defaults': {
    'variables': {
      # The condition that operates on cobalt_code is in a target_conditions
      # section, and will not have access to the default fallback value of
      # cobalt_code at the top of this file, or to the cobalt_code
      # variable placed at the root variables scope of .gyp files, because
      # those variables are not set at target scope.  As a workaround,
      # if cobalt_code is not set at target scope, define it in target scope
      # to contain whatever value it has during early variable expansion.
      # That's enough to make it available during target conditional
      # processing.
      'cobalt_code%': '<(cobalt_code)',
    },
    'defines': [
      'COBALT',
    ],
    'cflags': [ '<@(compiler_flags)' ],
    'ldflags': [ '<@(linker_flags)' ],
    'cflags_host': [
      '<@(compiler_flags_host)',
      '-D__LB_HOST__'
    ],
    'cflags_c_host': [
      '<@(compiler_flags_c_host)',
    ],
    'cflags_cc_host': [
      '<@(compiler_flags_cc_host)',
    ],
    'ldflags_host': [ '<@(linker_flags_host)' ],

    # Location of Cygwin which is used by the build system when running on a
    # Windows platform.
    'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],

    # Allows any source file to include files relative to the source tree.
    'include_dirs': [ '<(DEPTH)' ],
    'libraries': [ '<@(platform_libraries)' ],

    # TODO: This is needed to support the option to include
    # posix_emulation.h to all compiled source files. This dependency should
    # be refactored and removed.
    'include_dirs_target': [
      '<(DEPTH)/lbshell/src',
    ],
    'conditions': [
      ['posix_emulation_target_type == "shared_library"', {
        'defines': [
          '__LB_BASE_SHARED__=1',
        ],
      }],
      ['webkit_target_type == "shared_library"', {
        'defines': [
          'COBALT_WEBKIT_SHARED=1',
        ],
      }],
      ['OS == "lb_shell"', {
        'defines': [
          '__LB_SHELL__',
        ],
        'include_dirs_target': [
          '<(DEPTH)/lbshell/src/platform/<(target_arch)',
          '<(DEPTH)/lbshell/src/platform/<(target_arch)/posix_emulation/lb_shell',
          # headers that we don't need, but should exist somewhere in the path:
          '<(DEPTH)/lbshell/src/platform/<(target_arch)/posix_emulation/place_holders',
        ],
      }],  # OS == "lb_shell"
      ['OS == "starboard"', {
        # Keeps common Starboard target changes in the starboard project.
        'includes': [
          '../../../starboard/starboard_base_target.gypi',
        ],
      }],  # OS == "starboard"
      ['target_arch in ["xb1", "xb360"]', {
        'defines': ['_USE_MATH_DEFINES'],  # For #define M_PI
      }],
      ['in_app_dial == 1', {
        'defines': [
          'DIAL_SERVER',
        ],
      }],
      ['enable_file_scheme == 1', {
        'defines': [
          'COBALT_ENABLE_FILE_SCHEME',
        ],
      }],
      ['enable_spdy == 0', {
        'defines': [
          'COBALT_DISABLE_SPDY',
        ],
      }],
    ],

    # TODO: Revisit and remove unused configurations.
    'configurations': {
      'debug_base': {
        'abstract': 1,
        # no optimization, include symbols:
        'cflags': [ '<@(compiler_flags_debug)' ],
        'cflags_c': [ '<@(compiler_flags_c_debug)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_debug)' ],
        'ldflags': [ '<@(linker_flags_debug)' ],
        'defines': [
          'ALLOCATOR_STATS_TRACKING',
          'COBALT_BOX_DUMP_ENABLED',
          'COBALT_BUILD_TYPE_DEBUG',
          '_DEBUG',
          'ENABLE_DEBUG_COMMAND_LINE_SWITCHES',
          'ENABLE_DEBUG_C_VAL',
          'ENABLE_DEBUG_CONSOLE',
          'ENABLE_DIR_SOURCE_ROOT_ACCESS',
          'ENABLE_IGNORE_CERTIFICATE_ERRORS',
          'ENABLE_PARTIAL_LAYOUT_CONTROL',
          'ENABLE_TEST_RUNNER',
          '__LB_SHELL__ENABLE_SCREENSHOT__',
          '__LB_SHELL__FORCE_LOGGING__',  # TODO: Rename to COBALT_LOGGING_ENABLED.
          'SK_DEVELOPER',
        ],
      }, # end of debug_base
      'devel_base': {
        'abstract': 1,
        # optimize, include symbols:
        'cflags': [ '<@(compiler_flags_devel)' ],
        'cflags_c': [ '<@(compiler_flags_c_devel)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_devel)' ],
        'ldflags': [ '<@(linker_flags_devel)' ],
        'defines': [
          '_DEBUG',
          'ALLOCATOR_STATS_TRACKING',
          'COBALT_BUILD_TYPE_DEVEL',
          'ENABLE_DEBUG_COMMAND_LINE_SWITCHES',
          'ENABLE_DEBUG_C_VAL',
          'ENABLE_DEBUG_CONSOLE',
          'ENABLE_DIR_SOURCE_ROOT_ACCESS',
          'ENABLE_IGNORE_CERTIFICATE_ERRORS',
          'ENABLE_PARTIAL_LAYOUT_CONTROL',
          'ENABLE_TEST_RUNNER',
          '__LB_SHELL__ENABLE_SCREENSHOT__',
          '__LB_SHELL__FORCE_LOGGING__',
          'SK_DEVELOPER',
        ],
      }, # end of devel_base
      'qa_base': {
        'abstract': 1,
        # optimize:
        'cflags': [ '<@(compiler_flags_qa)' ],
        'cflags_c': [ '<@(compiler_flags_c_qa)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_qa)' ],
        'ldflags': [ '<@(linker_flags_qa)' ],
        'defines': [
          'ALLOCATOR_STATS_TRACKING',
          'COBALT_BUILD_TYPE_QA',
          'ENABLE_DEBUG_COMMAND_LINE_SWITCHES',
          'ENABLE_DEBUG_C_VAL',
          'ENABLE_DEBUG_CONSOLE',
          'ENABLE_DIR_SOURCE_ROOT_ACCESS',
          'ENABLE_IGNORE_CERTIFICATE_ERRORS',
          'ENABLE_PARTIAL_LAYOUT_CONTROL',
          'ENABLE_TEST_RUNNER',
          '__LB_SHELL__ENABLE_SCREENSHOT__',
          '__LB_SHELL__FOR_QA__',
          'NDEBUG',
        ],
      }, # end of devel_base
      'gold_base': {
        'abstract': 1,
        # optimize:
        'cflags': [ '<@(compiler_flags_gold)' ],
        'cflags_c': [ '<@(compiler_flags_c_gold)' ],
        'cflags_cc': [ '<@(compiler_flags_cc_gold)' ],
        'ldflags': [ '<@(linker_flags_gold)' ],
        'defines': [
          'ALLOCATOR_STATS_TRACKING',
          'COBALT_BUILD_TYPE_GOLD',
          'COBALT_FORCE_CSP',
          'COBALT_FORCE_HTTPS',
          '__LB_SHELL__FOR_RELEASE__',
          'NDEBUG',
          'TRACING_DISABLED',
        ],
      }, # end of gold_base
    }, # end of configurations
  }, # end of target_defaults

  # For configurations other than Gold, set the flag that lets test data files
  # be copied and carried along with the build.
  'conditions': [
    ['cobalt_config != "gold"', {
      'variables' : {
        'cobalt_copy_debug_console': 1,
        'cobalt_copy_test_data': 1,
        'enable_about_scheme': 1,
        'enable_file_scheme': 1,
        'enable_network_logging': 1,
        'enable_remote_debugging%': 1,
        'enable_screenshot': 1,
      },
    },
    {
      'variables' : {
        'cobalt_copy_debug_console': 0,
        'cobalt_copy_test_data': 0,
        'enable_about_scheme': 0,
        'enable_file_scheme': 0,
        'enable_network_logging': 0,
        'enable_remote_debugging%': 0,
        'enable_screenshot': 0,
      },
    }],
  ],
}
