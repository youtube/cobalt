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
namespace cobalt::androidtv_flags {

// keep-sorted start newline_separated=yes
// Allows critical memory pressure handling in foreground for V8.
// See b/521848285.
inline constexpr char kAllowCriticalMemoryPressureHandlingInForeground[] =
    "AllowCriticalMemoryPressureHandlingInForeground";

// Enables area-based video decoder buffer budget calculation.
// See b/520049429.
inline constexpr char kAreaBasedVideoBufferBudget[] =
    "AreaBasedVideoBufferBudget";

// Avoids reusing cc layer tree resources.
// See b/424591313.
inline constexpr char kAvoidCcReuseResource[] = "AvoidCCReuseResource";

// Bypasses Blink HTMLPreloadScanner and HTMLResourcePreloader.
// See b/545695147.
inline constexpr char kCobaltBypassHtmlPreloadScanner[] =
    "CobaltBypassHTMLPreloadScanner";

// Bypasses ResourceLoadScheduler subresource queueing and throttling.
// See b/545658521.
inline constexpr char kCobaltBypassResourceLoadScheduler[] =
    "CobaltBypassResourceLoadScheduler";

// Tunes dynamic Mojo data pipe size in bytes for media requests.
// See b/520888239.
inline constexpr char kCobaltDynamicMojoPipeMediaSize[] =
    "CobaltDynamicMojoPipeMediaSize";

// Tunes dynamic Mojo data pipe size in bytes for subresources.
// See b/520888239.
inline constexpr char kCobaltDynamicMojoPipeSubresourceSize[] =
    "CobaltDynamicMojoPipeSubresourceSize";

// Limits GPU decoded image working set budget in bytes.
// See b/512157288.
inline constexpr char kDecodedImageWorkingSetBudgetBytes[] =
    "DecodedImageWorkingSetBudgetBytes";

// Disables GPU memory buffer compositor resources.
// See b/515159109.
inline constexpr char kDisableGpuMemoryBufferCompositorResources[] =
    "DisableGpuMemoryBufferCompositorResources";

// Disables LessAggressiveParkableString to unpause foreground compression and
// use a 2-second aging interval.
// See b/544754813.
inline constexpr char kDisableLessAggressiveParkableString[] =
    "DisableLessAggressiveParkableString";

// Coordinates Activity lifecycle and safe surface teardown across overlapping
// activities.
// See b/528367182.
inline constexpr char kEnableActivityLifecycleCoordination[] =
    "EnableActivityLifecycleCoordination";

// Shrinks Mojo data pipes to the response Content-Length when known.
// See b/520888239.
inline constexpr char kEnableCobaltContentLengthAwareMojoPipeSizing[] =
    "EnableCobaltContentLengthAwareMojoPipeSizing";

// Enables dynamic Mojo data pipe sizing.
// See b/520888239.
inline constexpr char kEnableCobaltDynamicMojoPipeSizing[] =
    "EnableCobaltDynamicMojoPipeSizing";

// Enables mmap-backed WOFF2 font decompression disk cache.
// See b/544754138.
inline constexpr char kEnableCobaltMmapFontCache[] =
    "EnableCobaltMmapFontCache";

// Enables moderate memory pressure handling on Android.
// See b/521848285.
inline constexpr char kEnableModerateMemoryPressure[] =
    "EnableModerateMemoryPressure";

// Enables QUIC for network requests by passing --enable-quic, which suppresses
// the default --disable-quic (cobalt/app/cobalt_switch_defaults_android.cc).
// See b/467778593.
inline constexpr char kEnableQuic[] = "EnableQUIC";

// Allows scaling clipped images in GpuImageDecodeCache.
// See b/512157288.
inline constexpr char kEnableScalingClippedImages[] =
    "EnableScalingClippedImages";

// Evicts Blink MemoryCache on critical memory pressure.
// See b/521848285.
inline constexpr char kEvictMemoryCacheOnCriticalMemoryPressure[] =
    "EvictMemoryCacheOnCriticalMemoryPressure";

// Overrides available GPU memory in MB; defaults to 64 MB on 32-bit builds.
// See b/549674099.
inline constexpr char kForceGpuMemAvailableMb[] = "ForceGpuMemAvailableMb";

// Limits the number of items in the GPU image cache.
// See b/512157288.
inline constexpr char kGpuImageCacheLimitItems[] = "GpuImageCacheLimitItems";

// Tunes compositor offscreen interest area size in pixels via
// SmallerInterestArea.
// See b/512154139.
inline constexpr char kInterestAreaSizeInPixels[] = "InterestAreaSizeInPixels";

// Caps software/GPU image decode cache size in MB.
// See b/545216717.
inline constexpr char kLimitImageDecodeCacheSizeMb[] =
    "LimitImageDecodeCacheSizeMb";

// Configures memory pressure throttling cooldown in seconds.
// See b/521848285.
inline constexpr char kMemoryPressureCooldownInSeconds[] =
    "MemoryPressureCooldownInSeconds";

// Tunes delay in seconds before reclaiming prepaint tiles when idle via
// SmallerInterestArea.
// See b/512154139.
inline constexpr char kReclaimDelayInSeconds[] = "ReclaimDelayInSeconds";

// Enables V8 MinorMS for minor GC.
// See b/522958422.
inline constexpr char kUseMinorMsForMinorGc[] = "UseMinorMSForMinorGC";

// Enables Starboard lifecycle management on Android.
// See b/448156280.
inline constexpr char kUseStarboardLifecycle[] = "UseStarboardLifeCycle";

// Disables V8 Sparkplug baseline compiler.
// See b/512163617.
inline constexpr char kV8DisableSparkplug[] = "V8DisableSparkplug";

// Overrides V8 initial old space size in MB; defaults to 64 MB.
// See b/540879465.
inline constexpr char kV8InitialOldSpaceSize[] = "V8InitialOldSpaceSize";

// Overrides V8 max old space size in MB; defaults to 512 MB.
// See b/549674099.
inline constexpr char kV8MaxOldSpaceSize[] = "V8MaxOldSpaceSize";

// Sets V8 bytecode aging threshold in seconds for aggressive bytecode flushing.
// See b/535719369.
inline constexpr char kV8SetBytecodeOldTime[] = "V8SetBytecodeOldTime";
// keep-sorted end

}  // namespace cobalt::androidtv_flags

#endif  // COBALT_ANDROID_ANDROIDTV_FLAGS_H_
