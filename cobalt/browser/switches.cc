// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

const char kDebugConsoleMode[] = "debug_console";
const char kDebugConsoleModeHelp[] =
    "Switches different debug console modes: on | hud | off";

const char kDevServersListenIp[] = "dev_servers_listen_ip";
const char kDevServersListenIpHelp[] =
    "IP address of the interface that internal development servers (remote web "
    "debugger and WebDriver) listen on. If unspecified, INADDR_ANY (on most "
    "platforms). Tip: To listen to ANY interface use \"::\" (\"0.0.0.0\" for "
    "IPv4), and to listen to LOOPBACK use \"::1\" (\"127.0.0.1\" for IPv4)";

#if defined(ENABLE_DEBUGGER)
const char kRemoteDebuggingPort[] = "remote_debugging_port";
const char kRemoteDebuggingPortHelp[] =
    "Remote web debugger is served from the specified port. If 0, then the "
    "remote web debugger is disabled.";

const char kWaitForWebDebugger[] = "wait_for_web_debugger";
const char kWaitForWebDebuggerHelp[] =
    "Waits for remote web debugger to connect before loading the page.  A "
    "number may optionally be specified to indicate which in a sequence of "
    "page loads should wait.  For example, if the startup URL is a loader and "
    "that loader changes window.location to the URL of the actual app, then "
    "specify 1 to debug the loader or 2 to debug the app.";
#endif  // ENABLE_DEBUGGER

const char kDisableImageAnimations[] = "disable_image_animations";
const char kDisableImageAnimationsHelp[] =
    "Enables/disables animations on animated images (e.g. animated WebP).";

const char kForceDeterministicRendering[] = "force_deterministic_rendering";
const char kForceDeterministicRenderingHelp[] =
    "Forces the renderer to avoid doing anything that may result in "
    "1-pixel-off non-deterministic rendering output.  For example, a renderer "
    "may implement an optimization where text glyphs are rendered once and "
    "cached and re-used in situations where the cached glyph is approximately "
    "very similar, even if it is not exactly the same.  Setting this flag "
    "avoids that kind of behavior, allowing strict screen-diff tests to pass.";

const char kDisableMediaCodecs[] = "disable_media_codecs";
const char kDisableMediaCodecsHelp[] =
    "Disables the semicolon-separated list of codecs that will be treated as "
    "unsupported for media playback. Used for debugging and testing purposes."
    "It uses sub-string match to determine whether a codec is disabled, for "
    "example, setting the value to \"avc;hvc\" will disable any h264 and h265 "
    "playbacks.";

const char kDisableRasterizerCaching[] = "disable_rasterizer_caching";
const char kDisableRasterizerCachingHelp[] =
    "Disables caching of rasterized render tree nodes; caching improves "
    "performance but may result in sub-pixel differences.  Note that this "
    "is deprecated, the '--force_deterministic_rendering' flag should be "
    "used instead which does the same thing.";

const char kDisableSignIn[] = "disable_sign_in";
const char kDisableSignInHelp[] =
    "Disables sign-in on platforms that use H5VCC Account Manager.";

const char kDisableSplashScreenOnReloads[] = "disable_splash_screen_on_reloads";
const char kDisableSplashScreenOnReloadsHelp[] =
    "Disables the splash screen on reloads; instead it will only appear on the "
    "first navigate.";

const char kDisableWebDriver[] = "disable_webdriver";
const char kDisableWebDriverHelp[] = "Do not create the WebDriver server.";

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
    "Enables memory tracking by installing the memory tracker on startup. Run "
    "--memory_tracker=help for more info.";

const char kMinCompatibilityVersion[] = "min_compatibility_version";
const char kMinCompatibilityVersionHelp[] =
    "The minimum version of Cobalt that will be checked during compatibility "
    "validations.";

const char kNullSavegame[] = "null_savegame";
const char kNullSavegameHelp[] =
    "Setting NullSavegame will result in no data being read from previous "
    "sessions and no data being persisted to future sessions. It effectively "
    "makes the app run as if it has no local storage.";

const char kDisablePartialLayout[] = "disable_partial_layout";
const char kDisablePartialLayoutHelp[] =
    "Causes layout to re-compute the boxes for the entire DOM rather than "
    "re-using boxes for elements that have not been invalidated.";

const char kProd[] = "prod";
const char kProdHelp[] =
    "Several checks are not enabled by default in non-production(gold) build. "
    "Use this flag to simulate production build behavior.";

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

const char kUserAgent[] = "user_agent";
const char kUserAgentHelp[] =
    "Specifies a custom user agent for device simulations. The expected "
    "format is \"Mozilla/5.0 ('os_name_and_version') Cobalt/'cobalt_version'."
    "'cobalt_build_version_number'-'build_configuration' (unlike Gecko) "
    "'javascript_engine_version' 'rasterizer_type' 'starboard_version', "
    "'network_operator'_'device_type'_'chipset_model_number'_'model_year'/"
    "'firmware_version' ('brand', 'model', 'connection_type') 'aux_field'\".";

const char kUserAgentClientHints[] = "user_agent_client_hints";
const char kUserAgentClientHintsHelp[] =
    "Specify custom user agent client hints for device simulations. "
    "Configure user agent fields in a string delimited by semicolon ';'. "
    "If semicolon is expected to be in value of any user agent field, "
    "escape the semicolon by prefixing it with backslash '\\'. "
    "Empty field value is allowed. "
    "Example: "
    "--user_agent_client_hints=\"device_type=GAME;"
    "os_name_and_version=Linux armeabi-v7a\\; Android 7.1.2;evergreen_type=\" "
    "The 18 supported UA fields for overriding are: aux_field, brand, "
    "build_configuration, chipset_model_number, cobalt_build_version_number, "
    "cobalt_build_version_number, connection_type, device_type, "
    "evergreen_type,evergreen_version, firmware_version, "
    "javascript_engine_version, model, model_year, "
    "original_design_manufacturer, os_name_and_version, starboard_version, "
    "rasterizer_type";

const char kUserAgentOsNameVersion[] = "user_agent_os_name_version";
const char kUserAgentOsNameVersionHelp[] =
    "Specifies a custom 'os_name_and_version' user agent field with otherwise "
    "default user agent fields. Example: \"X11; Linux x86_64\".";

const char kUseTTS[] = "use_tts";
const char kUseTTSHelp[] =
    "Enable text-to-speech functionality, for platforms that implement the "
    "speech synthesis API. If the platform doesn't have speech synthesis, "
    "TTSLogger will be used instead.";

const char kWebDriverListenIp[] = "webdriver_listen_ip";
const char kWebDriverListenIpHelp[] =
    "IP that the WebDriver server should be listening on. (INADDR_ANY if "
    "unspecified). This is deprecated in favor of --dev_servers_listen_ip (if "
    "both are specified, --webdriver_listen_ip is used).";

const char kWebDriverPort[] = "webdriver_port";
const char kWebDriverPortHelp[] =
    "Port that the WebDriver server should be listening on.";

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
const char kDisableOnScreenKeyboard[] = "disable_on_screen_keyboard";
const char kDisableOnScreenKeyboardHelp[] =
    "Disable the on screen keyboard for testing.";
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

const char kMinLogLevel[] = "min_log_level";
const char kMinLogLevelHelp[] =
    "Set the minimum logging level: info|warning|error|fatal.";
const char kDisableJavaScriptJit[] = "disable_javascript_jit";
const char kDisableJavaScriptJitHelp[] =
    "Specifies that javascript jit should be disabled.";

const char kDisableMapToMesh[] = "disable_map_to_mesh";
const char kDisableMapToMeshHelp[] =
    "Specifies that map to mesh should be disabled. Cobalt maps 360 video "
    "textures onto a sphere in order to project 360 video onto the viewport "
    "by default. Specifying this flag renders 360 degree video unprojected.";

const char kDisableTimerResolutionLimit[] = "disable_timer_resolution_limit";
const char kDisableTimerResolutionLimitHelp[] =
    "By default, window.performance.now() will return values at a clamped "
    "minimum resolution of 20us.  By specifying this flag, the limit will be "
    "removed and the resolution will be 1us (or larger depending on the "
    "platform.";

const char kDisableUpdaterModule[] = "disable_updater_module";
const char kDisableUpdaterModuleHelp[] =
    "Disables the Cobalt Evergreen UpdaterModule which is responsible for "
    "downloading and installing new Cobalt updates. Passing the flag is "
    "equivalent to opting out from further updates.";

const char kEncodedImageCacheSizeInBytes[] =
    "encoded_image_cache_size_in_bytes";
const char kEncodedImageCacheSizeInBytesHelp[] =
    "Determines the capacity of the encoded image cache which manages encoded "
    "images downloaded from a web page. The cache uses CPU memory.";

const char kForceMigrationForStoragePartitioning[] =
    "force_migration_for_storage_partitioning";
const char kForceMigrationForStoragePartitioningHelp[] =
    "Overrides the default storage migration policy when upgrading to "
    "partitioned storage and forces data migration regardless of the"
    "initial app url. The default policy is to migrate data only for"
    "https://www.youtube.com/tv.";

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
    "surfaces downloaded from a web page.  While it depends on the platform, "
    "often (and ideally) these images are cached within GPU memory.";

const char kInitialURL[] = "url";
const char kInitialURLHelp[] =
    "Setting this switch defines the startup URL that Cobalt will use.  If no "
    "value is set, a default URL will be used.";

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

const char kOmitDeviceAuthenticationQueryParameters[] =
    "omit_device_authentication_query_parameters";
const char kOmitDeviceAuthenticationQueryParametersHelp[] =
    "When set, no device authentication parameters will be appended to the"
    "initial URL.";

const char kProxy[] = "proxy";
const char kProxyHelp[] =
    "Specifies a proxy to use for network connections. "
    "See comments in net::ProxyRules::ParseFromString() for more information. "
    "If you do not explicitly provide a scheme when providing the proxy server "
    "URL, it will default to HTTP.  So for example, for a HTTPS proxy you "
    "would want to specify '--proxy=\"https=https://localhost:443\"' instead "
    "of '--proxy=\"https=localhost:443\"'.";

const char kQrCodeOverlay[] = "qr_code_overlay";
const char kQrCodeOverlayHelp[] =
    "Display QrCode based overlay information. These information can be used"
    " for performance tuning or playback quality check.  By default qr code"
    " will be displayed in 4 different locations on the screen alternatively,"
    " and the number of locations can be overwritten by specifying it as the "
    " value of the command line parameter, like '--qr_code_overlay=6'.";

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

const char kSilenceInlineScriptWarnings[] = "silence_inline_script_warnings";
const char kSilenceInlineScriptWarningsHelp[] =
    "Prevents Cobalt from logging warnings when it encounters a non-async "
    "<script> tag inlined within HTML.  Cobalt fails to deal with these "
    "resources properly when suspending or resuming, so the warning usually "
    "indicates a bug if the web app intends to support suspending and resuming "
    "properly.";

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

const char kFallbackSplashScreenTopics[] = "fallback_splash_screen_topics";
const char kFallbackSplashScreenTopicsHelp[] =
    "Setting this switch defines a mapping of URL 'topics' to splash screen "
    "URLs or filenames that Cobalt will use in the absence of a web cache, "
    "(for example, music=music_splash_screen.html&foo=file:///bar.html). If a "
    "URL is given it should match the format of 'fallback_splash_screen_url'. "
    "A given filename should exist in the same directory as "
    "'fallback_splash_screen_url'. If no fallback url exists for the topic of "
    "the URL used to launch Cobalt, then the value of "
    "'fallback_splash_screen_url' will be used.";

const char kUseQAUpdateServer[] = "use_qa_update_server";
const char kUseQAUpdateServerHelp[] =
    "Uses the QA update server to test the changes to the configuration of the "
    "PROD update server.";

const char kVersion[] = "version";
const char kVersionHelp[] = "Prints the current version of Cobalt";

const char kViewport[] = "viewport";
const char kViewportHelp[] =
    "Specifies the viewport size: "
    "width ['x' height ['x' screen_diagonal_inches]]";

const char kVideoPlaybackRateMultiplier[] = "video_playback_rate_multiplier";
const char kVideoPlaybackRateMultiplierHelp[] =
    "Specifies the multiplier of video playback rate.  Set to a value greater "
    "than 1.0 to play video faster.  Set to a value less than 1.0 to play "
    "video slower.";

std::string HelpMessage() {
  std::string help_message;
  std::map<std::string, const char*> help_map {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
    {kDebugConsoleMode, kDebugConsoleModeHelp},
        {kDevServersListenIp, kDevServersListenIpHelp},
#if defined(ENABLE_DEBUGGER)
        {kWaitForWebDebugger, kWaitForWebDebuggerHelp},
        {kRemoteDebuggingPort, kRemoteDebuggingPortHelp},
#endif  // ENABLE_DEBUGGER
        {kDisableImageAnimations, kDisableImageAnimationsHelp},
        {kForceDeterministicRendering, kForceDeterministicRenderingHelp},
        {kDisableMediaCodecs, kDisableMediaCodecsHelp},
        {kDisableRasterizerCaching, kDisableRasterizerCachingHelp},
        {kDisableSignIn, kDisableSignInHelp},
        {kDisableSplashScreenOnReloads, kDisableSplashScreenOnReloadsHelp},
        {kDisableWebDriver, kDisableWebDriverHelp},
        {kExtraWebFileDir, kExtraWebFileDirHelp},
        {kFakeMicrophone, kFakeMicrophoneHelp},
        {kIgnoreCertificateErrors, kIgnoreCertificateErrorsHelp},
        {kInputFuzzer, kInputFuzzerHelp}, {kMemoryTracker, kMemoryTrackerHelp},
        {kMinCompatibilityVersion, kMinCompatibilityVersionHelp},
        {kNullSavegame, kNullSavegameHelp},
        {kDisablePartialLayout, kDisablePartialLayoutHelp}, {kProd, kProdHelp},
        {kRequireCSP, kRequireCSPHelp},
        {kRequireHTTPSLocation, kRequireHTTPSLocationHelp},
        {kShutdownAfter, kShutdownAfterHelp},
        {kStubImageDecoder, kStubImageDecoderHelp},
        {kSuspendFuzzer, kSuspendFuzzerHelp}, {kTimedTrace, kTimedTraceHelp},
        {kUserAgent, kUserAgentHelp},
        {kUserAgentClientHints, kUserAgentClientHintsHelp},
        {kUserAgentOsNameVersion, kUserAgentOsNameVersionHelp},
        {kUseTTS, kUseTTSHelp}, {kWebDriverListenIp, kWebDriverListenIpHelp},
        {kWebDriverPort, kWebDriverPortHelp},
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
        {kDisableOnScreenKeyboard, kDisableOnScreenKeyboardHelp},
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES
        {kDisableJavaScriptJit, kDisableJavaScriptJitHelp},
        {kDisableMapToMesh, kDisableMapToMeshHelp},
        {kDisableTimerResolutionLimit, kDisableTimerResolutionLimitHelp},
        {kDisableUpdaterModule, kDisableUpdaterModuleHelp},
        {kEncodedImageCacheSizeInBytes, kEncodedImageCacheSizeInBytesHelp},
        {kForceMigrationForStoragePartitioning,
         kForceMigrationForStoragePartitioningHelp},
        {kFPSPrint, kFPSPrintHelp}, {kFPSOverlay, kFPSOverlayHelp},
        {kHelp, kHelpHelp},
        {kImageCacheSizeInBytes, kImageCacheSizeInBytesHelp},
        {kInitialURL, kInitialURLHelp},
        {kLocalStoragePartitionUrl, kLocalStoragePartitionUrlHelp},
        {kMaxCobaltCpuUsage, kMaxCobaltCpuUsageHelp},
        {kMaxCobaltGpuUsage, kMaxCobaltGpuUsageHelp},
        {kMinLogLevel, kMinLogLevelHelp},
        {kOffscreenTargetCacheSizeInBytes,
         kOffscreenTargetCacheSizeInBytesHelp},
        {kOmitDeviceAuthenticationQueryParameters,
         kOmitDeviceAuthenticationQueryParametersHelp},
        {kProxy, kProxyHelp}, {kQrCodeOverlay, kQrCodeOverlayHelp},
        {kRemoteTypefaceCacheSizeInBytes, kRemoteTypefaceCacheSizeInBytesHelp},
        {kRetainRemoteTypefaceCacheDuringSuspend,
         kRetainRemoteTypefaceCacheDuringSuspendHelp},
        {kScratchSurfaceCacheSizeInBytes, kScratchSurfaceCacheSizeInBytesHelp},
        {kSilenceInlineScriptWarnings, kSilenceInlineScriptWarningsHelp},
        {kSkiaCacheSizeInBytes, kSkiaCacheSizeInBytesHelp},
        {kSkiaTextureAtlasDimensions, kSkiaTextureAtlasDimensionsHelp},
        {kSoftwareSurfaceCacheSizeInBytes,
         kSoftwareSurfaceCacheSizeInBytesHelp},
        {kFallbackSplashScreenURL, kFallbackSplashScreenURLHelp},
        {kUseQAUpdateServer, kUseQAUpdateServerHelp}, {kVersion, kVersionHelp},
        {kViewport, kViewportHelp},
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
