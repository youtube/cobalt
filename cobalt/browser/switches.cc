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
#include <map>

namespace cobalt {
namespace browser {
namespace switches {

// NOTE: If a new switch is to be defined, the switch along with its help
// message needs to be inserted to the help_map manually.
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

const char kAudioDecoderStub[] = "audio_decoder_stub";
const char kAudioDecoderStubHelp[] =
    "Decode audio data using ShellRawAudioDecoderStub.";

const char kDebugConsoleMode[] = "debug_console";
const char kDebugConsoleModeHelp[] =
    "Switches different debug console modes: on | hud | off";

const char kDisableImageAnimations[] = "disable_image_animations";
const char kDisableImageAnimationsHelp[] =
    "Enables/disables animations on animated images (e.g. animated WebP).";

const char kDisableRasterizerCaching[] = "disable_rasterizer_caching";
const char kDisableRasterizerCachingHelp[] =
    "Disables caching of rasterized render tree nodes; caching improves "
    "performance but may result in sub-pixel differences.";

const char kDisableSignIn[] = "disable_sign_in";
const char kDisableSignInHelp[] =
    "Disables sign-in on platforms that use H5VCC Account Manager.";

const char kDisableSplashScreenOnReloads[] = "disable_splash_screen_on_reloads";
const char kDisableSplashScreenOnReloadsHelp[] =
    "Disables the splash screen on reloads; instead it will only appear on the "
    "first navigate.";

const char kDisableWebDriver[] = "disable_webdriver";
const char kDisableWebDriverHelp[] = "Do not create the WebDriver server.";

const char kDisableWebmVp9[] = "disable_webm_vp9";
const char kDisableWebmVp9Help[] = "Disable webm/vp9.";

const char kExtraWebFileDir[] = "web_file_path";
const char kExtraWebFileDirHelp[] =
    "Additional base directory for accessing web files via file://.";

const char kFakeMicrophone[] = "fake_microphone";
const char kFakeMicrophoneHelp[] =
    "If this flag is set, fake microphone will be used to mock the user voice "
    "input.";

const char kIgnoreCertificateErrors[] = "ignore_certificate_errors";
const char kIgnoreCertificateErrorsHelp[] =
    "Setting this switch causes all certificate errors to be ignored.";

const char kInputFuzzer[] = "input_fuzzer";
const char kInputFuzzerHelp[] =
    "If this flag is set, input will be continuously generated randomly "
    "instead of taken from an external input device (like a controller).";

const char kMemoryTracker[] = "memory_tracker";
const char kMemoryTrackerHelp[] =
    "Enables memory tracking by installing the memory tracker on startup.";

const char kMinCompatibilityVersion[] = "min_compatibility_version";
const char kMinCompatibilityVersionHelp[] =
    "The minimum version of Cobalt that will be checked during compatibility "
    "validations.";

const char kMinLogLevel[] = "min_log_level";
const char kMinLogLevelHelp[] =
    "Set the minimum logging level: info|warning|error|fatal.";

const char kNullAudioStreamer[] = "null_audio_streamer";
const char kNullAudioStreamerHelp[] =
    "Use the NullAudioStreamer. Audio will be decoded but will not play back. "
    "No audio output library will be initialized or used.";

const char kNullSavegame[] = "null_savegame";
const char kNullSavegameHelp[] =
    "Setting NullSavegame will result in no data being read from previous "
    "sessions and no data being persisted to future sessions. It effectively "
    "makes the app run as if it has no local storage.";

const char kPartialLayout[] = "partial_layout";
const char kPartialLayoutHelp[] = "Switches partial layout: on | off";

const char kProd[] = "prod";
const char kProdHelp[] =
    "Several checks are not enabled by default in non-production(gold) build. "
    "Use this flag to simulate production build behavior.";

const char kProxy[] = "proxy";
const char kProxyHelp[] = "Specifies a proxy to use for network connections.";

const char kRemoteDebuggingPort[] = "remote_debugging_port";
const char kRemoteDebuggingPortHelp[] =
    "Creates a remote debugging server and listens on the specified port.";

const char kRequireCSP[] = "require_csp";
const char kRequireCSPHelp[] =
    "Forbid Cobalt to start without receiving csp headers which is enabled by "
    "default in production.";

const char kRequireHTTPSLocation[] = "require_https";
const char kRequireHTTPSLocationHelp[] =
    "Ask Cobalt to only accept https url which is enabled by default in "
    "production.";

const char kShutdownAfter[] = "shutdown_after";
const char kShutdownAfterHelp[] =
    "If this flag is set, Cobalt will automatically shutdown after the "
    "specified number of seconds have passed.";

const char kStubImageDecoder[] = "stub_image_decoder";
const char kStubImageDecoderHelp[] =
    "Decode all images using StubImageDecoder.";

const char kSuspendFuzzer[] = "suspend_fuzzer";
const char kSuspendFuzzerHelp[] =
    "If this flag is set, alternating calls to |SbSystemRequestSuspend| and "
    "|SbSystemRequestUnpause| will be made periodically.";

const char kTimedTrace[] = "timed_trace";
const char kTimedTraceHelp[] =
    "If this is set, then a trace (see base/debug/trace_eventh.h) is started "
    "on Cobalt startup.  A value must also be specified for this switch, "
    "which is the duration in seconds of how long the trace will be done "
    "for before ending and saving the results to disk.  Results will be saved"
    " to the file timed_trace.json in the log output directory.";

const char kUseTTS[] = "use_tts";
const char kUseTTSHelp[] =
    "Enable text-to-speech functionality, for platforms that implement the "
    "speech synthesis API. If the platform doesn't have speech synthesis, "
    "TTSLogger will be used instead.";

extern const char kVideoContainerSizeOverride[] =
    "video_container_size_override";
extern const char kVideoContainerSizeOverrideHelp[] =
    "Set the video container size override";

extern const char kVideoDecoderStub[] = "video_decoder_stub";
extern const char kVideoDecoderStubHelp[] =
    "Decode video data using ShellRawVideoDecoderStub.";

const char kWebDriverListenIp[] = "webdriver_listen_ip";
const char kWebDriverListenIpHelp[] =
    "IP that the WebDriver server should be listening on. (INADDR_ANY if "
    "unspecified).";

const char kWebDriverPort[] = "webdriver_port";
const char kWebDriverPortHelp[] =
    "Port that the WebDriver server should be listening on.";

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

const char kDisableJavaScriptJit[] = "disable_javascript_jit";
const char kDisableJavaScriptJitHelp[] =
    "Specifies that javascript jit should be disabled.";

const char kEnableMapToMeshRectanglar[] = "enable_map_to_mesh_rectangular";
const char kEnableMapToMeshRectanglarHelp[] =
    "If toggled and map-to-mesh is supported on this platform, this allows it "
    "to accept the 'rectangular' keyword. Useful to get rectangular stereo "
    "video on platforms that do not support stereoscopy natively, letting the "
    "client apply a stereo mesh projection (one that differs for each eye).";

// If toggled, framerate statistics will be printed to stdout after each
// animation completes, or after a maximum number of frames has been collected.
const char kFPSPrint[] = "fps_stdout";
const char kFPSPrintHelp[] =
    "If toggled, framerate statistics will be printed to stdout after each "
    "animation completes, or after a maximum number of frames has been "
    "collected.";

const char kFPSOverlay[] = "fps_overlay";
const char kFPSOverlayHelp[] =
    "If toggled, framerate statistics will be displayed in an on-screen "
    "overlay and updated after each animation completes, or after a maximum "
    "number of frames has been collected.";

const char kHelp[] = "help";
const char kHelpHelp[] = "Prints help information of cobalt command";

const char kImageCacheSizeInBytes[] = "image_cache_size_in_bytes";
const char kImageCacheSizeInBytesHelp[] =
    "Determines the capacity of the image cache which manages image "
    "surfaces300 downloaded from a web page.  While it depends on the "
    "platform, often (and ideally) these images are cached within GPU memory.";

const char kInitialURL[] = "url";
const char kInitialURLHelp[] =
    "Setting this switch defines the startup URL that Cobalt will use.  If no "
    "value is set, a default URL will be used.";

const char kJavaScriptGcThresholdInBytes[] = "javascript_gc_threshold_in_bytes";
const char kJavaScriptGcThresholdInBytesHelp[] =
    "Specifies the javascript gc threshold. When this amount of garbage has "
    "collected then the garbage collector will begin running.";

const char kLocalStoragePartitionUrl[] = "local_storage_partition_url";
const char kLocalStoragePartitionUrlHelp[] =
    "Overrides the default storage partition with a custom partition URL to "
    "use for local storage. The provided URL is canonicalized.";

const char kMaxCobaltCpuUsage[] = "max_cobalt_cpu_usage";
const char kMaxCobaltCpuUsageHelp[] =
    "Specifies the maximum CPU usage of the cobalt.";

const char kMaxCobaltGpuUsage[] = "max_cobalt_gpu_usage";
const char kMaxCobaltGpuUsageHelp[] =
    "Specifies the maximum GPU usage of the cobalt.";

const char kOffscreenTargetCacheSizeInBytes[] =
    "offscreen_target_cache_size_in_bytes";
const char kOffscreenTargetCacheSizeInBytesHelp[] =
    "Determines the amount of GPU memory the offscreen target atlases will "
    "use. This is specific to the direct-GLES rasterizer and caches any render "
    "tree nodes which require skia for rendering. Two atlases will be "
    "allocated from this memory or multiple atlases of the frame size if the "
    "limit allows. It is recommended that enough memory be reserved for two "
    "RGBA atlases about a quarter of the frame size.";

const char kReduceCpuMemoryBy[] = "reduce_cpu_memory_by";
const char kReduceCpuMemoryByHelp[] =
    "Reduces the cpu-memory of the system by this amount. This causes AutoMem "
    "to reduce the runtime size of the CPU-Memory caches.";

const char kReduceGpuMemoryBy[] = "reduce_gpu_memory_by";
const char kReduceGpuMemoryByHelp[] =
    "Reduces the gpu-memory of the system by this amount. This causes AutoMem "
    "to reduce the runtime size of the GPU-Memory caches.";

const char kRemoteTypefaceCacheSizeInBytes[] =
    "remote_typeface_cache_size_in_bytes";
const char kRemoteTypefaceCacheSizeInBytesHelp[] =
    "Determines the capacity of the remote typefaces cache which manages all "
    "typefaces downloaded from a web page.";

const char kRetainRemoteTypefaceCacheDuringSuspend[] =
    "retain_remote_typeface_cache_during_suspend";
const char kRetainRemoteTypefaceCacheDuringSuspendHelp[] =
    "Causes the remote typeface cache to be retained when Cobalt is suspended,"
    " so that they don't need to be re-downloaded when Cobalt is resumed.";

const char kScratchSurfaceCacheSizeInBytes[] =
    "scratch_surface_cache_size_in_bytes";
const char kScratchSurfaceCacheSizeInBytesHelp[] =
    "Determines the capacity of the scratch surface cache.  The scratch surface"
    " cache facilitates the reuse of temporary offscreen surfaces within a "
    "single frame.  This setting is only relevant when using the "
    "hardware-accelerated Skia rasterizer.  While it depends on the platform, "
    "this setting may affect GPU memory usage.";

const char kSkiaCacheSizeInBytes[] = "skia_cache_size_in_bytes";
const char kSkiaCacheSizeInBytesHelp[] =
    "Determines the capacity of the skia cache.  The Skia cache is maintained "
    "within Skia and is used to cache the results of complicated effects such "
    "as shadows, so that Skia draw calls that are used repeatedly across "
    "frames can be cached into surfaces.  This setting is only relevant when "
    "using the hardware-accelerated Skia rasterizer.  While it depends on "
    "the platform, this setting may affect GPU memory usage.";

const char kSkiaTextureAtlasDimensions[] = "skia_atlas_texture_dimensions";
const char kSkiaTextureAtlasDimensionsHelp[] =
    "Specifies the dimensions of the Skia caching texture atlases (e.g. "
    "2048x2048).";

const char kSoftwareSurfaceCacheSizeInBytes[] =
    "software_surface_cache_size_in_bytes";
const char kSoftwareSurfaceCacheSizeInBytesHelp[] =
    "Only relevant if you are using the Blitter API. Determines the capacity "
    "of the software surface cache, which is used to "
    "cache all surfaces that are rendered via a software rasterizer to avoid "
    "re-rendering them.";

const char kFallbackSplashScreenURL[] = "fallback_splash_screen_url";
const char kFallbackSplashScreenURLHelp[] =
    "Setting this switch defines the splash screen URL that Cobalt will "
    "use in absence of a web cache. The referenced url should be a content "
    "file (for example file:///foobar.html) or an embedded file "
    "(for example h5vcc-embedded://foobar.html) and all files referenced must "
    "be content or embedded files as well. If none is passed "
    "(case-insensitive), no splash screen will be constructed. If "
    "no value is set, the URL in gyp_configuration.gypi or base.gypi will be "
    "used.";

const char kVersion[] = "version";
const char kVersionHelp[] = "Prints the current version of Cobalt";

const char kViewport[] = "viewport";
const char kViewportHelp[] = "Specifies the viewport size: width ['x' height]";

const char kVideoPlaybackRateMultiplier[] = "video_playback_rate_multiplier";
const char kVideoPlaybackRateMultiplierHelp[] =
    "Specifies the multiplier of video playback rate.  Set to a value greater "
    "than 1.0 to play video faster.  Set to a value less than 1.0 to play "
    "video slower.";

std::string HelpMessage() {
  std::string help_message;
  std::map<const char*, const char*> help_map {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
    {kAudioDecoderStub, kAudioDecoderStubHelp},
        {kDebugConsoleMode, kDebugConsoleModeHelp},
        {kDisableImageAnimations, kDisableImageAnimationsHelp},
        {kDisableRasterizerCaching, kDisableRasterizerCachingHelp},
        {kDisableSignIn, kDisableSignInHelp},
        {kDisableSplashScreenOnReloads, kDisableSplashScreenOnReloadsHelp},
        {kDisableWebDriver, kDisableWebDriverHelp},
        {kDisableWebmVp9, kDisableWebmVp9Help},
        {kExtraWebFileDir, kExtraWebFileDirHelp},
        {kFakeMicrophone, kFakeMicrophoneHelp},
        {kIgnoreCertificateErrors, kIgnoreCertificateErrorsHelp},
        {kInputFuzzer, kInputFuzzerHelp}, {kMemoryTracker, kMemoryTrackerHelp},
        {kMinCompatibilityVersion, kMinCompatibilityVersionHelp},
        {kMinLogLevel, kMinLogLevelHelp},
        {kNullAudioStreamer, kNullAudioStreamerHelp},
        {kNullSavegame, kNullSavegameHelp},
        {kPartialLayout, kPartialLayoutHelp}, {kProd, kProdHelp},
        {kProxy, kProxyHelp}, {kRemoteDebuggingPort, kRemoteDebuggingPortHelp},
        {kRequireCSP, kRequireCSPHelp},
        {kRequireHTTPSLocation, kRequireHTTPSLocationHelp},
        {kShutdownAfter, kShutdownAfterHelp},
        {kStubImageDecoder, kStubImageDecoderHelp},
        {kSuspendFuzzer, kSuspendFuzzerHelp}, {kTimedTrace, kTimedTraceHelp},
        {kUseTTS, kUseTTSHelp},
        {kVideoContainerSizeOverride, kVideoContainerSizeOverrideHelp},
        {kVideoDecoderStub, kVideoDecoderStubHelp},
        {kWebDriverListenIp, kWebDriverListenIpHelp},
        {kWebDriverPort, kWebDriverPortHelp},
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

        {kDisableJavaScriptJit, kDisableJavaScriptJitHelp},
        {kEnableMapToMeshRectanglar, kEnableMapToMeshRectanglarHelp},
        {kFPSPrint, kFPSPrintHelp}, {kFPSOverlay, kFPSOverlayHelp},
        {kHelp, kHelpHelp},
        {kImageCacheSizeInBytes, kImageCacheSizeInBytesHelp},
        {kInitialURL, kInitialURLHelp},
        {kJavaScriptGcThresholdInBytes, kJavaScriptGcThresholdInBytesHelp},
        {kLocalStoragePartitionUrl, kLocalStoragePartitionUrlHelp},
        {kMaxCobaltCpuUsage, kMaxCobaltCpuUsageHelp},
        {kMaxCobaltGpuUsage, kMaxCobaltGpuUsageHelp},
        {kOffscreenTargetCacheSizeInBytes,
         kOffscreenTargetCacheSizeInBytesHelp},
        {kReduceCpuMemoryBy, kReduceCpuMemoryByHelp},
        {kReduceGpuMemoryBy, kReduceGpuMemoryByHelp},
        {kRemoteTypefaceCacheSizeInBytes, kRemoteTypefaceCacheSizeInBytesHelp},
        {kRetainRemoteTypefaceCacheDuringSuspend,
         kRetainRemoteTypefaceCacheDuringSuspendHelp},
        {kScratchSurfaceCacheSizeInBytes, kScratchSurfaceCacheSizeInBytesHelp},
        {kSkiaCacheSizeInBytes, kSkiaCacheSizeInBytesHelp},
        {kSkiaTextureAtlasDimensions, kSkiaTextureAtlasDimensionsHelp},
        {kSoftwareSurfaceCacheSizeInBytes,
         kSoftwareSurfaceCacheSizeInBytesHelp},
        {kFallbackSplashScreenURL, kFallbackSplashScreenURLHelp},
        {kVersion, kVersionHelp}, {kViewport, kViewportHelp},
        {kVideoPlaybackRateMultiplier, kVideoPlaybackRateMultiplierHelp},
  };

  for (const auto& switch_message : help_map) {
    help_message.append("  --")
        .append(switch_message.first)
        .append("\n")
        .append("  ")
        .append(switch_message.second)
        .append("\n\n");
  }
  return help_message;
}

}  // namespace switches
}  // namespace browser
}  // namespace cobalt
