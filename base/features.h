// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURES_H_
#define BASE_FEATURES_H_

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace base::features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// Alphabetical:
#if BUILDFLAG(IS_COBALT)
// When enabled, Cobalt will handle TRIM_MEMORY_RUNNING_LOW and
// TRIM_MEMORY_RUNNING_MODERATE signals as moderate memory pressure on Android.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltEnableModerateMemoryPressure);

// When enabled, image transfer cache entries bypass serialization and transfer
// images directly to the GPU service thread in-process.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltInProcessImageTransferCache);

// When enabled, overrides the memory pressure throttling cooldown (60s default)
// with the configured cooldown_seconds parameter on Android.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltMemoryPressureCooldown);

// The throttling cooldown in seconds between memory pressure notifications when
// kCobaltMemoryPressureCooldown is enabled.
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                       kCobaltMemoryPressureCooldownSeconds);

// When enabled, gates the CC image decode cache items limit and size limit (in
// MB) via Finch feature and parameters.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltCCImageCacheLimitItems);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kCobaltCCImageCacheLimitItemsCount);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kCobaltCCImageCacheLimitItemsMbs);

// When enabled, gates the GPU memory budget via Finch feature and parameter.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltForceGpuMemAvailable);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kCobaltForceGpuMemAvailableMb);

// When enabled, overrides the GPU discardable cache size limit returned by
// gpu::DiscardableCacheSizeLimit(), which caps the unlocked entries kept by the
// GPU service transfer cache (e.g. uploaded decoded images) and the
// discardable texture managers. Enabled by default on 3P/Starboard platforms.
// Disabled by default on Android TV, where Cobalt runs in low-end device mode
// and the upstream limit is already 1 MB. The parameter is the limit in MB and
// defaults to 1, the upstream low-end Android value. Values above 256 MB, the
// largest upstream default, are capped. Disabling the feature restores the
// upstream per-platform defaults. --force-gpu-mem-discardable-limit-mb still
// takes precedence over both.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltGpuDiscardableCacheLimit);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kCobaltGpuDiscardableCacheLimitMb);

// When enabled, gates the V8 max old space size via Finch feature and
// parameter.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltV8MaxOldSpaceSize);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kCobaltV8MaxOldSpaceSizeMb);

// When enabled, gates the V8 initial old space size via Finch feature and
// parameter.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltV8InitialOldSpaceSize);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kCobaltV8InitialOldSpaceSizeMb);

// When enabled, V8 runs with --optimize-for-size, favoring memory reduction over
// execution speed. Enabled by default on Android to preserve existing status
// quo, and disabled by default on 3P (non-Android) platforms to unlock locked
// 30 FPS UI framerates, while allowing dynamic control via Finch or command
// line.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltV8OptimizeForSize);

// When enabled, overrides the compositor skewport target times, which control
// speculative pre-rastering of offscreen tiles. When disabled, the upstream
// Chromium defaults apply (1.0 software raster / 0.2 GPU raster). Both params
// default to 0, which disables pre-rastering to reduce GPU texture memory.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltSkewportTargetTime);
// Applies to software raster. Upstream Chromium default is 1.0.
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(double,
                                       kCobaltSkewportTargetTimeInSeconds);
// Applies to GPU raster. Upstream Chromium default is 0.2.
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(
    double,
    kCobaltGpuRasterizationSkewportTargetTimeInSeconds);

// When enabled, gates the compositor's prepaint memory budget via Finch feature
// and parameter. Applies to all Cobalt platforms (Android TV and 3P/Starboard).
// The parameter is the percentage of the tile memory budget that may be spent
// on prepaint (non-visible) tiles; 0 disables prepaint raster entirely.
BASE_EXPORT BASE_DECLARE_FEATURE(kCobaltMaxMemoryForPrepaint);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                       kCobaltMaxMemoryForPrepaintPercentage);
#endif  // BUILDFLAG(IS_COBALT)

BASE_EXPORT BASE_DECLARE_FEATURE(kFeatureParamWithCache);

BASE_EXPORT BASE_DECLARE_FEATURE(kFastFilePathIsParent);

BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(bool,
                                       kUseRustJsonParserInCurrentSequence);

BASE_EXPORT BASE_DECLARE_FEATURE(kLowEndMemoryExperiment);

BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kLowMemoryDeviceThresholdMB);

// PPM: Poor performance moment.
//
// This feature covers fixes to many egregious performance problems and the goal
// is to measure their aggregated impact.
BASE_EXPORT BASE_DECLARE_FEATURE(kReducePPMs);

BASE_EXPORT BASE_DECLARE_FEATURE(kScopedBestEffortExecutionFenceForTaskQueue);

BASE_EXPORT BASE_DECLARE_FEATURE(kSimdutfBase64Encode);

BASE_EXPORT BASE_DECLARE_FEATURE(kStackScanMaxFramePointerToStackEndGap);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(
    int,
    kStackScanMaxFramePointerToStackEndGapThresholdMB);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
BASE_EXPORT BASE_DECLARE_FEATURE(kPartialLowEndModeOn3GbDevices);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartialLowEndModeOnMidRangeDevices);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_EXPORT BASE_DECLARE_FEATURE(kBackgroundNotPerceptibleBinding);
BASE_EXPORT BASE_DECLARE_FEATURE(kEffectiveBindingState);
BASE_EXPORT BASE_DECLARE_FEATURE(
    kPostPowerMonitorBroadcastReceiverInitToBackground);
BASE_EXPORT BASE_DECLARE_FEATURE(kPostGetMyMemoryStateToBackground);
BASE_EXPORT BASE_DECLARE_FEATURE(kRebindingChildServiceConnectionController);
BASE_EXPORT BASE_DECLARE_FEATURE(kRebindServiceBatchApi);
BASE_EXPORT BASE_DECLARE_FEATURE(kUseIsUnboundCheck);
BASE_EXPORT BASE_DECLARE_FEATURE(kUseSharedRebindServiceConnection);

BASE_EXPORT BASE_DECLARE_FEATURE(kBackgroundThreadPoolFieldTrial);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(int,
                                       kBackgroundThreadPoolFieldTrialConfig);

BASE_EXPORT BASE_DECLARE_FEATURE(kLibraryPrefetcherMadvise);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(size_t, kLibraryPrefetcherMadviseLength);
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(bool, kLibraryPrefetcherMadviseFallback);

#endif

BASE_EXPORT BASE_DECLARE_FEATURE(kUseTerminationStatusMemoryExhaustion);

// Whether the ReducePPMs feature is enabled. Unlike
// `FeatureList::IsEnabled(base::features::kReducePPMs)`, this can be called
// racily with initializing the FeatureList (although the return value might not
// reflect the state of the feature in the FeatureList in that case).
BASE_EXPORT bool IsReducePPMsEnabled();

// Initializes global variables that depend on `FeatureList`. Must be invoked
// early on process startup, but after `FeatureList` initialization. Different
// parts of //base read experiment state from global variables instead of
// directly from `FeatureList` to avoid data races (default values are used
// before this function is called to initialize the global variables).
BASE_EXPORT void Init();

}  // namespace base::features

#endif  // BASE_FEATURES_H_
