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

#include <string>

#include "starboard/configuration.h"

#ifndef COBALT_BROWSER_SWITCHES_H_
#define COBALT_BROWSER_SWITCHES_H_

namespace cobalt {
namespace browser {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
extern const char kDebugConsoleMode[];
extern const char kDebugConsoleModeHelp[];
extern const char kDevServersListenIp[];
extern const char kDevServersListenIpHelp[];

#if defined(ENABLE_DEBUGGER)
extern const char kRemoteDebuggingPort[];
extern const char kRemoteDebuggingPortHelp[];
extern const char kWaitForWebDebugger[];
extern const char kWaitForWebDebuggerHelp[];
#endif  // ENABLE_DEBUGGER

extern const char kDisableImageAnimations[];
extern const char kDisableImageAnimationsHelp[];
extern const char kForceDeterministicRendering[];
extern const char kDisableMediaCodecs[];
extern const char kDisableMediaCodecsHelp[];
extern const char kDisableRasterizerCaching[];
extern const char kDisableSignIn[];
extern const char kDisableSignInHelp[];
extern const char kDisableSplashScreenOnReloads[];
extern const char kDisableSplashScreenOnReloadsHelp[];
extern const char kDisableWebDriver[];
extern const char kDisableWebDriverHelp[];
extern const char kExtraWebFileDir[];
extern const char kExtraWebFileDirHelp[];
extern const char kFakeMicrophone[];
extern const char kFakeMicrophoneHelp[];
extern const char kIgnoreCertificateErrors[];
extern const char kIgnoreCertificateErrorsHelp[];
extern const char kInputFuzzer[];
extern const char kInputFuzzerHelp[];
extern const char kMemoryTracker[];
extern const char kMemoryTrackerHelp[];
extern const char kMinCompatibilityVersion[];
extern const char kMinCompatibilityVersionHelp[];
extern const char kNullSavegame[];
extern const char kNullSavegameHelp[];
extern const char kDisablePartialLayout[];
extern const char kDisablePartialLayoutHelp[];
extern const char kProd[];
extern const char kProdHelp[];
extern const char kRequireCSP[];
extern const char kRequireCSPHelp[];
extern const char kRequireHTTPSLocation[];
extern const char kRequireHTTPSLocationHelp[];
extern const char kShutdownAfter[];
extern const char kShutdownAfterHelp[];
extern const char kStubImageDecoder[];
extern const char kStubImageDecoderHelp[];
extern const char kSuspendFuzzer[];
extern const char kSuspendFuzzerHelp[];
extern const char kTimedTrace[];
extern const char kTimedTraceHelp[];
extern const char kUserAgentClientHints[];
extern const char kUserAgentClienthintsHelp[];
extern const char kUserAgentOsNameVersion[];
extern const char kUserAgentOsNameVersionHelp[];
extern const char kUseTTS[];
extern const char kUseTTSHelp[];
extern const char kWebDriverListenIp[];
extern const char kWebDriverListenIpHelp[];
extern const char kWebDriverPort[];
extern const char kWebDriverPortHelp[];

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
extern const char kDisableOnScreenKeyboard[];
extern const char kDisableOnScreenKeyboardHelp[];
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

extern const char kDisableJavaScriptJit[];
extern const char kDisableJavaScriptJitHelp[];
extern const char kDisableMapToMesh[];
extern const char kDisableMapToMeshHelp[];
extern const char kDisableTimerResolutionLimit[];
extern const char kDisableTimerResolutionLimitHelp[];
extern const char kDisableUpdaterModule[];
extern const char kDisableUpdaterModuleHelp[];
extern const char kEncodedImageCacheSizeInBytes[];
extern const char kEncodedImageCacheSizeInBytesHelp[];
extern const char kForceMigrationForStoragePartitioning[];
extern const char kFPSPrint[];
extern const char kFPSPrintHelp[];
extern const char kFPSOverlay[];
extern const char kFPSOverlayHelp[];
extern const char kHelp[];
extern const char kHelpHelp[];
extern const char kImageCacheSizeInBytes[];
extern const char kImageCacheSizeInBytesHelp[];
extern const char kInitialURL[];
extern const char kInitialURLHelp[];
extern const char kLocalStoragePartitionUrl[];
extern const char kLocalStoragePartitionUrlHelp[];
extern const char kMaxCobaltCpuUsage[];
extern const char kMaxCobaltCpuUsageHelp[];
extern const char kMaxCobaltGpuUsage[];
extern const char kMaxCobaltGpuUsageHelp[];
extern const char kMinLogLevel[];
extern const char kMinLogLevelHelp[];
extern const char kOffscreenTargetCacheSizeInBytes[];
extern const char kOffscreenTargetCacheSizeInBytesHelp[];
extern const char kOmitDeviceAuthenticationQueryParameters[];
extern const char kOmitDeviceAuthenticationQueryParametersHelp[];
extern const char kProxy[];
extern const char kProxyHelp[];
extern const char kQrCodeOverlay[];
extern const char kQrCodeOverlayHelp[];
extern const char kRemoteTypefaceCacheSizeInBytes[];
extern const char kRemoteTypefaceCacheSizeInBytesHelp[];
extern const char kRetainRemoteTypefaceCacheDuringSuspend[];
extern const char kRetainRemoteTypefaceCacheDuringSuspendHelp[];
extern const char kScratchSurfaceCacheSizeInBytes[];
extern const char kScratchSurfaceCacheSizeInBytesHelp[];
extern const char kSilenceInlineScriptWarnings[];
extern const char kSilenceInlineScriptWarningsHelp[];
extern const char kSkiaCacheSizeInBytes[];
extern const char kSkiaCacheSizeInBytesHelp[];
extern const char kSkiaTextureAtlasDimensions[];
extern const char kSkiaTextureAtlasDimensionsHelp[];
extern const char kSoftwareSurfaceCacheSizeInBytes[];
extern const char kSoftwareSurfaceCacheSizeInBytesHelp[];
extern const char kFallbackSplashScreenURL[];
extern const char kFallbackSplashScreenURLHelp[];
extern const char kFallbackSplashScreenTopics[];
extern const char kFallbackSplashScreenTopicsHelp[];
extern const char kUseQAUpdateServer[];
extern const char kUseQAUpdateServerHelp[];
extern const char kVersion[];
extern const char kVersionHelp[];
extern const char kViewport[];
extern const char kViewportHelp[];
extern const char kVideoPlaybackRateMultiplier[];
extern const char kVideoPlaybackRateMultiplierHelp[];

std::string HelpMessage();

}  // namespace switches
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SWITCHES_H_
