/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/browser/switches.h"

namespace cobalt {
namespace browser {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
// Allow insecure HTTP network connections.
const char kAllowHttp[] = "allow_http";

// Decode audio data using ShellRawAudioDecoderStub.
const char kAudioDecoderStub[] = "audio_decoder_stub";

// Set the content security policy enforcement mode: disable | enable
// disable: Allow all resource loads. Ignore CSP totally.
// enable: default mode. Enforce CSP strictly.  Require CSP headers or fail
// the initial document load.
const char kCspMode[] = "csp_mode";

// Switches different debug console modes: on | hud | off
const char kDebugConsoleMode[] = "debug_console";

// Disable webm/vp9.
const char kDisableWebmVp9[] = "disable_webm_vp9";

// Create WebDriver server.
const char kEnableWebDriver[] = "enable_webdriver";

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

// Specifies a proxy to use for network connections.
const char kProxy[] = "proxy";

// Switches partial layout: on | off
const char kPartialLayout[] = "partial_layout";

// Creates a remote debugging server and listens on the specified port.
const char kRemoteDebuggingPort[] = "remote_debugging_port";

// If this flag is set, Cobalt will automatically shutdown after the specified
// number of seconds have passed.
const char kShutdownAfter[] = "shutdown_after";

// Decode all images using StubImageDecoder.
const char kStubImageDecoder[] = "stub_image_decoder";

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

// Port that the WebDriver server should be listening on.
const char kWebDriverPort[] = "webdriver_port";

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

// Determines the capacity of the image cache which manages image surfaces
// downloaded from a web page.  While it depends on the platform, often (and
// ideally) these images are cached within GPU memory.
const char kImageCacheSizeInBytes[] = "image_cache_size_in_bytes";

// Setting this switch defines the startup URL that Cobalt will use.  If no
// value is set, a default URL will be used.
const char kInitialURL[] = "url";

// Determines the capacity of the remote typefaces cache which manages all
// typefaces downloaded from a web page.
const char kRemoteTypefaceCacheSizeInBytes[] =
    "remote_typeface_cache_size_in_bytes";

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

// Determines the capacity of the surface cache.  The surface cache tracks which
// render tree nodes are being re-used across frames and stores the nodes that
// are most CPU-expensive to render into surfaces.  While it depends on the
// platform, this setting may affect GPU memory usage.
const char kSurfaceCacheSizeInBytes[] = "surface_cache_size_in_bytes";

// Specifies the viewport size: width ['x' height]
const char kViewport[] = "viewport";

}  // namespace switches
}  // namespace browser
}  // namespace cobalt
