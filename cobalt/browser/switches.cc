// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/switches.h"

namespace cobalt {
namespace browser {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

// Decode audio data using ShellRawAudioDecoderStub.
const char kAudioDecoderStub[] = "audio_decoder_stub";

// Switches different debug console modes: on | hud | off
const char kDebugConsoleMode[] = "debug_console";

// Enables/disables animations on animated images (e.g. animated WebP).
const char kDisableImageAnimations[] = "disable_image_animations";

// Disables the splash screen on reloads; instead it will only appear on the
// first navigate.
const char kDisableSplashScreenOnReloads[] = "disable_splash_screen_on_reloads";

// Do not create the WebDriver server.
const char kDisableWebDriver[] = "disable_webdriver";

// Disable webm/vp9.
const char kDisableWebmVp9[] = "disable_webm_vp9";

// Additional base directory for accessing web files via file://.
const char kExtraWebFileDir[] = "web_file_path";

// If this flag is set, fake microphone will be used to mock the user voice
// input.
const char kFakeMicrophone[] = "fake_microphone";

// Setting this switch causes all certificate errors to be ignored.
const char kIgnoreCertificateErrors[] = "ignore_certificate_errors";

// If this flag is set, input will be continuously generated randomly instead of
// taken from an external input device (like a controller).
const char kInputFuzzer[] = "input_fuzzer";

// Set the minimum logging level: info|warning|error|fatal.
const char kMinLogLevel[] = "min_log_level";

// Use the NullAudioStreamer. Audio will be decoded but will not play back. No
// audio output library will be initialized or used.
const char kNullAudioStreamer[] = "null_audio_streamer";

// Setting NullSavegame will result in no data being read from previous sessions
// and no data being persisted to future sessions.  It effectively makes the
// app run as if it has no local storage.
const char kNullSavegame[] = "null_savegame";

// Several checks are not enabled by default in non-production(gold) build. Use
// this flag to simulate production build behavior.
const char kProd[] = "prod";

// Specifies a proxy to use for network connections.
const char kProxy[] = "proxy";

// Switches partial layout: on | off
const char kPartialLayout[] = "partial_layout";

// Creates a remote debugging server and listens on the specified port.
const char kRemoteDebuggingPort[] = "remote_debugging_port";

// Forbid Cobalt to start without receiving csp headers which is enabled by
// default in production.
const char kRequireCSP[] = "require_csp";

// Ask Cobalt to only accept https url which is enabled by default in
// production.
const char kRequireHTTPSLocation[] = "require_https";

// If this flag is set, Cobalt will automatically shutdown after the specified
// number of seconds have passed.
const char kShutdownAfter[] = "shutdown_after";

// Decode all images using StubImageDecoder.
const char kStubImageDecoder[] = "stub_image_decoder";

// If this flag is set, alternating calls to |SbSystemRequestSuspend| and
// |SbSystemRequestUnpause| will be made periodically.
const char kSuspendFuzzer[] = "suspend_fuzzer";

// If this is set, then a trace (see base/debug/trace_eventh.h) is started on
// Cobalt startup.  A value must also be specified for this switch, which is
// the duration in seconds of how long the trace will be done for before ending
// and saving the results to disk.  Results will be saved to the file
// "timed_trace.json" in the log output directory.
const char kTimedTrace[] = "timed_trace";

// Set the video container size override.
extern const char kVideoContainerSizeOverride[] =
    "video_container_size_override";

// Decode video data using ShellRawVideoDecoderStub.
extern const char kVideoDecoderStub[] = "video_decoder_stub";

// Enable text-to-speech functionality, for platforms that implement the speech
// synthesis API. If the platform doesn't have speech synthesis, TTSLogger will
// be used instead.
const char kUseTTS[] = "use_tts";

// Port that the WebDriver server should be listening on.
const char kWebDriverPort[] = "webdriver_port";

// IP that the WebDriver server should be listening on.
// (INADDR_ANY if unspecified).
const char kWebDriverListenIp[] = "webdriver_listen_ip";

// Enables memory tracking by installing the memory tracker on startup.
const char kMemoryTracker[] = "memory_tracker";

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

// If toggled, framerate statistics will be printed to stdout after each
// animation completes, or after a maximum number of frames has been collected.
const char kFPSPrint[] = "fps_stdout";

// If toggled, framerate statistics will be displayed in an on-screen overlay
// and updated after each animation completes, or after a maximum number of
// frames has been collected.
const char kFPSOverlay[] = "fps_overlay";

// Determines the capacity of the image cache which manages image surfaces
// downloaded from a web page.  While it depends on the platform, often (and
// ideally) these images are cached within GPU memory.
const char kImageCacheSizeInBytes[] = "image_cache_size_in_bytes";

// Setting this switch defines the startup URL that Cobalt will use.  If no
// value is set, a default URL will be used.
const char kInitialURL[] = "url";

// Overrides the default storage partition with a custom partition URL to use
// for local storage. The provided URL is canonicalized.
const char kLocalStoragePartitionUrl[] = "local_storage_partition_url";

// Determines the capacity of the remote typefaces cache which manages all
// typefaces downloaded from a web page.
const char kRemoteTypefaceCacheSizeInBytes[] =
    "remote_typeface_cache_size_in_bytes";

// Causes the remote typeface cache to be retained when Cobalt is suspended, so
// that they don't need to be re-downloaded when Cobalt is resumed.
const char kRetainRemoteTypefaceCacheDuringSuspend[] =
    "retain_remote_typeface_cache_during_suspend";

// Determines the capacity of the scratch surface cache.  The scratch surface
// cache facilitates the reuse of temporary offscreen surfaces within a single
// frame.  This setting is only relevant when using the hardware-accelerated
// Skia rasterizer.  While it depends on the platform, this setting may affect
// GPU memory usage.
const char kScratchSurfaceCacheSizeInBytes[] =
    "scratch_surface_cache_size_in_bytes";

// Determines the capacity of the skia cache.  The Skia cache is maintained
// within Skia and is used to cache the results of complicated effects such as
// shadows, so that Skia draw calls that are used repeatedly across frames can
// be cached into surfaces.  This setting is only relevant when using the
// hardware-accelerated Skia rasterizer.  While it depends on the platform, this
// setting may affect GPU memory usage.
const char kSkiaCacheSizeInBytes[] = "skia_cache_size_in_bytes";

// Only relevant if you are using the Blitter API.
// Determines the capacity of the software surface cache, which is used to
// cache all surfaces that are rendered via a software rasterizer to avoid
// re-rendering them.
const char kSoftwareSurfaceCacheSizeInBytes[] =
    "software_surface_cache_size_in_bytes";

// Setting this switch defines the splash screen URL that Cobalt will
// use in absence of a web cache. The referenced url should be a
// content file (for example file:///foobar.html) or an embedded file
// (for example h5vcc-embedded://foobar.html) and all files referenced
// must be content or embedded files as well. If "none" is passed
// (case-insensitive), no splash screen will be constructed. If no
// value is set, the URL in gyp_configuration.gypi or base.gypi will
// be used.
const char kFallbackSplashScreenURL[] = "fallback_splash_screen_url";

// Determines the capacity of the surface cache.  The surface cache tracks which
// render tree nodes are being re-used across frames and stores the nodes that
// are most CPU-expensive to render into surfaces.  While it depends on the
// platform, this setting may affect GPU memory usage.
const char kSurfaceCacheSizeInBytes[] = "surface_cache_size_in_bytes";

// Determines the amount of GPU memory the offscreen target atlases will
// use. This is specific to the direct-GLES rasterizer and serves a similar
// purpose as the surface_cache_size_in_bytes, but caches any render tree
// nodes which require skia for rendering. Two atlases will be allocated
// from this memory or multiple atlases of the frame size if the limit
// allows. It is recommended that enough memory be reserved for two RGBA
// atlases about a quarter of the frame size.
const char kOffscreenTargetCacheSizeInBytes[] =
    "offscreen_target_cache_size_in_bytes";

// Specifies the viewport size: width ['x' height]
const char kViewport[] = "viewport";

// Specifies that javascript jit should be disabled.
const char kDisableJavaScriptJit[] = "disable_javascript_jit";

// Specifies the javascript gc threshold. When this amount of garbage has
// collected then the garbage collector will begin running.
const char kJavaScriptGcThresholdInBytes[] = "javascript_gc_threshold_in_bytes";

const char kSkiaTextureAtlasDimensions[] = "skia_atlas_texture_dimensions";

// Specifies the maximum CPU usage of the cobalt.
const char kMaxCobaltCpuUsage[] = "max_cobalt_cpu_usage";

// Specifies the maximum GPU usage of the cobalt.
const char kMaxCobaltGpuUsage[] = "max_cobalt_gpu_usage";

// Reduces the cpu-memory of the system by this amount. This causes AutoMem to
// reduce the runtime size of the CPU-Memory caches.
const char kReduceCpuMemoryBy[] = "reduce_cpu_memory_by";

// Reduces the gpu-memory of the system by this amount. This causes AutoMem to
// reduce the runtime size of the GPU-Memory caches.
const char kReduceGpuMemoryBy[] = "reduce_gpu_memory_by";

// Specifies the multiplier of video playback rate.  Set to a value greater than
// 1.0 to play video faster.  Set to a value less than 1.0 to play video slower.
const char kVideoPlaybackRateMultiplier[] = "video_playback_rate_multiplier";

}  // namespace switches
}  // namespace browser
}  // namespace cobalt
