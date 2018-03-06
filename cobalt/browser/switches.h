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

#include <string>

#ifndef COBALT_BROWSER_SWITCHES_H_
#define COBALT_BROWSER_SWITCHES_H_

namespace cobalt {
namespace browser {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
extern const char kAudioDecoderStub[];
extern const char kAudioDecoderStubHelp[];
extern const char kDebugConsoleMode[];
extern const char kDebugConsoleModeHelp[];
extern const char kDisableImageAnimations[];
extern const char kDisableImageAnimationsHelp[];
extern const char kDisableRasterizerCaching[];
extern const char kDisableSignIn[];
extern const char kDisableSignInHelp[];
extern const char kDisableSplashScreenOnReloads[];
extern const char kDisableSplashScreenOnReloadsHelp[];
extern const char kDisableWebDriver[];
extern const char kDisableWebDriverHelp[];
extern const char kDisableWebmVp9[];
extern const char kDisableWebmVp9Help[];
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
extern const char kMinLogLevel[];
extern const char kMinLogLevelHelp[];
extern const char kNullAudioStreamer[];
extern const char kNullAudioStreamerHelp[];
extern const char kNullSavegame[];
extern const char kNullSavegameHelp[];
extern const char kPartialLayout[];
extern const char kPartialLayoutHelp[];
extern const char kProd[];
extern const char kProdHelp[];
extern const char kProxy[];
extern const char kProxyHelp[];
extern const char kRemoteDebuggingPort[];
extern const char kRemoteDebuggingPortHelp[];
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
extern const char kUseTTS[];
extern const char kUseTTSHelp[];
extern const char kVideoContainerSizeOverride[];
extern const char kVideoContainerSizeOverrideHelp[];
extern const char kVideoDecoderStub[];
extern const char kVideoDecoderStubHelp[];
extern const char kWebDriverListenIp[];
extern const char kWebDriverListenIpHelp[];
extern const char kWebDriverPort[];
extern const char kWebDriverPortHelp[];
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

extern const char kDisableJavaScriptJit[];
extern const char kDisableJavaScriptJitHelp[];
extern const char kEnableMapToMeshRectanglar[];
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
extern const char kJavaScriptGcThresholdInBytes[];
extern const char kJavaScriptGcThresholdInBytesHelp[];
extern const char kLocalStoragePartitionUrl[];
extern const char kLocalStoragePartitionUrlHelp[];
extern const char kMaxCobaltCpuUsage[];
extern const char kMaxCobaltCpuUsageHelp[];
extern const char kMaxCobaltGpuUsage[];
extern const char kMaxCobaltGpuUsageHelp[];
extern const char kOffscreenTargetCacheSizeInBytes[];
extern const char kOffscreenTargetCacheSizeInBytesHelp[];
extern const char kReduceCpuMemoryBy[];
extern const char kReduceCpuMemoryByHelp[];
extern const char kReduceGpuMemoryBy[];
extern const char kReduceGpuMemoryByHelp[];
extern const char kRemoteTypefaceCacheSizeInBytes[];
extern const char kRemoteTypefaceCacheSizeInBytesHelp[];
extern const char kRetainRemoteTypefaceCacheDuringSuspend[];
extern const char kRetainRemoteTypefaceCacheDuringSuspendHelp[];
extern const char kScratchSurfaceCacheSizeInBytes[];
extern const char kScratchSurfaceCacheSizeInBytesHelp[];
extern const char kSkiaCacheSizeInBytes[];
extern const char kSkiaCacheSizeInBytesHelp[];
extern const char kSkiaTextureAtlasDimensions[];
extern const char kSkiaTextureAtlasDimensionsHelp[];
extern const char kSoftwareSurfaceCacheSizeInBytes[];
extern const char kSoftwareSurfaceCacheSizeInBytesHelp[];
extern const char kFallbackSplashScreenURL[];
extern const char kFallbackSplashScreenURLHelp[];
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
