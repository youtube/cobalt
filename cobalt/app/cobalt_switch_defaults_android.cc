// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/app/cobalt_switch_defaults.h"

#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "cobalt/browser/switches.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"
#include "content/common/features.h"  // nogncheck
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"

void ApplyDefaultCommandLineSwitches() {
  cobalt::CommandLinePreprocessor::ApplyDefaults(
      base::CommandLine::ForCurrentProcess());
}

namespace cobalt {

const std::vector<const char*>&
CommandLinePreprocessor::GetCobaltToggleSwitches() {
  // ==========
  // IMPORTANT:
  //
  // These command line switches defaults affect AndroidTV platforms. If you are
  // making changes to these values, please check that other platforms (such as
  // Linux/Evergreen) are getting corresponding updates.

  static const std::vector<const char*> kCobaltToggleSwitches{
      // Run Cobalt as a single process.
      ::switches::kSingleProcess,
      // Enable Blink to work in overlay video mode.
      ::switches::kForceVideoOverlays,
      // Disables RGBA_4444 textures which causes rendering artifacts when
      // low-end-device-mode is enabled.
      blink::switches::kDisableRGBA4444Textures,
      // Disable Chrome's accelerated video encoding and decoding (Cobalt uses
      // Starboard's stack).
      ::switches::kDisableAcceleratedVideoDecode,
      ::switches::kDisableAcceleratedVideoEncode,
      // Hide scrollbars to avoid memory allocation.
      ::switches::kHideScrollbars,
      // Use hermetic custom fonts.xml for Skia to avoid scanning OS fonts on
      // startup.
      ::switches::kUseCustomAndroidFontsXml,
  };
  return kCobaltToggleSwitches;
}

const base::CommandLine::SwitchMap&
CommandLinePreprocessor::GetCobaltParamSwitchDefaults() {
  static const base::CommandLine::SwitchMap kCobaltSwitchDefaults{
      // Autoplay video with url.
      {::switches::kAutoplayPolicy,
       ::switches::autoplay::kNoUserGestureRequiredPolicy},
      // Set default raster threads to 2 for smoother performance.
      {::switches::kNumRasterThreads, "2"},
#if !defined(ARCH_CPU_ARM64)
      // Enforce ANGLE to use GLES backend by default on Android platforms
      // excluding arm64.
      {::switches::kUseANGLE, gl::kANGLEImplementationOpenGLESName},
#endif
      // Limit the HTTP disk cache to 25 MiB (25 * 1024 * 1024 bytes).
      {switches::kMaxHttpCacheSize, "26214400"},
      {blink::switches::kJavaScriptFlags,
       // Disable decommitting pooled pages to prevent virtual memory
       // fragmentation.
       "--no-decommit-pooled-pages,"
       // Disable v8 concurrent marking by default.
       "--no-concurrent-marking"},
      // Enable precise memory info so we can make accurate client-side
      // measurements.
      {::switches::kEnableBlinkFeatures, "PreciseMemoryInfo"},
      {::switches::kEnableFeatures,
       base::JoinString(
           {
               // Pass javascript console log to adb log.
               features::kLogJsConsoleMessages.name,
#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
               // It is important to use a feature override instead of the
               // rendering switch, to make sure certain devices are excluded.
               features::kDefaultPassthroughCommandDecoder.name,
#endif
               // Compositor switches to reduce prepaint tile cache size.
               features::kSmallerInterestArea.name,
               features::kReclaimPrepaintTilesWhenIdle.name,
               features::kReclaimOldPrepaintTiles.name,
               blink::features::kWebAudioRemoveAudioDestinationResampler.name,
               // Commit localStorage/sessionStorage writes at the end of the JS
               // task that made them so recent writes survive process
               // termination.
               storage::kDomStorageSmartFlushing.name,
           },
           ",")},
      {::switches::kDisableFeatures,
       base::JoinString(
           {
               // Disable BackupRefPtr and have shim allocator tracked by
               // MemoryReclaimer.
               base::features::kPartitionAllocBackupRefPtr.name,
               // Disable AAudio to make the microphone use OpenSL ES.
               // OpenSL ES supports seamless switching to virtual microphones
               // like AtvRemote. For details, see http://b/478022126#comment6.
               features::kUseAAudioInput.name,
               // Disable FontSrcLocalMatching lookup table.
               features::kFontSrcLocalMatching.name,
               // Disable deferring audio focus until audible for
               // Starboard/Cobalt:
               // 1. Cobalt runs on living-room/TV devices where playback is
               // user-initiated and should acquire audio focus immediately.
               // 2. Starboard renders audio directly to native audio sinks,
               // bypassing Chromium's AudioStreamMonitor, so WebContents is
               // never reported as audible, meaning deferred audio focus is
               // never executed.
               media::kDeferAudioFocusUntilAudible.name,
           },
           ",")},
  };
  return kCobaltSwitchDefaults;
}

}  // namespace cobalt
