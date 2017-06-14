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

    # Enable support for the map to mesh filter, which is primarily used to
    # implement spherical video playback.
    'enable_map_to_mesh%': 0,

    # 'sb_enable_lib' is initially defined inside this inner 'variables' dict so
    # that it can be accessed by 'cobalt_enable_lib' below here.
    'variables': {
      # Enables embedding Cobalt as a shared library within another app. This
      # requires a 'lib' starboard implementation for the corresponding platform.
      'sb_enable_lib%': 0,
    },
    'sb_enable_lib%': '<(sb_enable_lib)',
    'cobalt_enable_lib': '<(sb_enable_lib)',

    # This variable defines what Cobalt's preferred strategy should be for
    # handling internally triggered application exit requests (e.g. the user
    # chooses to back out of the application).
    #   'stop'    -- The application should call SbSystemRequestStop() on exit,
    #                resulting in a complete shutdown of the application.
    #   'suspend' -- The application should call SbSystemRequestSuspend() on
    #                exit, resulting in the application being "minimized".
    #   'noexit'  -- The application should never allow the user to trigger an
    #                exit, this will be managed by the system.
    'cobalt_user_on_exit_strategy': 'stop',

    # Contains the current font package selection.  This can be used to trade
    # font quality, coverage, and latency for different font package sizes.
    # The font package can be one of the following options:
    #   'expanded' -- The largest package. It includes everything in the
    #                 'standard' package, along with 'bold' weight CJK. It is
    #                 recommended that 'local_font_cache_size_in_bytes' be
    #                 increased to 24MB when using this package to account for
    #                 the extra memory required by bold CJK. This package is
    #                 ~48.7MB.
    #   'standard' -- The default package. It includes all sans-serif, serif,
    #                 and FCC fonts, non-CJK fallback fonts in both 'normal' and
    #                 'bold' weights, and 'normal' weight CJK ('bold' weight CJK
    #                 is synthesized from it). This package is ~29.4MB.
    #   'limited_with_jp' -- A significantly smaller package than 'standard'.
    #                 This package removes all but 'normal' and 'bold' weighted
    #                 sans-serif and serif, removes the FCC fonts (which must be
    #                 provided by the system or downloaded from the web),
    #                 removes the 'bold' weighted non-CJK fallback fonts (the
    #                 'normal' weight is still included and is used to
    #                 synthesize bold), and replaces standard CJK with low
    #                 quality CJK. However, higher quality Japanese is still
    #                 included. Because low quality CJK cannot synthesize bold,
    #                 bold glyphs are unavailable in Chinese and Korean. This
    #                 package is ~10.9MB.
    #   'limited'  -- A smaller package than 'limited_with_jp'. The two packages
    #                 are identical with the exception that 'limited' does not
    #                 include the higher quality Japanese font; instead it
    #                 relies on low quality CJK for all CJK characters. Because
    #                 low quality CJK cannot synthesize bold, bold glyphs are
    #                 unavailable in Chinese, Japanese, and Korean. This package
    #                 is ~7.7MB.
    #   'minimal'  -- The smallest possible font package. It only includes
    #                 Roboto's Basic Latin characters. Everything else must be
    #                 provided by the system or downloaded from the web. This
    #                 package is ~16.4KB.
    # NOTE: When bold is needed, but unavailable, it is typically synthesized,
    #       resulting in lower quality glyphs than those generated directly from
    #       a bold font. However, this does not occur with low quality CJK,
    #       which is not high enough quality to synthesize. Its glyphs always
    #       have a 'normal' weight.
    'cobalt_font_package%': 'standard',

    # Font package overrides can be used to modify the files included within the
    # selected package. The following values are available:
    #   -1 -- The package value for the specified category is not overridden.
    #    0 -- The package value is overridden and no fonts for the specified
    #         category are included.
    #    1 -- The package value is overridden and fonts from the specified
    #         category with a weight of 'normal' and a style of 'normal' are
    #         included.
    #    2 -- The package value is overridden and fonts from the specified
    #         category with a weight of either 'normal' or bold' and a style of
    #         'normal' are included.
    #    3 -- The package value is overridden and fonts from the specified
    #         category with a weight of either 'normal' or 'bold' and a style of
    #         either 'normal' or 'italic' are included.
    #    4 -- The package value is overridden and all available fonts from the
    #         specified category are included. This may include additional
    #         weights beyond 'normal' and 'bold'.
    # See content/fonts/README.md for details on the specific values used by
    # each of the packages use for the various font categories.
    'cobalt_font_package_override_named_sans_serif%': -1,
    'cobalt_font_package_override_named_serif%': -1,
    'cobalt_font_package_override_named_fcc_fonts%': -1,
    'cobalt_font_package_override_fallback_lang_non_cjk%': -1,
    'cobalt_font_package_override_fallback_lang_cjk%': -1,
    'cobalt_font_package_override_fallback_lang_cjk_low_quality%': -1,
    'cobalt_font_package_override_fallback_lang_jp%': -1,
    'cobalt_font_package_override_fallback_emoji%': -1,
    'cobalt_font_package_override_fallback_symbols%': -1,

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

    # If set to 1, will enable support for rendering only the regions of the
    # display that are modified due to animations, instead of re-rendering the
    # entire scene each frame.  This feature can reduce startup time where
    # usually there is a small loading spinner animating on the screen.  On GLES
    # renderers, Cobalt will attempt to implement this support by using
    # eglSurfaceAttrib(..., EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED), otherwise
    # the dirty region will be silently disabled.  On Blitter API platforms,
    # if this is enabled, we explicitly create an extra offscreen full-size
    # intermediate surface to render into.  Note that some GLES driver
    # implementations may internally allocate an extra full screen surface to
    # support this feature, and many have been noticed to not properly support
    # this functionality (but they report that they do), and for these reasons
    # this value is defaulted to 0.
    'render_dirty_region_only%': 0,

    # Modify this value to adjust the default rasterizer setting for your
    # platform.
    'default_renderer_options_dependency%': '<(DEPTH)/cobalt/renderer/default_options_starboard.gyp:default_options',

    # Allow throttling of the frame rate. This is expressed in terms of
    # milliseconds and can be a floating point number. Keep in mind that
    # swapping frames may take some additional processing time, so it may be
    # better to specify a lower delay. For example, '33' instead of '33.33'
    # for 30 Hz refresh.
    'cobalt_minimum_frame_time_in_milliseconds%': '16.4',

    # Cobalt will call eglSwapInterval() and specify this value before calling
    # eglSwapBuffers() each frame.
    'cobalt_egl_swap_interval%': 1,

    # The variables allow changing the target type on platforms where the
    # native code may require an additional packaging step (ex. Android).
    'gtest_target_type%': 'executable',
    'final_executable_type%': 'executable',
    'posix_emulation_target_type%': 'static_library',
    'webkit_target_type%': 'static_library',

    # Set to 1 to build with DIAL support.
    'in_app_dial%': 0,

    # Set to 1 to enable a custom MediaSessionClient.
    'custom_media_session_client%': 0,

    # Set to 1 to enable H5vccAccountManager.
    'enable_account_manager%': 0,

    # Set to 1 to enable H5vccCrashLog.
    'enable_crash_log%': 0,

    # Set to 1 to compile with SPDY support.
    'enable_spdy%': 0,

    # Set to 1 to enable filtering of HTTP headers before sending.
    'enable_xhr_header_filtering%': 0,

    # Halt execution on failure to allocate memory.
    'abort_on_allocation_failure%': 1,

    # Used by cobalt/media/media.gyp to pick a proper media platform.
    'sb_media_platform%': 'starboard',

    # Needed for backwards compatibility with lbshell code.
    'lbshell_root%': '<(DEPTH)/lbshell',

    # The relative path from src/ to the directory containing the
    # starboard_platform.gyp file.  It is currently set to
    # 'starboard/<(target_arch)' to make semi-starboard platforms work.
    # TODO: Set the default value to '' once all semi-starboard platforms are
    # moved to starboard.
    'starboard_path%': 'starboard/<(target_arch)',

    # List of platform-specific targets that get compiled into cobalt.
    'cobalt_platform_dependencies%': [],

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

    # Temporary indicator for Tizen - should eventually move to feature defines.
    'tizen_os%': 0,

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
    #   - skia_glyph_atlas_width * skia_glyph_atlas_height
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
    'scratch_surface_cache_size_in_bytes%': 0,

    # Determines the capacity of the surface cache.  The surface cache tracks
    # which render tree nodes are being re-used across frames and stores the
    # nodes that are most CPU-expensive to render into surfaces.
    'surface_cache_size_in_bytes%': 0,

    # Determines the capacity of the image cache, which manages image surfaces
    # downloaded from a web page.  While it depends on the platform, often (and
    # ideally) these images are cached within GPU memory.
    # Set to -1 to automatically calculate the value at runtime, based on
    # features like windows dimensions and the value of
    # SbSystemGetTotalGPUMemory().
    'image_cache_size_in_bytes%': -1,

    # Determines the capacity of the local font cache, which manages all fonts
    # loaded from local files. Newly encountered sections of font files are
    # lazily loaded into the cache, enabling subsequent requests to the same
    # file sections to be handled via direct memory access. Once the limit is
    # reached, further requests are handled via file stream.
    # Setting the value to 0 disables memory caching and causes all font file
    # accesses to be done using file streams.
    'local_font_cache_size_in_bytes%': 16 * 1024 * 1024,

    # Determines the capacity of the remote font cache, which manages all
    # fonts downloaded from a web page.
    'remote_font_cache_size_in_bytes%': 4 * 1024 * 1024,

    # Determines the capacity of the mesh cache. Each mesh is held compressed
    # in main memory, to be inflated into a GPU buffer when needed for
    # projection. When set to 'auto', will be adjusted according to whether
    # the enable_map_to_mesh is true or not.  If enable_map_to_mesh is false,
    # then the mesh cache size will be set to 0.
    'mesh_cache_size_in_bytes%': 'auto',

    # Only relevant if you are using the Blitter API.
    # Determines the capacity of the software surface cache, which is used to
    # cache all surfaces that are rendered via a software rasterizer to avoid
    # re-rendering them.
    'software_surface_cache_size_in_bytes%': 8 * 1024 * 1024,

    # Modifying this value to be non-1.0f will result in the image cache
    # capacity being cleared and then temporarily reduced for the duration that
    # a video is playing.  This can be useful for some platforms if they are
    # particularly constrained for (GPU) memory during video playback.  When
    # playing a video, the image cache is reduced to:
    # image_cache_size_in_bytes *
    #     image_cache_capacity_multiplier_when_playing_video.
    'image_cache_capacity_multiplier_when_playing_video%': '1.0f',

    # Determines the size in pixels of the glyph atlas where rendered glyphs are
    # cached. The resulting memory usage is 2 bytes of GPU memory per pixel.
    # When a value is used that is too small, thrashing may occur that will
    # result in visible stutter. Such thrashing is more likely to occur when CJK
    # language glyphs are rendered and when the size of the glyphs in pixels is
    # larger, such as for higher resolution displays.
    # The negative default values indicates to the engine that these settings
    # should be automatically set.
    'skia_glyph_atlas_width%': '-1',
    'skia_glyph_atlas_height%': '-1',

    # Determines the size of garbage collection threshold. After this many bytes
    # have been allocated, the mozjs garbage collector will run. Lowering this
    # has been found to reduce performance and decrease JavaScript memory usage.
    # For example, we have measured on at least one platform that performance
    # becomes 7% worse on average in certain cases when adjusting this number
    # from 8MB to 1MB.
    'mozjs_garbage_collection_threshold_in_bytes%': 8 * 1024 * 1024,

    # Max Cobalt CPU usage specifies that the cobalt program should
    # keep it's size below the specified size. A value of -1 causes this
    # value to be assumed from the starboard API function:
    # SbSystemGetTotalCPUMemory().
    'max_cobalt_cpu_usage%': -1,

    # Max Cobalt GPU usage specifies that the cobalt program should
    # keep it's size below the specified size. A value of -1 causes this
    # value to be assumed from the starboard API function:
    # SbSystemGetTotalGPUMemory().
    'max_cobalt_gpu_usage%': -1,

    # When specified this value will reduce the cpu memory consumption by
    # the specified amount. -1 disables the value.
    # When this value is specified then max_cobalt_cpu_usage will not be
    # used in memory_constrainer, but will still be used for triggering
    # a warning if the engine consumes more memory than this value specifies.
    'reduce_cpu_memory_by%': -1,

    # When specified this value will reduce the gpu memory consumption by
    # the specified amount. -1 disables the value.
    # When this value is specified then max_cobalt_gpu_usage will not be
    # used in memory_constrainer, but will still be used for triggering
    # a warning if the engine consumes more memory than this value specifies.
    'reduce_gpu_memory_by%': -1,

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


    # The only currently-supported Javascript engine is 'mozjs'.
    # TODO: Figure out how to massage gyp the right way to make this work
    # as expected, rather than requiring it to be set for each platform.
    #'javascript_engine%': 'mozjs',

    # Disable jit and run in interpreter-only mode by default. It can be set to
    # 1 to run in jit mode.  We have found that disabling jit often results in
    # faster JavaScript execution and lower memory usage.
    # Setting this to 1 on a platform or engine for which there is no JIT
    # implementation is a no-op.
    'cobalt_enable_jit%': 0,

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
    # The event polling mechanism available on this platform to support libevent.
    # Platforms may redefine to 'poll' if necessary.
    # Other mechanisms, e.g. devpoll, kqueue, select, are not yet supported.
    'sb_libevent_method%': 'epoll',

    # Use media source extension implementation that is conformed to the
    # Candidate Recommandation of July 5th 2016.
    'cobalt_media_source_2016%': 0,

    # Note that the following media buffer related variables are only used when
    # |cobalt_media_source_2016| is set to 1.

    # This can be set to "memory" or "file".  When it is set to "memory", the
    # media buffers will be stored in main memory allocated by SbMemory
    # functions.  When it is set to "file", the media buffers will be stored in
    # a temporary file in the system cache folder acquired by calling
    # SbSystemGetPath() with "kSbSystemPathCacheDirectory".  Note that when its
    # value is "file" the media stack will still allocate memory to cache the
    # the buffers in use.
    'cobalt_media_buffer_storage_type%': 'memory',
    # The amount of memory that will be used to store media buffers allocated
    # during system startup.  To allocate a large chunk at startup helps with
    # reducing frafmentation and can avoid failures to allocate incrementally.
    # This can be set to 0.
    'cobalt_media_buffer_initial_capacity%': 0 * 1024 * 1024,
    # When the media stack needs more memory to store media buffers, it will
    # allocate extra memory in units of |cobalt_media_buffer_allocation_unit|.
    # This can be set to 0, in which case the media stack will allocate extra
    # memory on demand.  When |cobalt_media_buffer_initial_capacity| and this
    # value are both set to 0, the media stack will allocate individual buffers
    # directly using SbMemory functions.
    'cobalt_media_buffer_allocation_unit%': 0 * 1024 * 1024,

    # Specifies the maximum amount of memory used by audio or text buffers of
    # media source before triggering a garbage collection.  A large value will
    # cause more memory being used by audio buffers but will also make
    # JavaScript app less likely to re-download audio data.  Note that the
    # JavaScript app may experience significant difficulty if this value is too
    # low.
    'cobalt_media_buffer_non_video_budget%': 5 * 1024 * 1024,

    # Specifies the maximum amount of memory used by video buffers of media
    # source before triggering a garbage collection when the video resolution is
    # lower than 1080p (1920x1080).  A large value will cause more memory being
    # used by video buffers but will also make JavaScript app less likely to
    # re-download video data.  Note that the JavaScript app may experience
    # significant difficulty if this value is too low.
    'cobalt_media_buffer_video_budget_1080p%': 16 * 1024 * 1024,
    # Specifies the maximum amount of memory used by video buffers of media
    # source before triggering a garbage collection when the video resolution is
    # lower than 4k (3840x2160).  A large value will cause more memory being
    # used by video buffers but will also make JavaScript app less likely to
    # re-download video data.  Note that the JavaScript app may experience
    # significant difficulty if this value is too low.
    'cobalt_media_buffer_video_budget_4k%': 60 * 1024 * 1024,
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
      'COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET=<(cobalt_media_buffer_non_video_budget)',
      'COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P=<(cobalt_media_buffer_video_budget_1080p)',
      'COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K=<(cobalt_media_buffer_video_budget_4k)',
      'COBALT_MEDIA_BUFFER_INITIAL_CAPACITY=<(cobalt_media_buffer_initial_capacity)',
      'COBALT_MEDIA_BUFFER_ALLOCATION_UNIT=<(cobalt_media_buffer_allocation_unit)',
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

    'conditions': [
      ['cobalt_media_source_2016 == 1', {
        'defines': [
          'COBALT_MEDIA_SOURCE_2016=1',
        ],
      }, {
        'defines': [
          'COBALT_MEDIA_SOURCE_2012=1',
        ],
      }],
      ['cobalt_media_buffer_storage_type == "memory"', {
        'defines': [
          'COBALT_MEDIA_BUFFER_STORAGE_TYPE_MEMORY=1',
        ],
      }, {
        'defines': [
          'COBALT_MEDIA_BUFFER_STORAGE_TYPE_FILE=1',
        ],
      }],
      ['final_executable_type=="shared_library"', {
        'target_conditions': [
          ['_toolset=="target"', {
            'defines': [
              # Rewrite main() functions into StarboardMain. TODO: This is a
              # hack, it would be better to be more surgical, here.
              'main=StarboardMain',
            ],
            'cflags': [
              # To link into a shared library on Linux and similar platforms,
              # the compiler must be told to generate Position Independent Code.
              # This appears to cause errors when linking the code statically,
              # however.
              '-fPIC',
            ],
          }],
        ],
      }],
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
          # TODO: This is needed to support the option to include
          # posix_emulation.h to all compiled source files. This dependency
          # should be refactored and removed.
          '<(DEPTH)/lbshell/src',
        ],
      }],  # OS == "lb_shell"
      ['OS == "starboard"', {
        'target_conditions': [
          ['_toolset=="target"', {
            # Keeps common Starboard target changes in the starboard project.
            'includes': [
              '../../../starboard/starboard_base_target.gypi',
            ],
          }],
        ],
      }],  # OS == "starboard"
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
          'COBALT_ENABLE_JAVASCRIPT_ERROR_LOGGING',
          'COBALT_SECURITY_SCREEN_CLEAR_TO_UGLY_COLOR',
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
          'COBALT_ENABLE_JAVASCRIPT_ERROR_LOGGING',
          'COBALT_SECURITY_SCREEN_CLEAR_TO_UGLY_COLOR',
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
          'COBALT_ENABLE_JAVASCRIPT_ERROR_LOGGING',
          'COBALT_SECURITY_SCREEN_CLEAR_TO_UGLY_COLOR',
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
  # Clients must copy over all content; to avoid having to copy over extra data, we
  # omit the test data
  'conditions': [
    ['cobalt_config != "gold" and cobalt_enable_lib == 0', {
      'variables' : {
        'cobalt_copy_debug_console': 1,
        'cobalt_copy_test_data': 1,
        'enable_about_scheme': 1,
        'enable_fake_microphone': 1,
        'enable_file_scheme': 1,
        'enable_network_logging': 1,
        'enable_remote_debugging%': 1,
        'enable_screenshot': 1,
        'enable_webdriver%': 1,
        'sb_allows_memory_tracking': 1,
      },
    },
    {
      'variables' : {
        'cobalt_copy_debug_console': 0,
        'cobalt_copy_test_data': 0,
        'enable_about_scheme': 0,
        'enable_fake_microphone': 0,
        'enable_file_scheme': 0,
        'enable_network_logging': 0,
        'enable_remote_debugging%': 0,
        'enable_screenshot': 0,
        'enable_webdriver': 0,
        'sb_allows_memory_tracking': 0,
      },
    }],
  ],
}
