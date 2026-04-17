// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/shell/common/shell_switches.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "sandbox/policy/switches.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gl/gl_switches.h"

namespace cobalt {

const std::vector<const char*>&
CommandLinePreprocessor::GetCobaltToggleSwitches() {
  static const std::vector<const char*> kCobaltToggleSwitches{
      // Enable Blink to work in overlay video mode
      ::switches::kForceVideoOverlays,
      // Disable multiprocess mode.
      ::switches::kSingleProcess,
      // Hide content shell toolbar.
      ::switches::kContentShellHideToolbar,
      // Accelerated GL is blanket disabled for Linux. Ignore the GPU blocklist
      // to enable it.
      ::switches::kIgnoreGpuBlocklist,
      // Disable Zygote (a process fork utility); in turn needs sandbox
      // disabled.
      ::switches::kNoZygote,
      sandbox::policy::switches::kNoSandbox,
      // Rasterize Tiles directly to GPU memory (ZeroCopyRasterBufferProvider).
      blink::switches::kEnableZeroCopy,
      // For Starboard the signal handlers are already setup. Disable the
      // Chromium registrations to avoid overriding the Starboard ones.
      ::switches::kDisableInProcessStackTraces,
      // Cobalt doesn't use Chrome's accelerated video decoding/encoding.
      ::switches::kDisableAcceleratedVideoDecode,
      ::switches::kDisableAcceleratedVideoEncode,
  };

  return kCobaltToggleSwitches;
}

const base::CommandLine::SwitchMap&
CommandLinePreprocessor::GetCobaltParamSwitchDefaults() {
  static const base::CommandLine::SwitchMap kCobaltSwitchDefaults({
      // Use passthrough command decoder.
      {::switches::kUseCmdDecoder, "passthrough"},
      // Set the default size for the content shell/starboard window.
      {::switches::kContentShellHostWindowSize, "1920x1080"},
      // Enable remote Devtools access.
      {::switches::kRemoteDebuggingPort, "9222"},
      {::switches::kRemoteAllowOrigins, "http://localhost:9222"},
      // kEnableLowEndDeviceMode sets MSAA to 4 (and not 8, the default). But
      // we set it explicitly just in case.
      {blink::switches::kGpuRasterizationMSAASampleCount, "4"},
      // Enable precise memory info so we can make accurate client-side
      // measurements.
      {::switches::kEnableBlinkFeatures, "PreciseMemoryInfo"},
      // Enable autoplay video/audio, as Cobalt may launch directly into media
      // playback before user interaction.
      {::switches::kAutoplayPolicy, "no-user-gesture-required"},
  });
  return kCobaltSwitchDefaults;
}

}  // namespace cobalt
