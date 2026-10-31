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

#ifndef COBALT_BROWSER_FEATURES_H_
#define COBALT_BROWSER_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace cobalt {
namespace features {

// Enables the variations config expiration check.
extern const base::Feature kExperimentConfigExpiration;

// Test finch feature for Finch end to end testing.
extern const base::Feature kTestFinchFeature;

// Test finch feature param for Finch end to end testing.
extern const base::FeatureParam<std::string> kTestFinchFeatureParam;

// Enables emitting the Finch token in the User-Agent string for experiment
// verification.
extern const base::Feature kEnableUserAgentFinchToken;

// Token value to emit in the User-Agent string.
extern const base::FeatureParam<std::string> kUserAgentFinchTokenParam;

// Enables native hang reporting via Crashpad.
extern const base::Feature kHangReporting;

// Sets the timeout in seconds for hang watching.
extern const base::FeatureParam<int> kHangWatchTimeSeconds;

// Sets the monitoring period in seconds for hang watching.
extern const base::FeatureParam<int> kHangWatchMonitoringPeriodSeconds;

// Enables thread dump on hang for the main thread.
extern const base::Feature kHangWatchMainThreadDump;

// Enables thread dump on hang for the IO thread.
extern const base::Feature kHangWatchIOThreadDump;

// Enables thread dump on hang for the thread pool threads.
extern const base::Feature kHangWatchThreadPoolDump;

// Enables thread dump on hang for the renderer thread.
extern const base::Feature kHangWatchRendererThreadDump;

// Enables detecting severe hangs (long hangs) via UMA without terminating.
extern const base::Feature kHangWatcherLongHangDetection;

// Enables native abort (LOG(FATAL)) when a long hang is detected.
extern const base::Feature kHangWatcherLongHangKill;

// Sets the timeout in seconds for a hang to be considered a long hang.
extern const base::FeatureParam<int> kLongHangTimeoutSeconds;

// Enables overriding the default metrics collection interval with a fixed
// value.
extern const base::Feature kCobaltMetricsIntervalFeature;

// Sets CPU metrics collection interval in seconds (default 5 min).
extern const base::FeatureParam<int> kCpuMetricsIntervalParam;

// Sets memory metrics collection interval in seconds (default 5 min).
extern const base::FeatureParam<int> kMemoryMetricsIntervalParam;

// Enables virtual address space fragmentation metrics, which are only
// collected on 32-bit Android.
extern const base::Feature kCobaltVirtualAddressSpaceMetrics;

// Probability of emitting the metrics on any given memory collection cycle
// (default 0.01, i.e. one cycle in a hundred).
extern const base::FeatureParam<double>
    kVirtualAddressSpaceSampleProbabilityParam;

// Enables Cobalt Memory Attribution Manager.
extern const base::Feature kCobaltMemoryAttributionManager;

// Sets Cobalt Memory Attribution reporting interval in seconds (default 10
// min).
extern const base::FeatureParam<int>
    kCobaltMemoryAttributionReportIntervalParam;

// Enables custom memory buffer size for in-memory updates.
extern const base::Feature kInMemoryUpdatesMemoryBuffer;

// Sets the memory buffer size in bytes.
extern const base::FeatureParam<int> kInMemoryUpdatesMemoryBufferParam;

// Disables showing the splash screen.
extern const base::Feature kDisableSplashScreen;

// Forces the display of a video as the splash screen.
extern const base::Feature kForceVideoSplashScreen;

// Enables reading the splash screen assets that the WebApp cached with the
// Cache API of the splash domain (splash.webm, splash.png). When disabled, the
// splash screen uses its built-in assets without reading the cache. Disabled by
// default, so that Finch's crash safe mode (the empty experiment config)
// reverts to the built-in assets if a cached asset crashes Cobalt at startup,
// and so that Finch can turn it off as a kill switch.
// Locally: --enable-features=EnableCachedSplashFile
BASE_DECLARE_FEATURE(kEnableCachedSplashFile);

// Enables overriding the splash screen configuration with the params below.
BASE_DECLARE_FEATURE(kSplashScreenConfig);

// How long after the splash screen starts Cobalt switches to the main
// WebContents, once that has loaded (default: 1500ms). The splash screen starts
// when its page finishes loading or, for a video, when the video starts
// playing, within 2s. If SplashScreenConfig is enabled, it is also passed
// to the splash screen as the "timeout" query param (in ms), and the built-in
// splash screen stays up until then instead of closing when its animation
// ends. Values need a unit, e.g., "3s" or "2500ms".
// Locally: --enable-features=SplashScreenConfig:timeout/3s
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kSplashScreenTimeoutParam);

#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
// Overrides the URL loaded as the splash screen. If empty (the default) or not
// a valid URL, the built-in splash screen (switches::kSplashScreenURL) is used.
// The "force_image", "cache", "wasm_policy" and "timeout" query params are
// still appended. Only available in non-gold builds; gold builds always use the
// built-in splash screen. The value must be URL-encoded, e.g., for
// http://example.com:
// --enable-features=SplashScreenConfig:url/http%3A%2F%2Fexample.com
BASE_DECLARE_FEATURE_PARAM(std::string, kSplashScreenUrlParam);
#endif  // !BUILDFLAG(COBALT_IS_RELEASE_BUILD)

// Has the built-in splash screen choose the image or video that it shows with
// a WebAssembly policy, splash_policy.wasm, from the built-in assets and those
// that the WebApp cached for the splash screen's topic, which Cobalt starts
// listing when the splash screen starts (SplashAssetList). The policy chooses
// what the splash screen chooses without it, so that the two differ only in
// how the asset is chosen, and the splash screen falls back to its own choice
// if the policy fails. Only with this feature is the list served
// (h5vcc-embedded://splash_assets.json), and does splash.html's
// Content-Security-Policy, and no other's, allow WebAssembly
// ('wasm-unsafe-eval'). Disabled by default.
// Locally: --enable-features=SplashWasmPolicy
BASE_DECLARE_FEATURE(kSplashWasmPolicy);

// Enables video Picture-in-Picture support.
extern const base::Feature kEnablePictureInPicture;

// Enables native memory ablation study to verify Finch and memory metrics.
extern const base::Feature kCobaltNativeMemoryAblation;
// Disables the dismiss button on network error dialogs
BASE_DECLARE_FEATURE(kDisableNetworkDialogDismissButton);

// Memory ablation size to allocate in Megabytes (default: 0).
extern const base::FeatureParam<int> kMemoryAblationSizeMBParam;

// Delay before performing memory ablation (default: 0s).
extern const base::FeatureParam<base::TimeDelta> kMemoryAblationDelayParam;

// Enables GPU memory ablation study to allocate and hold GPU memory.
BASE_DECLARE_FEATURE(kCobaltGpuMemoryAblation);

// GPU memory ablation size to allocate in Megabytes (default: 0).
extern const base::FeatureParam<int> kGpuMemoryAblationSizeMBParam;

// Delay before performing GPU memory ablation (default: 0s).
extern const base::FeatureParam<base::TimeDelta> kGpuMemoryAblationDelayParam;

// Enables Cobalt memory pressure evaluators.
extern const base::Feature kEnableCobaltMemoryPressureEvaluator;

// Threshold fraction of process memory budget for moderate memory pressure.
extern const base::FeatureParam<double>
    kCobaltMemoryPressureModerateFractionParam;

// Threshold fraction of process memory budget for critical memory pressure.
extern const base::FeatureParam<double>
    kCobaltMemoryPressureCriticalFractionParam;

// Process memory budget override in Megabytes (0 = auto-resolve from physical
// RAM).
extern const base::FeatureParam<int> kCobaltMemoryPressureBudgetMBParam;

// Polling interval in seconds for memory pressure evaluation.
extern const base::FeatureParam<int>
    kCobaltMemoryPressurePollIntervalSecondsParam;

// Hands the Cobalt client hint headers to the network service once, through
// NetworkContextParams, instead of installing a TrustedURLLoaderHeaderClient
// that costs Mojo round trips to the browser UI thread on every request.
BASE_DECLARE_FEATURE(kCobaltSkipTrustedHeaderClient);

// Freezes the web app (document freeze event, JS timers stopped, freeze-time
// memory purging) when the Android Activity is stopped, after an optional
// delay. Disabled by default: Chrobalt currently never freezes in the
// background on Android TV (except NVIDIA Shield). Replaces the retired
// EnableFreeze Java switch.
BASE_DECLARE_FEATURE(kCobaltFreezeOnBackground);

// Delay in milliseconds between the visibilitychange:hidden event and the
// freeze event when kCobaltFreezeOnBackground is enabled. 0 freezes
// immediately; 1500 matches the Cobalt 25 Conceal -> Freeze delay.
extern const base::FeatureParam<int> kCobaltFreezeOnBackgroundDelayMs;

#if BUILDFLAG(IS_ANDROID)
// Android-only: Enables and configures persisting the 16-byte process state
// summary to Android's ActivityManager.setProcessStateSummary() (API 30+).
BASE_DECLARE_FEATURE(kCobaltProcessStateSummary);

// Android-only: Minimum interval between rate-limited process state summary
// flushes to ActivityManager (default: 15s).
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kProcessStateSummaryMinFlushIntervalParam);
#endif  // BUILDFLAG(IS_ANDROID)

// Deduplicates prior-session BrowserStabilityMetrics .pma files (single
// authoritative startup reader/merger) and Android ApplicationExitInfo records
// across sessions.
BASE_DECLARE_FEATURE(kCobaltStabilityMetricsDeduplication);

}  // namespace features
}  // namespace cobalt

#endif  // COBALT_BROWSER_FEATURES_H_
