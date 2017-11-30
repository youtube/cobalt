# Copyright 2017 Google Inc. All Rights Reserved.
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

#####################################################################
# If you modify this file, PLEASE REMEMBER TO UPDATE
# //cobalt/build/config/base.gni AS WELL
#####################################################################

# This contains the defaults and documentation for all Cobalt-defined GYP
# variables. Starboard-defined variables are specified in
# starboard/build/base_configuration.gypi.
#
# starboard/build/cobalt_configuration includes this file automatically in all
# .gyp files processed by starboard/build/gyp_cobalt.
{
  'variables': {
    # Cobalt variables.

    # We need to define some variables inside of an inner 'variables' scope
    # so that they can be referenced by other outer variables here.  Also, it
    # allows for the specification of default values that get referenced by
    # a top level scope.
    'variables': {
      'cobalt_webapi_extension_source_idl_files%': [],
      'cobalt_webapi_extension_generated_header_idl_files%': [],
      'cobalt_media_source_2016%': 1,
    },

    # Whether Cobalt is being built.
    'cobalt': 1,

    # Contains the current build configuration.
    'cobalt_config%': 'gold',
    'cobalt_fastbuild%': 0,

    # Enable support for the map to mesh filter, which is primarily used to
    # implement spherical video playback.
    'enable_map_to_mesh%': 0,

    # Enables embedding Cobalt as a shared library within another app. This
    # requires a 'lib' starboard implementation for the corresponding platform.
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
    'cobalt_user_on_exit_strategy%': 'stop',

    # Contains the current font package selection.  This can be used to trade
    # font quality, coverage, and latency for different font package sizes.
    # The font package can be one of the following options:
    #   'standard' -- The default package. It includes all sans-serif, serif,
    #                 and FCC fonts, non-CJK fallback fonts in both 'normal' and
    #                 'bold' weights, 'normal' weight CJK ('bold' weight CJK is
    #                 synthesized from it), and historic script fonts. This
    #                 package is ~31.4MB.
    #   'limited_with_jp' -- A significantly smaller package than 'standard'.
    #                 This package removes all but 'normal' and 'bold' weighted
    #                 sans-serif and serif, removes the FCC fonts (which must be
    #                 provided by the system or downloaded from the web),
    #                 removes the 'bold' weighted non-CJK fallback fonts (the
    #                 'normal' weight is still included and is used to
    #                 synthesize bold), and replaces standard CJK with low
    #                 quality CJK. However, higher quality Japanese is still
    #                 included. Because low quality CJK cannot synthesize bold,
    #                 bold glyphs are unavailable in Chinese and Korean.
    #                 Historic script fonts are not included. This package is
    #                 ~11.5MB.
    #   'limited'  -- A smaller package than 'limited_with_jp'. The two packages
    #                 are identical with the exception that 'limited' does not
    #                 include the higher quality Japanese font; instead it
    #                 relies on low quality CJK for all CJK characters. Because
    #                 low quality CJK cannot synthesize bold, bold glyphs are
    #                 unavailable in Chinese, Japanese, and Korean. This package
    #                 is ~8.3MB.
    #   'minimal'  -- The smallest possible font package. It only includes
    #                 Roboto's Basic Latin characters. Everything else must be
    #                 provided by the system or downloaded from the web. This
    #                 package is ~40.0KB.
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
    'cobalt_font_package_override_fallback_historic%': -1,
    'cobalt_font_package_override_fallback_emoji%': -1,
    'cobalt_font_package_override_fallback_symbols%': -1,

    # Build version number.
    'cobalt_version%': 0,

    # Defines what kind of rasterizer will be used.  This can be adjusted to
    # force a stub graphics implementation or software graphics implementation.
    # It can be one of the following options:
    #   'direct-gles' -- Uses a light wrapper over OpenGL ES to handle most
    #                    draw elements. This will fall back to the skia hardware
    #                    rasterizer for some render tree node types, but is
    #                    generally faster on the CPU and GPU. This can handle
    #                    360 rendering.
    #   'hardware'    -- As much hardware acceleration of graphics commands as
    #                    possible. This uses skia to wrap OpenGL ES commands.
    #                    Required for 360 rendering.
    #   'software'    -- Perform most rasterization using the CPU and only
    #                    interact with the GPU to send the final image to the
    #                    output window.
    #   'stub'        -- Stub graphics rasterization.  A rasterizer object will
    #                    still be available and valid, but it will do nothing.
    'rasterizer_type%': 'direct-gles',

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

    # Override this value to adjust the default rasterizer setting for your
    # platform.
    'default_renderer_options_dependency%': '<(DEPTH)/cobalt/renderer/default_options_starboard.gyp:default_options',

    # Override this to inject a custom interface into Cobalt's JavaScript
    # `window` global object.  This implies that you will have to provide your
    # own IDL files to describe that interface and all interfaces that it
    # references.  See cobalt/doc/webapi_extension.md for more information.
    'cobalt_webapi_extension_source_idl_files%': [
      '<@(cobalt_webapi_extension_source_idl_files)',
    ],
    # Override this to have Cobalt build IDL files that result in generated
    # header files that may need to be included from other C++ source files.
    # This includes, for example, IDL enumerations.  See
    # cobalt/doc/webapi_extension.md for more information.
    'cobalt_webapi_extension_generated_header_idl_files%': [
      '<@(cobalt_webapi_extension_generated_header_idl_files)',
    ],

    # This gyp target must implement the functions defined in
    # <(DEPTH)/cobalt/browser/idl_extensions.h.  See
    # cobalt/doc/webapi_extension.md for more information.
    'cobalt_webapi_extension_gyp_target%':
        '<(DEPTH)/cobalt/browser/null_webapi_extension.gyp:null_webapi_extension',

    # Allow throttling of the frame rate. This is expressed in terms of
    # milliseconds and can be a floating point number. Keep in mind that
    # swapping frames may take some additional processing time, so it may be
    # better to specify a lower delay. For example, '33' instead of '33.33'
    # for 30 Hz refresh.
    'cobalt_minimum_frame_time_in_milliseconds%': '16.0',

    # Cobalt will call eglSwapInterval() and specify this value before calling
    # eglSwapBuffers() each frame.
    'cobalt_egl_swap_interval%': 1,

    # Set to 1 to build with DIAL support.
    'in_app_dial%': 0,

    # Set to 1 to enable a custom MediaSessionClient.
    'custom_media_session_client%': 0,

    # Set to 1 to enable H5vccAccountManager.
    'enable_account_manager%': 0,

    # Set to 1 to enable H5vccCrashLog.
    'enable_crash_log%': 0,

    # Set to 1 to enable H5vccSSO (Single Sign On).
    'enable_sso%': 0,

    # Set to 1 to compile with SPDY support.
    'enable_spdy%': 0,

    # Set to 1 to enable filtering of HTTP headers before sending.
    'enable_xhr_header_filtering%': 0,

    # List of platform-specific targets that get compiled into cobalt.
    'cobalt_platform_dependencies%': [],

    # The URL of default build time splash screen - see
    #   cobalt/doc/splash_screen.md for information about this.
    'fallback_splash_screen_url%': 'none',

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

    # Determines the amount of GPU memory the offscreen target atlases will
    # use. This is specific to the direct-GLES rasterizer and serves a similar
    # purpose as the surface_cache_size_in_bytes, but caches any render tree
    # nodes which require skia for rendering. Two atlases will be allocated
    # from this memory or multiple atlases of the frame size if the limit
    # allows. It is recommended that enough memory be reserved for two RGBA
    # atlases about a quarter of the frame size.
    'offscreen_target_cache_size_in_bytes%': -1,

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

    # Determines the size of garbage collection threshold. After this many
    # bytes have been allocated, the SpiderMonkey garbage collector will run.
    # Lowering this has been found to reduce performance and decrease
    # JavaScript memory usage. For example, we have measured on at least one
    # platform that performance becomes 7% worse on average in certain cases
    # when adjusting this number from 8MB to 1MB.
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

    # The only currently-supported Javascript engine is 'mozjs-45'.
    'default_javascript_engine': 'mozjs-45',
    'javascript_engine%': '<(default_javascript_engine)',

    # Disable JIT and run in interpreter-only mode by default. It can be set
    # to 1 to run in JIT mode.  For SpiderMonkey in particular, we have found
    # that disabling JIT often results in faster JavaScript execution and
    # lower memory usage.
    # Setting this to 1 on a platform or engine for which there is no JIT
    # implementation is a no-op.
    # Setting this to 0 on an engine for which there is a JIT implementation
    # is a platform configuration error.
    'cobalt_enable_jit%': 0,

    # Can be set to enable zealous garbage collection, if |javascript_engine|
    # supports it.  Zealous garbage collection will cause garbage collection
    # to occur much more frequently than normal, for the purpose of finding or
    # reproducing bugs.
    'cobalt_gc_zeal%': 0,

    # Use media source extension implementation that is conformed to the
    # Candidate Recommandation of July 5th 2016.
    'cobalt_media_source_2016%': '<(cobalt_media_source_2016)',

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
    # When either |cobalt_media_buffer_initial_capacity| or
    # |cobalt_media_buffer_allocation_unit| isn't zero, media buffers will be
    # allocated using a memory pool.  Set the following variable to 1 to
    # allocate the media buffer pool memory on demand and return all memory to
    # the system when there is no media buffer allocated.  Setting the following
    # value to 0 results in that Cobalt will allocate
    # |cobalt_media_buffer_initial_capacity| bytes for media buffer on startup
    # and will not release any media buffer memory back to the system even if
    # there is no media buffers allocated.
    'cobalt_media_buffer_pool_allocate_on_demand%': 1,
    # The amount of memory that will be used to store media buffers allocated
    # during system startup.  To allocate a large chunk at startup helps with
    # reducing fragmentation and can avoid failures to allocate incrementally.
    # This can be set to 0.
    'cobalt_media_buffer_initial_capacity%': 21 * 1024 * 1024,
    # When the media stack needs more memory to store media buffers, it will
    # allocate extra memory in units of |cobalt_media_buffer_allocation_unit|.
    # This can be set to 0, in which case the media stack will allocate extra
    # memory on demand.  When |cobalt_media_buffer_initial_capacity| and this
    # value are both set to 0, the media stack will allocate individual buffers
    # directly using SbMemory functions.
    'cobalt_media_buffer_allocation_unit%': 1 * 1024 * 1024,

    # The media buffer will be allocated using the following alignment.  Set
    # this to a larger value may increase the memory consumption of media
    # buffers.
    'cobalt_media_buffer_alignment%': 0,
    # Extra bytes allocated at the end of a media buffer to ensure that the
    # buffer can be use optimally by specific instructions like SIMD.  Set to 0
    # to remove any padding.
    'cobalt_media_buffer_padding%': 0,

    # The memory used when playing mp4 videos that is not in DASH format.  The
    # resolution of such videos shouldn't go beyond 1080p.  Its value should be
    # less than the sum of 'cobalt_media_buffer_non_video_budget' and
    # 'cobalt_media_buffer_video_budget_1080p' but not less than 8 MB.
    'cobalt_media_buffer_progressive_budget%': 12 * 1024 * 1024,

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

    'compiler_flags_host': [
      '-D__LB_HOST__',  # TODO: Is this still needed?
    ],

    'defines_debug': [
      'ALLOCATOR_STATS_TRACKING',
      'COBALT_BOX_DUMP_ENABLED',
      'COBALT_BUILD_TYPE_DEBUG',
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

      # TODO: Rename to COBALT_LOGGING_ENABLED.
      '__LB_SHELL__FORCE_LOGGING__',

      'SK_DEVELOPER',
    ],
    'defines_devel': [
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
    'defines_qa': [
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
    ],
    'defines_gold': [
      'ALLOCATOR_STATS_TRACKING',
      'COBALT_BUILD_TYPE_GOLD',
      'COBALT_FORCE_CSP',
      'COBALT_FORCE_HTTPS',
      'TRACING_DISABLED',
    ],
  },

  'target_defaults': {
    'defines': [
      'COBALT',
      'COBALT_MEDIA_BUFFER_POOL_ALLOCATE_ON_DEMAND=<(cobalt_media_buffer_pool_allocate_on_demand)',
      'COBALT_MEDIA_BUFFER_INITIAL_CAPACITY=<(cobalt_media_buffer_initial_capacity)',
      'COBALT_MEDIA_BUFFER_ALLOCATION_UNIT=<(cobalt_media_buffer_allocation_unit)',
      'COBALT_MEDIA_BUFFER_ALIGNMENT=<(cobalt_media_buffer_alignment)',
      'COBALT_MEDIA_BUFFER_PADDING=<(cobalt_media_buffer_padding)',
      'COBALT_MEDIA_BUFFER_PROGRESSIVE_BUDGET=<(cobalt_media_buffer_progressive_budget)',
      'COBALT_MEDIA_BUFFER_NON_VIDEO_BUDGET=<(cobalt_media_buffer_non_video_budget)',
      'COBALT_MEDIA_BUFFER_VIDEO_BUDGET_1080P=<(cobalt_media_buffer_video_budget_1080p)',
      'COBALT_MEDIA_BUFFER_VIDEO_BUDGET_4K=<(cobalt_media_buffer_video_budget_4k)',
    ],
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
      ['cobalt_gc_zeal == 1', {
        'defines': [
          'COBALT_GC_ZEAL=1',
          'JS_GC_ZEAL=1',
        ],
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
  }, # end of target_defaults

  # For configurations other than Gold, set the flag that lets test data files
  # be copied and carried along with the build.
  # Clients must copy over all content; to avoid having to copy over extra data, we
  # omit the test data
  'conditions': [
    ['cobalt_config != "gold"', {
      'variables' : {
        'cobalt_copy_debug_console': 1,
        'enable_about_scheme': 1,
        'enable_fake_microphone': 1,
        'enable_file_scheme': 1,
        'enable_network_logging': 1,
        'enable_remote_debugging%': 1,
        'enable_screenshot': 1,
        'enable_webdriver%': 1,
      },
    },
    {
      'variables' : {
        'cobalt_copy_debug_console': 0,
        'enable_about_scheme': 0,
        'enable_fake_microphone': 0,
        'enable_file_scheme': 0,
        'enable_network_logging': 0,
        'enable_remote_debugging%': 0,
        'enable_screenshot': 0,
        'enable_webdriver': 0,
      },
    }],
    ['cobalt_config != "gold" and cobalt_enable_lib == 0', {
      'variables' : {
        'cobalt_copy_test_data': 1,
      },
    },
    {
      'variables' : {
        'cobalt_copy_test_data': 0,
      },
    }],
  ],
}
