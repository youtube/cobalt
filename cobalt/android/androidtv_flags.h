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

#ifndef COBALT_ANDROID_ANDROIDTV_FLAGS_H_
#define COBALT_ANDROID_ANDROIDTV_FLAGS_H_

// Experiment keys provided by the Android TV app embedding Cobalt via
// CobaltActivity.getJavaSwitches(), and expanded into Chromium/Cobalt switches
// and features by ExpandAndroidTvFlags() (cobalt/android/androidtv_flag_map.h).
// On the Java side these keys are called JavaSwitches for historical reasons.
// Java constants (AndroidTvFlags.java) are generated from this file.
namespace androidtv_flags {

// keep-sorted start newline_separated=yes
inline constexpr char kAllowCriticalMemoryPressureHandlingInForeground[] =
    "AllowCriticalMemoryPressureHandlingInForeground";

inline constexpr char kAreaBasedVideoBufferBudget[] =
    "AreaBasedVideoBufferBudget";

inline constexpr char kAvoidCcReuseResource[] = "AvoidCCReuseResource";

inline constexpr char kCobaltBypassHtmlPreloadScanner[] =
    "CobaltBypassHTMLPreloadScanner";

inline constexpr char kCobaltBypassResourceLoadScheduler[] =
    "CobaltBypassResourceLoadScheduler";

inline constexpr char kCobaltDynamicMojoPipeMediaSize[] =
    "CobaltDynamicMojoPipeMediaSize";

inline constexpr char kCobaltDynamicMojoPipeSubresourceSize[] =
    "CobaltDynamicMojoPipeSubresourceSize";

inline constexpr char kDecodedImageWorkingSetBudgetBytes[] =
    "DecodedImageWorkingSetBudgetBytes";

inline constexpr char kDisableGpuMemoryBufferCompositorResources[] =
    "DisableGpuMemoryBufferCompositorResources";

inline constexpr char kDisableLessAggressiveParkableString[] =
    "DisableLessAggressiveParkableString";

inline constexpr char kEnableActivityLifecycleCoordination[] =
    "EnableActivityLifecycleCoordination";

inline constexpr char kEnableCobaltContentLengthAwareMojoPipeSizing[] =
    "EnableCobaltContentLengthAwareMojoPipeSizing";

inline constexpr char kEnableCobaltDynamicMojoPipeSizing[] =
    "EnableCobaltDynamicMojoPipeSizing";

inline constexpr char kEnableCobaltMmapFontCache[] =
    "EnableCobaltMmapFontCache";

inline constexpr char kEnableModerateMemoryPressure[] =
    "EnableModerateMemoryPressure";

inline constexpr char kEnableQuic[] = "EnableQUIC";

inline constexpr char kEnableScalingClippedImages[] =
    "EnableScalingClippedImages";

inline constexpr char kEvictMemoryCacheOnCriticalMemoryPressure[] =
    "EvictMemoryCacheOnCriticalMemoryPressure";

// Forces a 720p UI (--force-device-scale-factor=1.5) when the device is
// eligible (1 GB of RAM or less and a display of at least 1080p), as evaluated
// by ExpandAndroidTvFlags() via AndroidTvDeviceInfo.
inline constexpr char kForce720pUiOn1GbDevices[] = "Force720pUiOn1GbDevices";

inline constexpr char kForceGpuMemAvailableMb[] = "ForceGpuMemAvailableMb";

inline constexpr char kGpuImageCacheLimitItems[] = "GpuImageCacheLimitItems";

inline constexpr char kInterestAreaSizeInPixels[] = "InterestAreaSizeInPixels";

inline constexpr char kLimitImageDecodeCacheSizeMb[] =
    "LimitImageDecodeCacheSizeMb";

inline constexpr char kMemoryPressureCooldownInSeconds[] =
    "MemoryPressureCooldownInSeconds";

inline constexpr char kReclaimDelayInSeconds[] = "ReclaimDelayInSeconds";

inline constexpr char kUseMinorMsForMinorGc[] = "UseMinorMSForMinorGC";

inline constexpr char kUseStarboardLifecycle[] = "UseStarboardLifeCycle";

inline constexpr char kV8DisableSparkplug[] = "V8DisableSparkplug";

inline constexpr char kV8InitialOldSpaceSize[] = "V8InitialOldSpaceSize";

inline constexpr char kV8MaxOldSpaceSize[] = "V8MaxOldSpaceSize";

inline constexpr char kV8SetBytecodeOldTime[] = "V8SetBytecodeOldTime";
// keep-sorted end

}  // namespace androidtv_flags

#endif  // COBALT_ANDROID_ANDROIDTV_FLAGS_H_
