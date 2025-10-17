// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromecast_buildflags.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/forcedark/forcedark_switches.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/switches.h"

namespace blink::features {

// -----------------------------------------------------------------------------
// Feature definitions and associated constants (feature params, et cetera)
//
// When adding new features or constants for features, please keep the features
// sorted by identifier name (e.g. `kAwesomeFeature`), and the constants for
// that feature grouped with the associated feature.
//
// When defining feature params for auto-generated features (e.g. from
// `RuntimeEnabledFeatures)`, they should still be ordered in this section based
// on the identifier name of the generated feature.

// Controls the capturing of the Ad-Auction-Signals header, and the maximum
// allowed Ad-Auction-Signals header value.
BASE_FEATURE(kAdAuctionSignals,
             "AdAuctionSignals",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAdAuctionSignalsMaxSizeBytes,
                   &kAdAuctionSignals,
                   "ad-auction-signals-max-size-bytes",
                   10000);

// Avoids copying ResourceRequest::TrustedParams when possible.
BASE_FEATURE(kAvoidTrustedParamsCopies,
             "AvoidTrustedParamsCopies",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Block all MIDI access with the MIDI_SYSEX permission
BASE_FEATURE(kBlockMidiByDefault,
             "BlockMidiByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kComputePressureRateObfuscationMitigation,
             "ComputePressureRateObfuscationMitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCrashReportingAPIMoreContextData,
             "CrashReportingAPIMoreContextData",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverrideCrashReportingEndpoint,
             "OverrideCrashReportingEndpoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLowerHighResolutionTimerThreshold,
             "LowerHighResolutionTimerThreshold",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowDatapipeDrainedAsBytesConsumerInBFCache,
             "AllowDatapipeDrainedAsBytesConsumerInBFCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowDevToolsMainThreadDebuggerForMultipleMainFrames,
             "AllowDevToolsMainThreadDebuggerForMultipleMainFrames",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables URN URLs like those produced by Protected Audience auctions to be
// displayed by iframes (instead of requiring fenced frames).
BASE_FEATURE(kAllowURNsInIframes,
             "AllowURNsInIframes",
             base::FEATURE_ENABLED_BY_DEFAULT);

// A console warning is shown when the opaque url returned from Protected
// Audience/selectUrl is used to navigate an iframe. Since fenced frames are not
// going to be enforced for these APIs in the short-medium term, disabling this
// warning for now.
BASE_FEATURE(kDisplayWarningDeprecateURNIframesUseFencedFrames,
             "DisplayWarningDeprecateURNIframesUseFencedFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A server-side switch for the kRealtimeAudio thread type of
// RealtimeAudioWorkletThread object. This can be controlled by a field trial,
// it will use the kNormal type thread when disabled.
BASE_FEATURE(kAudioWorkletThreadRealtimePriority,
             "AudioWorkletThreadRealtimePriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_APPLE)
// When enabled, RealtimeAudioWorkletThread scheduling is optimized taking into
// account how often the worklet logic is executed (which is determined by the
// AudioContext buffer duration).
BASE_FEATURE(kAudioWorkletThreadRealtimePeriodMac,
             "AudioWorkletThreadRealtimePeriodMac",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// A thread pool system for effective usage of RealtimeAudioWorkletThread
// instances.
BASE_FEATURE(kAudioWorkletThreadPool,
             "AudioWorkletThreadPool",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, WebFormElement applies the same special case to nested forms
// as it does for the outermost form. The fix is relevant only to Autofill.
// For other callers of HTMLFormElement::ListedElements(), which don't traverse
// shadow trees and flatten nested forms, are not affected by the feature at
// all. This is a kill switch.
BASE_FEATURE(kAutofillFixFieldsAssociatedWithNestedFormsByParser,
             "AutofillFixFieldsAssociatedWithNestedFormsByParser",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If disabled (default for many years), autofilling triggers KeyDown and
// KeyUp events that do not send any key codes. If enabled, these events
// contain the "Unidentified" key.
BASE_FEATURE(kAutofillSendUnidentifiedKeyAfterFill,
             "AutofillSendUnidentifiedKeyAfterFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

// https://crbug.com/1472970
BASE_FEATURE(kAutoSpeculationRules,
             "AutoSpeculationRules",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kAutoSpeculationRulesHoldback,
                   &kAutoSpeculationRules,
                   "holdback",
                   false);

BASE_FEATURE(kAvoidForcedLayoutOnInitialEmptyDocumentInSubframe,
             "AvoidForcedLayoutOnInitialEmptyDocumentInSubframe",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(https://crbug.com/327075943): Delete this.
BASE_FEATURE(kBFCacheOpenBroadcastChannel,
             "BFCacheOpenBroadcastChannel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBackForwardCacheDWCOnJavaScriptExecution,
             "BackForwardCacheDWCOnJavaScriptExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable background resource fetch in Blink. See https://crbug.com/1379780 for
// more details.
BASE_FEATURE(kBackgroundResourceFetch,
             "BackgroundResourceFetch",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kBackgroundFontResponseProcessor,
                   &kBackgroundResourceFetch,
                   "background-font-response-processor",
                   true);
BASE_FEATURE_PARAM(bool,
                   kBackgroundScriptResponseProcessor,
                   &kBackgroundResourceFetch,
                   "background-script-response-processor",
                   true);
BASE_FEATURE_PARAM(bool,
                   kBackgroundCodeCacheDecoderStart,
                   &kBackgroundResourceFetch,
                   "background-code-cache-decoder-start",
                   true);

// Redefine the oklab and oklch spaces to have gamut mapping baked into them.
// https://crbug.com/1508329
BASE_FEATURE(kBakedGamutMapping,
             "BakedGamutMapping",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to configure a per-origin allowlist of performance.mark events that are
// permitted to be included in slow reports traces. See crbug.com/1181774.
BASE_FEATURE(kBackgroundTracingPerformanceMark,
             "BackgroundTracingPerformanceMark",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kBackgroundTracingPerformanceMark_AllowList,
                   &kBackgroundTracingPerformanceMark,
                   "allow_list",
                   "");

// Boost the priority of the first N not-small images.
// crbug.com/1431169
BASE_FEATURE(kBoostImagePriority,
             "BoostImagePriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
// The number of images to bopost the priority of before returning
// to the default (low) priority.
BASE_FEATURE_PARAM(int,
                   kBoostImagePriorityImageCount,
                   &kBoostImagePriority,
                   "image_count",
                   5);
// Maximum size of an image (in px^2) to be considered "small".
// Small images, where dimensions are specified in the markup, are not boosted.
BASE_FEATURE_PARAM(int,
                   kBoostImagePriorityImageSize,
                   &kBoostImagePriority,
                   "image_size",
                   10000);
// Number of medium-priority requests to allow in tight-mode independent of the
// total number of outstanding requests.
BASE_FEATURE_PARAM(int,
                   kBoostImagePriorityTightMediumLimit,
                   &kBoostImagePriority,
                   "tight_medium_limit",
                   2);

// Boost the priority of certain loading tasks (https://crbug.com/1470003).
BASE_FEATURE(kBoostImageSetLoadingTaskPriority,
             "BoostImageSetLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostFontLoadingTaskPriority,
             "BoostFontLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostVideoLoadingTaskPriority,
             "BoostVideoLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostRenderBlockingStyleLoadingTaskPriority,
             "BoostRenderBlockingStyleLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBoostNonRenderBlockingStyleLoadingTaskPriority,
             "BoostNonRenderBlockingStyleLoadingTaskPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the check for whether the IP address is publicly routable will be
// bypassed when determining the eligibility for a page to be included in topics
// calculation. This is useful for developers to test in local environment.
BASE_FEATURE(kBrowsingTopicsBypassIPIsPubliclyRoutableCheck,
             "BrowsingTopicsBypassIPIsPubliclyRoutableCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables calling the Topics API through Javascript (i.e.
// document.browsingTopics()). For this feature to take effect, the main Topics
// feature has to be enabled first (i.e. `kBrowsingTopics` is enabled, and,
// either a valid Origin Trial token exists or `kPrivacySandboxAdsAPIsOverride`
// is enabled.)
BASE_FEATURE(kBrowsingTopicsDocumentAPI,
             "BrowsingTopicsDocumentAPI",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Decoupled with the main `kBrowsingTopics` feature, so it allows us to
// decouple the server side configs.
BASE_FEATURE(kBrowsingTopicsParameters,
             "BrowsingTopicsParameters",
             base::FEATURE_ENABLED_BY_DEFAULT);
// The periodic topics calculation interval.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsTimePeriodPerEpoch,
                   &kBrowsingTopicsParameters,
                   "time_period_per_epoch",
                   base::Days(7));
// The number of epochs from where to calculate the topics to give to a
// requesting contexts.
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsNumberOfEpochsToExpose,
                   &kBrowsingTopicsParameters,
                   "number_of_epochs_to_expose",
                   3);
// The number of top topics to derive and to keep for each epoch (week).
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsNumberOfTopTopicsPerEpoch,
                   &kBrowsingTopicsParameters,
                   "number_of_top_topics_per_epoch",
                   5);
// The probability (in percent number) to return the random topic to a site. The
// "random topic" is per-site, and is selected from the full taxonomy uniformly
// at random, and each site has a
// `kBrowsingTopicsUseRandomTopicProbabilityPercent`% chance to see their random
// topic instead of one of the top topics.
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsUseRandomTopicProbabilityPercent,
                   &kBrowsingTopicsParameters,
                   "use_random_topic_probability_percent",
                   5);
// Maximum delay between the calculation of the latest epoch and when a site
// starts seeing that epoch's topics. Each site transitions to the latest epoch
// at a per-site, per-epoch random time within
// [calculation time, calculation time + max delay).
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsMaxEpochIntroductionDelay,
                   &kBrowsingTopicsParameters,
                   "max_epoch_introduction_delay",
                   base::Days(2));
// The duration an epoch is retained before deletion.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsEpochRetentionDuration,
                   &kBrowsingTopicsParameters,
                   "epoch_retention_duration",
                   base::Days(28));
// Maximum time offset between when a site stops seeing an epoch's topics and
// when the epoch is actually deleted. Each site transitions away from the
// epoch at a per-site, per-epoch random time within
// [deletion time - max offset, deletion time].
//
// Note: The actual phase-out time can be influenced by the
// 'kBrowsingTopicsNumberOfEpochsToExpose' setting. If this setting enforces a
// more restrictive phase-out, that will take precedence.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsMaxEpochPhaseOutTimeOffset,
                   &kBrowsingTopicsParameters,
                   "max_epoch_phase_out_time_offset",
                   base::Days(2));
// How many epochs (weeks) of API usage data (i.e. topics observations) will be
// based off for the filtering of topics for a calling context.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsNumberOfEpochsOfObservationDataToUseForFiltering,
    &kBrowsingTopicsParameters,
    "number_of_epochs_of_observation_data_to_use_for_filtering",
    3);
// The max number of observed-by context domains to keep for each top topic
// during the epoch topics calculation. The final number of domains associated
// with each topic may be larger than this threshold, because that set of
// domains will also include all domains associated with the topic's descendant
// topics. The intent is to cap the in-use memory.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic,
    &kBrowsingTopicsParameters,
    "max_number_of_api_usage_context_domains_to_keep_per_topic",
    1000);
// The max number of entries allowed to be retrieved from the
// `BrowsingTopicsSiteDataStorage` database for each query for the API usage
// contexts. The query will occur once per epoch (week) at topics calculation
// time. The intent is to cap the peak memory usage.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextEntriesToLoadPerEpoch,
    &kBrowsingTopicsParameters,
    "max_number_of_api_usage_context_entries_to_load_per_epoch",
    100000);
// The max number of API usage context domains allowed to be stored per page
// load.
BASE_FEATURE_PARAM(
    int,
    kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToStorePerPageLoad,
    &kBrowsingTopicsParameters,
    "max_number_of_api_usage_context_domains_to_store_per_page_load",
    30);
// The taxonomy version. This only affects the topics classification that occurs
// during this browser session, and doesn't affect the pre-existing epochs.
BASE_FEATURE_PARAM(int,
                   kBrowsingTopicsTaxonomyVersion,
                   &kBrowsingTopicsParameters,
                   "taxonomy_version",
                   kBrowsingTopicsTaxonomyVersionDefault);
// Comma separated Topic IDs to be blocked. Descendant topics of each blocked
// topic will be blocked as well.
BASE_FEATURE_PARAM(std::string,
                   kBrowsingTopicsDisabledTopicsList,
                   &kBrowsingTopicsParameters,
                   "disabled_topics_list",
                   "");
// Comma separated list of Topic IDs. Prioritize these topics and their
// descendants during top topic selection.
BASE_FEATURE_PARAM(std::string,
                   kBrowsingTopicsPrioritizedTopicsList,
                   &kBrowsingTopicsParameters,
                   "prioritized_topics_list",
                   "57,86,126,149,172,180,196,207,239,254,263,272,289,299,332");
// When a topics calculation times out for the first time, the duration to wait
// before starting a new one.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kBrowsingTopicsFirstTimeoutRetryDelay,
                   &kBrowsingTopicsParameters,
                   "first_timeout_retry_delay",
                   base::Minutes(1));

// When enabled allows the header name used in the blink
// CacheStorageCodeCacheHint runtime feature to be modified.  This runtime
// feature disables generating full code cache for responses stored in
// cache_storage during a service worker install event.  The runtime feature
// must be enabled via the blink runtime feature mechanism, however.
BASE_FEATURE(kCacheStorageCodeCacheHintHeader,
             "CacheStorageCodeCacheHintHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kCacheStorageCodeCacheHintHeaderName,
                   &kCacheStorageCodeCacheHintHeader,
                   "name",
                   "x-CacheStorageCodeCacheHint");

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
// Enables camera preview in permission bubble and site settings.
BASE_FEATURE(kCameraMicPreview,
             "CameraMicPreview",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Temporarily disabled due to issues:
// - PDF blank previews
// - Canvas corruption on ARM64 macOS
// See https://g-issues.chromium.org/issues/328755781
BASE_FEATURE(kCanvas2DHibernation,
             "Canvas2DHibernation",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// When hibernating, make sure that the just-used transfer memory (to transfer
// the snapshot) is freed.
BASE_FEATURE(kCanvas2DHibernationReleaseTransferMemory,
             "Canvas2DHibernationReleaseTransferMemory",
             base::FeatureState::FEATURE_DISABLED_BY_DEFAULT);

// Whether to capture the source location of JavaScript execution, which is one
// of the renderer eviction reasons for Back/Forward Cache.
BASE_FEATURE(kCaptureJSExecutionLocation,
             "CaptureJSExecutionLocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCheckHTMLParserBudgetLessOften,
             "CheckHTMLParserBudgetLessOften",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearSiteDataPrefetchPrerenderCache,
             "ClearSiteDataPrefetchPrerenderCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `dpr` client hint.
BASE_FEATURE(kClientHintsDPR_DEPRECATED,
             "ClientHintsDPR_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `device-memory` client hint.
BASE_FEATURE(kClientHintsDeviceMemory_DEPRECATED,
             "ClientHintsDeviceMemory_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable legacy `width` client hint.
BASE_FEATURE(kClientHintsResourceWidth_DEPRECATED,
             "ClientHintsResourceWidth_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable `form-factor` client hint for XR devices.
BASE_FEATURE(kClientHintsXRFormFactor,
             "ClientHintsXRFormFactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable legacy `viewport-width` client hint.
BASE_FEATURE(kClientHintsViewportWidth_DEPRECATED,
             "ClientHintsViewportWidth_DEPRECATED",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Disabling this will cause parkable strings to never be compressed.
// This is useful for headless mode + virtual time. Since virtual time advances
// quickly, strings may be parked too eagerly in that mode.
BASE_FEATURE(kCompressParkableStrings,
             "CompressParkableStrings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables more conservative settings for ParkableString: suspend parking in
// foreground, and increase aging tick intervals.
BASE_FEATURE(kLessAggressiveParkableString,
             "LessAggressiveParkableString",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Limits maximum capacity of disk data allocator per renderer process.
// DiskDataAllocator and its clients(ParkableString, ParkableImage) will try
// to keep the limitation.
BASE_FEATURE_PARAM(int,
                   kMaxDiskDataAllocatorCapacityMB,
                   &kCompressParkableStrings,
                   "max_disk_capacity_mb",
                   -1);

// When enabled, CreateNewWindow() and ShowCreatedWindow() mojo calls are
// coalesced into a single call to CreateNewWindow().
BASE_FEATURE(kCombineNewWindowIPCs,
             "CombineNewWindowIPCs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls off-thread code cache consumption.
BASE_FEATURE(kConsumeCodeCacheOffThread,
             "ConsumeCodeCacheOffThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the constant streaming in the ContentCapture task.
BASE_FEATURE(kContentCaptureConstantStreaming,
             "ContentCaptureConstantStreaming",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCorrectFloatExtensionTestForWebGL,
             "CorrectFloatExtensionTestForWebGL",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, add a new option, {imageOrientation: 'none'}, to
// createImageBitmap, which ignores the image orientation metadata of the source
// and renders the image as encoded.
BASE_FEATURE(kCreateImageBitmapOrientationNone,
             "CreateImageBitmapOrientationNone",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeferRendererTasksAfterInput,
             "DeferRendererTasksAfterInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kDeferRendererTasksAfterInputPolicyParamName[] = "policy";
const char kDeferRendererTasksAfterInputMinimalTypesPolicyName[] =
    "minimal-types";
const char
    kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyName[] =
        "non-user-blocking-deferrable-types";
const char kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyName[] =
    "non-user-blocking-types";
const char kDeferRendererTasksAfterInputAllDeferrableTypesPolicyName[] =
    "all-deferrable-types";
const char kDeferRendererTasksAfterInputAllTypesPolicyName[] = "all-types";

const base::FeatureParam<TaskDeferralPolicy>::Option kTaskDeferralOptions[] = {
    {TaskDeferralPolicy::kMinimalTypes,
     kDeferRendererTasksAfterInputMinimalTypesPolicyName},
    {TaskDeferralPolicy::kNonUserBlockingDeferrableTypes,
     kDeferRendererTasksAfterInputNonUserBlockingDeferrableTypesPolicyName},
    {TaskDeferralPolicy::kNonUserBlockingTypes,
     kDeferRendererTasksAfterInputNonUserBlockingTypesPolicyName},
    {TaskDeferralPolicy::kAllDeferrableTypes,
     kDeferRendererTasksAfterInputAllDeferrableTypesPolicyName},
    {TaskDeferralPolicy::kAllTypes,
     kDeferRendererTasksAfterInputAllTypesPolicyName}};

BASE_FEATURE_ENUM_PARAM(TaskDeferralPolicy,
                        kTaskDeferralPolicyParam,
                        &kDeferRendererTasksAfterInput,
                        kDeferRendererTasksAfterInputPolicyParamName,
                        TaskDeferralPolicy::kAllTypes,
                        &kTaskDeferralOptions);

BASE_FEATURE(kDelayAsyncScriptExecution,
             "DelayAsyncScriptExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<DelayAsyncScriptDelayType>::Option
    delay_async_script_execution_delay_types[] = {
        {DelayAsyncScriptDelayType::kFinishedParsing, "finished_parsing"},
        {DelayAsyncScriptDelayType::kFirstPaintOrFinishedParsing,
         "first_paint_or_finished_parsing"},
        {DelayAsyncScriptDelayType::kTillFirstLcpCandidate,
         "till_first_lcp_candidate"},
};

BASE_FEATURE_ENUM_PARAM(DelayAsyncScriptDelayType,
                        kDelayAsyncScriptExecutionDelayParam,
                        &kDelayAsyncScriptExecution,
                        "delay_async_exec_delay_type",
                        DelayAsyncScriptDelayType::kFinishedParsing,
                        &delay_async_script_execution_delay_types);

const base::FeatureParam<DelayAsyncScriptTarget>::Option
    delay_async_script_target_types[] = {
        {DelayAsyncScriptTarget::kAll, "all"},
        {DelayAsyncScriptTarget::kCrossSiteOnly, "cross_site_only"},
        {DelayAsyncScriptTarget::kCrossSiteWithAllowList,
         "cross_site_with_allow_list"},
        {DelayAsyncScriptTarget::kCrossSiteWithAllowListReportOnly,
         "cross_site_with_allow_list_report_only"},
};
BASE_FEATURE_ENUM_PARAM(DelayAsyncScriptTarget,
                        kDelayAsyncScriptTargetParam,
                        &kDelayAsyncScriptExecution,
                        "delay_async_exec_target_site",
                        DelayAsyncScriptTarget::kAll,
                        &delay_async_script_target_types);

// kDelayAsyncScriptExecution will delay executing async script at max
// |delay_async_exec_delay_limit|.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kDelayAsyncScriptExecutionDelayLimitParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_delay_limit",
                   base::Seconds(0));

// kDelayAsyncScriptExecution will be disabled after document elapsed more than
// |delay_async_exec_feature_limit|. Zero value means no limit.
// This is to avoid unnecessary async script delay after LCP (for
// kEachLcpCandidate or kEachPaint). Because we can't determine the LCP timing
// while loading, we use timeout instead.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kDelayAsyncScriptExecutionFeatureLimitParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_feature_limit",
                   base::Seconds(0));

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionDelayByDefaultParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_delay_by_default",
                   true);

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionMainFrameOnlyParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_main_frame_only",
                   false);

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionWhenLcpFoundInHtml,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_when_lcp_found_in_html",
                   false);

// kDelayAsyncScriptExecution will change evaluation schedule for the
// specified target.
const base::FeatureParam<AsyncScriptExperimentalSchedulingTarget>::Option
    async_script_experimental_scheduling_targets[] = {
        {AsyncScriptExperimentalSchedulingTarget::kAds, "ads"},
        {AsyncScriptExperimentalSchedulingTarget::kNonAds, "non_ads"},
        {AsyncScriptExperimentalSchedulingTarget::kBoth, "both"},
};
BASE_FEATURE_ENUM_PARAM(AsyncScriptExperimentalSchedulingTarget,
                        kDelayAsyncScriptExecutionTargetParam,
                        &kDelayAsyncScriptExecution,
                        "delay_async_exec_target_script_category",
                        AsyncScriptExperimentalSchedulingTarget::kBoth,
                        &async_script_experimental_scheduling_targets);

// If true, kDelayAsyncScriptExecution will not change the script
// evaluation timing for the non parser inserted script.
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncExecExcludeNonParserInsertedParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_exclude_non_parser_inserted",
                   false);

// If true, kDelayAsyncScriptExecution will not change the script
// evaluation timing for the scripts that were added via document.write().
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncExecExcludeDocumentWriteParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_exclude_document_write",
                   false);

BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionOptOutLowFetchPriorityHintParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_opt_out_low_fetch_priority_hint",
                   false);
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionOptOutAutoFetchPriorityHintParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_opt_out_auto_fetch_priority_hint",
                   false);
BASE_FEATURE_PARAM(bool,
                   kDelayAsyncScriptExecutionOptOutHighFetchPriorityHintParam,
                   &kDelayAsyncScriptExecution,
                   "delay_async_exec_opt_out_high_fetch_priority_hint",
                   false);

BASE_FEATURE(kDelayLayerTreeViewDeletionOnLocalSwap,
             "DelayLayerTreeViewDeletionOnLocalSwap",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam,
                   &kDelayLayerTreeViewDeletionOnLocalSwap,
                   "deletion_task_delay",
                   base::Milliseconds(1000));

// Improves the signal-to-noise ratio of network error related messages in the
// DevTools Console.
// See http://crbug.com/124534.
BASE_FEATURE(kDevToolsImprovedNetworkError,
             "DevToolsImprovedNetworkError",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDirectCompositorThreadIpc,
             "DirectCompositorThreadIpc",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kDisableArrayBufferSizeLimitsForTesting,
             "DisableArrayBufferSizeLimitsForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDiscardInputEventsToRecentlyMovedFrames,
             "DiscardInputEventsToRecentlyMovedFrames",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableThirdPartyStoragePartitioning3DeprecationTrial,
             "DisableThirdPartyStoragePartitioning3DeprecationTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Drop input events at the browser process until the process receives the first
// signal that the renderer has sent a frame to cc (https://crbug.com/40057499).
BASE_FEATURE(kDropInputEventsWhilePaintHolding,
             "DropInputEventsWhilePaintHolding",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEstablishGpuChannelAsync,
             "EstablishGpuChannelAsync",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(crbug.com/1278147): Experiment with this more on desktop to
             // see if it can help.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Whether to respect loading=lazy attribute for images when they are on
// invisible pages.
BASE_FEATURE(kEnableLazyLoadImageForInvisiblePage,
             "EnableLazyLoadImageForInvisiblePage",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<EnableLazyLoadImageForInvisiblePageType>::Option
    enable_lazy_load_image_for_invisible_page_types[] = {
        {EnableLazyLoadImageForInvisiblePageType::kAllInvisiblePage,
         "all_invisible_page"},
        {EnableLazyLoadImageForInvisiblePageType::kPrerenderPage,
         "prerender_page"}};
BASE_FEATURE_ENUM_PARAM(
    EnableLazyLoadImageForInvisiblePageType,
    kEnableLazyLoadImageForInvisiblePageTypeParam,
    &kEnableLazyLoadImageForInvisiblePage,
    "enabled_page_type",
    EnableLazyLoadImageForInvisiblePageType::kAllInvisiblePage,
    &enable_lazy_load_image_for_invisible_page_types);

// Prevents an opener from being returned when a BlobURL is cross-site to the
// window's top-level site.
BASE_FEATURE(kEnforceNoopenerOnBlobURLNavigation,
             "EnforceNoopenerOnBlobURLNavigation",
// TODO(crbug.com/421810301): Temporarily disable this feature on ChromeOS due
// to a regression.
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kEventTimingIgnorePresentationTimeFromUnexpectedFrameSource,
             "EventTimingIgnorePresentationTimeFromUnexpectedFrameSource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExpandCompositedCullRect,
             "ExpandCompositedCullRect",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kCullRectPixelDistanceToExpand,
                   &kExpandCompositedCullRect,
                   "pixels",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
                   2000
#else
                   4000
#endif
);
BASE_FEATURE_PARAM(double,
                   kCullRectExpansionDPRCoef,
                   &kExpandCompositedCullRect,
                   "dpr_coef",
                   1);
BASE_FEATURE_PARAM(bool,
                   kSmallScrollersUseMinCullRect,
                   &kExpandCompositedCullRect,
                   "small_scroller_opt",
                   true);
BASE_FEATURE_PARAM(int,
                   kCullRectChangedEnoughDistance,
                   &kExpandCompositedCullRect,
                   "changed_enough",
                   512);

// Enable the <fencedframe> element; see crbug.com/1123606. Note that enabling
// this feature does not automatically expose this element to the web, it only
// allows the element to be enabled by the runtime enabled feature, for origin
// trials.
BASE_FEATURE(kFencedFrames, "FencedFrames", base::FEATURE_ENABLED_BY_DEFAULT);

// Enable sending event-level reports through reportEvent() in cross-origin
// subframes. This requires opt-in both from the cross-origin subframe that is
// sending the beacon as well as the document that contains information about
// the reportEvent() endpoints.
BASE_FEATURE(kFencedFramesCrossOriginEventReporting,
             "FencedFramesCrossOriginEventReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Temporarily un-disable credentials on fenced frame automatic beacons until
// third party cookie deprecation.
// TODO(crbug.com/1496395): Remove this after 3PCD.
BASE_FEATURE(kFencedFramesAutomaticBeaconCredentials,
             "FencedFramesAutomaticBeaconCredentials",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFencedFramesCrossOriginAutomaticBeaconData,
             "FencedFramesCrossOriginAutomaticBeaconData",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls functionality related to network revocation/local unpartitioned
// data access in fenced frames.
BASE_FEATURE(kFencedFramesLocalUnpartitionedDataAccess,
             "FencedFramesLocalUnpartitionedDataAccess",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFencedFramesReportEventHeaderChanges,
             "FencedFramesReportEventHeaderChanges",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a bug fix that allows a 'src' allowlist in the |allow| parameter of a
// <fencedframe> or <iframe> loaded with a FencedFrameConfig to behave as
// expected. See: https://crbug.com/349080952
BASE_FEATURE(kFencedFramesSrcPermissionsPolicy,
             "FencedFramesSrcPermissionsPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls access to an API to exempt certain URLs from fenced frame
// network revocation to facilitate testing.
BASE_FEATURE(kExemptUrlFromNetworkRevocationForTesting,
             "ExemptUrlFromNetworkRevocationForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use "style" and "json" destinations for CSS and JSON modules.
// https://crbug.com/1491336
BASE_FEATURE(kFetchDestinationJsonCssModules,
             "kFetchDestinationJsonCssModules",
             base::FEATURE_ENABLED_BY_DEFAULT);

// File handling icons. https://crbug.com/1218213
BASE_FEATURE(kFileHandlingIcons,
             "FileHandlingIcons",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFileSystemUrlNavigation,
             "FileSystemUrlNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFileSystemUrlNavigationForChromeAppsOnly,
             "FileSystemUrlNavigationForChromeAppsOnly",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFilteringScrollPrediction,
             "FilteringScrollPrediction",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(b/284271126): Run the experiment on desktop and enable if
             // positive.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
BASE_FEATURE_PARAM(std::string,
                   kFilteringScrollPredictionFilterParam,
                   &kFilteringScrollPrediction,
                   "filter",
                   "one_euro_filter");

// See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
// Enables FLEDGE implementation. See https://crbug.com/1186444.
BASE_FEATURE(kFledge, "Fledge", base::FEATURE_ENABLED_BY_DEFAULT);

// See
// https://github.com/WICG/turtledove/blob/main/FLEDGE_browser_bidding_and_auction_API.md
BASE_FEATURE(kFledgeBiddingAndAuctionServer,
             "FledgeBiddingAndAuctionServer",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kFledgeBiddingAndAuctionKeyURL,
                   &kFledgeBiddingAndAuctionServer,
                   "FledgeBiddingAndAuctionKeyURL",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kFledgeBiddingAndAuctionKeyConfig,
                   &kFledgeBiddingAndAuctionServer,
                   "FledgeBiddingAndAuctionKeyConfig",
                   "");

// See https://github.com/WICG/turtledove/issues/1334
BASE_FEATURE(kFledgeOriginScopedKeys,
             "FledgeOriginScopedKeys",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(std::string,
                   kFledgeOriginScopedKeyConfig,
                   &kFledgeOriginScopedKeys,
                   "FledgeOriginScopedKeyConfig",
                   "");

// See in the header.
BASE_FEATURE(kFledgeConsiderKAnonymity,
             "FledgeConsiderKAnonymity",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kFledgeEnforceKAnonymity,
             "FledgeEnforceKAnonymity",
             base::FEATURE_DISABLED_BY_DEFAULT);

// See the header for more details.
BASE_FEATURE(kFledgeLimitSelectableBuyerAndSellerReportingIds,
             "FledgeLimitSelectableBuyerAndSellerReportingIds",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kFledgeSelectableBuyerAndSellerReportingIdsSoftLimit,
                   &kFledgeLimitSelectableBuyerAndSellerReportingIds,
                   "SelectableBuyerAndSellerReportingIdsSoftLimit",
                   -1);
BASE_FEATURE_PARAM(int,
                   kFledgeSelectableBuyerAndSellerReportingIdsHardLimit,
                   &kFledgeLimitSelectableBuyerAndSellerReportingIds,
                   "SelectableBuyerAndSellerReportingIdsHardLimit",
                   -1);

BASE_FEATURE(kFledgeMaxGroupLifetimeFeature,
             "FledgeMaxGroupLifetimeFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeMaxGroupLifetime,
                   &kFledgeMaxGroupLifetimeFeature,
                   "fledge_max_group_lifetime",
                   base::Days(30));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeMaxGroupLifetimeForMetadata,
                   &kFledgeMaxGroupLifetimeFeature,
                   "fledge_max_group_lifetime_for_metadata",
                   base::Days(30));

BASE_FEATURE(kFledgeEnableSampleDebugReportOnCookieSetting,
             "FledgeEnableSampleDebugReportOnCookieSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeSampleDebugReports,
             "FledgeSampleDebugReports",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeDebugReportLockout,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_lockout",
                   base::Days(365 * 3));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeDebugReportRestrictedCooldown,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_restricted_cooldown",
                   base::Days(365));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeDebugReportShortCooldown,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_short_cooldown",
                   base::Days(14));
BASE_FEATURE_PARAM(int,
                   kFledgeDebugReportSamplingRandomMax,
                   &kFledgeSampleDebugReports,
                   "fledge_debug_report_sampling_random_max",
                   1000);
BASE_FEATURE_PARAM(
    int,
    kFledgeDebugReportSamplingRestrictedCooldownRandomMax,
    &kFledgeSampleDebugReports,
    "fledge_debug_report_sampling_restricted_cooldown_random_max",
    10);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeEnableFilteringDebugReportStartingFrom,
                   &kFledgeSampleDebugReports,
                   "fledge_enable_filtering_debug_report_starting_from",
                   base::Milliseconds(0));

BASE_FEATURE_PARAM(int,
                   kFledgeCustomMaxAuctionAdComponentsValue,
                   &kFledgeCustomMaxAuctionAdComponents,
                   "FledgeAdComponentLimit",
                   40);

BASE_FEATURE_PARAM(int,
                   kFledgeRealTimeReportingNumBuckets,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingNumBuckets",
                   1024);
BASE_FEATURE_PARAM(double,
                   kFledgeRealTimeReportingEpsilon,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingEpsilon",
                   1);
BASE_FEATURE_PARAM(double,
                   kFledgeRealTimeReportingPlatformContributionPriority,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingPlatformContributionPriority",
                   1);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kFledgeRealTimeReportingWindow,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingWindow",
                   base::Seconds(20));
BASE_FEATURE_PARAM(int,
                   kFledgeRealTimeReportingMaxReports,
                   &kFledgeRealTimeReporting,
                   "FledgeRealTimeReportingMaxReports",
                   10);

// Enable enforcement of permission policy for
// privateAggregation.contributeToHistogramOnEvent.
BASE_FEATURE(kFledgeEnforcePermissionPolicyContributeOnEvent,
             "FledgeEnforcePermissionPolicyContributeOnEvent",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFledgeDisableLocalAdsAuctions,
             "FledgeDisableLocalAdsAuctions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Provides a configurable limit on the number of
// `selectableBuyerAndSellerReportingIds` for which the browser fetches k-anon
// keys. If the `SelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit` is
// negative, no limit is enforced.
BASE_FEATURE(kFledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon,
             "FledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(
    int,
    kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit,
    &kFledgeLimitSelectableBuyerAndSellerReportingIdsFetchedFromKAnon,
    "SelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit",
    -1);

// Feature flag to truncate the set of `selectableBuyerAndSellerReportingIds`
// to only those for which k-anon status was fetched, as limited by the
// `kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit` parameter
// defined above. This is only meaningful if
// `kFledgeSelectableBuyerAndSellerReportingIdsFetchedFromKAnonLimit` is >= 0.
BASE_FEATURE(kFledgeTruncateSelectableBuyerAndSellerReportingIdsToKAnonLimit,
             "FledgeTruncateSelectableBuyerAndSellerReportingIdsToKAnonLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceHighPerformanceGPUForWebGL,
             "ForceHighPerformanceGPUForWebGL",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kForceInOrderScript,
             "ForceInOrderScript",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Text autosizing uses heuristics to inflate text sizes on devices with
// small screens. This feature is for disabling these heuristics.
BASE_FEATURE(kForceOffTextAutosizing,
             "ForceOffTextAutosizing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Automatically convert light-themed pages to use a Blink-generated dark theme
BASE_FEATURE(kForceWebContentsDarkMode,
             "WebContentsForceDark",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Which algorithm should be used for color inversion?
const base::FeatureParam<ForceDarkInversionMethod>::Option
    forcedark_inversion_method_options[] = {
        {ForceDarkInversionMethod::kUseBlinkSettings,
         "use_blink_settings_for_method"},
        {ForceDarkInversionMethod::kHslBased, "hsl_based"},
        {ForceDarkInversionMethod::kCielabBased, "cielab_based"},
        {ForceDarkInversionMethod::kRgbBased, "rgb_based"}};

BASE_FEATURE_ENUM_PARAM(ForceDarkInversionMethod,
                        kForceDarkInversionMethodParam,
                        &kForceWebContentsDarkMode,
                        "inversion_method",
                        ForceDarkInversionMethod::kUseBlinkSettings,
                        &forcedark_inversion_method_options);

// Should images be inverted?
const base::FeatureParam<ForceDarkImageBehavior>::Option
    forcedark_image_behavior_options[] = {
        {ForceDarkImageBehavior::kUseBlinkSettings,
         "use_blink_settings_for_images"},
        {ForceDarkImageBehavior::kInvertNone, "none"},
        {ForceDarkImageBehavior::kInvertSelectively, "selective"}};

BASE_FEATURE_ENUM_PARAM(ForceDarkImageBehavior,
                        kForceDarkImageBehaviorParam,
                        &kForceWebContentsDarkMode,
                        "image_behavior",
                        ForceDarkImageBehavior::kUseBlinkSettings,
                        &forcedark_image_behavior_options);

// Do not invert text lighter than this.
// Range: 0 (do not invert any text) to 255 (invert all text)
// Can also set to -1 to let Blink's internal settings control the value
BASE_FEATURE_PARAM(int,
                   kForceDarkForegroundLightnessThresholdParam,
                   &kForceWebContentsDarkMode,
                   "foreground_lightness_threshold",
                   -1);

// Do not invert backgrounds darker than this.
// Range: 0 (invert all backgrounds) to 255 (invert no backgrounds)
// Can also set to -1 to let Blink's internal settings control the value
BASE_FEATURE_PARAM(int,
                   kForceDarkBackgroundLightnessThresholdParam,
                   &kForceWebContentsDarkMode,
                   "background_lightness_threshold",
                   -1);

const base::FeatureParam<ForceDarkImageClassifier>::Option
    forcedark_image_classifier_policy_options[] = {
        {ForceDarkImageClassifier::kUseBlinkSettings,
         "use_blink_settings_for_image_policy"},
        {ForceDarkImageClassifier::kNumColorsWithMlFallback,
         "num_colors_with_ml_fallback"},
        {ForceDarkImageClassifier::kTransparencyAndNumColors,
         "transparency_and_num_colors"},
};

BASE_FEATURE_ENUM_PARAM(ForceDarkImageClassifier,
                        kForceDarkImageClassifierParam,
                        &kForceWebContentsDarkMode,
                        "classifier_policy",
                        ForceDarkImageClassifier::kUseBlinkSettings,
                        &forcedark_image_classifier_policy_options);

// Enables the frequency capping for detecting large sticky ads.
// Large-sticky-ads are those ads that stick to the bottom of the page
// regardless of a user’s efforts to scroll, and take up more than 30% of the
// screen’s real estate.
BASE_FEATURE(kFrequencyCappingForLargeStickyAdDetection,
             "FrequencyCappingForLargeStickyAdDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the frequency capping for detecting overlay popups. Overlay-popups
// are the interstitials that pop up and block the main content of the page.
BASE_FEATURE(kFrequencyCappingForOverlayPopupDetection,
             "FrequencyCappingForOverlayPopupDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGMSCoreEmoji, "GMSCoreEmoji", base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
// Defers device selection until after permission is granted.
BASE_FEATURE(kGetUserMediaDeferredDeviceSettingsSelection,
             "GetUserMediaDeferredDeviceSettingsSelection",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE_PARAM(std::string,
                   kHTMLParserYieldEventNameForPause,
                   &kHTMLParserYieldByUserTiming,
                   "pause_event_name",
                   "");

BASE_FEATURE_PARAM(std::string,
                   kHTMLParserYieldEventNameForResume,
                   &kHTMLParserYieldByUserTiming,
                   "resume_event_name",
                   "");

BASE_FEATURE_PARAM(size_t,
                   kHTMLParserYieldTimeoutInMs,
                   &kHTMLParserYieldByUserTiming,
                   "timeout_ms",
                   20);

BASE_FEATURE(kIgnoreInputWhileHidden,
             "IgnoreInputWhileHidden",
             // TODO(crbug.com/407265465) Some Accessibility tools on Windows
             // appear to mark the Renderer as Hidden. This feature currently
             // breaks them. Disabling until the root cause can be identified.
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kImageLoadingPrioritizationFix,
             "ImageLoadingPrioritizationFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIndexedDBCompressValuesWithSnappy,
             "IndexedDBCompressValuesWithSnappy",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int>
    kIndexedDBCompressValuesWithSnappyCompressionThreshold{
        &features::kIndexedDBCompressValuesWithSnappy,
        /*name=*/"compression-threshold",
        /*default_value=*/-1};

BASE_FEATURE(kInputPredictorTypeChoice,
             "InputPredictorTypeChoice",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInputScenarioPriorityBoost,
             "InputScenarioPriorityBoost",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool> kInputScenarioPriorityBoostIncludesLoading{
    &features::kInputScenarioPriorityBoost,
    "input_scenario_priority_boost_includes_loading", false};

// When enabled, wake ups from throttleable TaskQueues are limited to 1 per
// minute in a page that has been backgrounded for 5 minutes.
//
// Intensive wake up throttling is enforced in addition to other throttling
// mechanisms:
//  - 1 wake up per second in a background page or hidden cross-origin frame
//  - 1% CPU time in a page that has been backgrounded for 10 seconds
//
// Feature tracking bug: https://crbug.com/1075553
//
// The base::Feature should not be read from; rather the provided accessors
// should be used, which also take into account the managed policy override of
// the feature.
//
// The base::Feature is enabled by default on all platforms. However, on
// Android, it has no effect because page freezing kicks in at the same time. It
// would have an effect if the grace period ("grace_period_seconds" param) was
// reduced.
BASE_FEATURE(kIntensiveWakeUpThrottling,
             "IntensiveWakeUpThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Name of the parameter that controls the grace period during which there is no
// intensive wake up throttling after a page is hidden. Defined here to allow
// access from about_flags.cc. The FeatureParam is defined in
// third_party/blink/renderer/platform/scheduler/common/features.cc.
const char kIntensiveWakeUpThrottling_GracePeriodSeconds_Name[] =
    "grace_period_seconds";

BASE_FEATURE(kInteractiveDetectorIgnoreFcp,
             "InteractiveDetectorIgnoreFcp",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow process isolation of iframes with the 'sandbox' attribute set. Whether
// or not such an iframe will be isolated may depend on options specified with
// the attribute. Note: At present, only iframes with origin-restricted
// sandboxes are isolated.
BASE_FEATURE(kIsolateSandboxedIframes,
             "IsolateSandboxedIframes",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<IsolateSandboxedIframesGrouping>::Option
    isolated_sandboxed_iframes_grouping_types[] = {
        {IsolateSandboxedIframesGrouping::kPerSite, "per-site"},
        {IsolateSandboxedIframesGrouping::kPerOrigin, "per-origin"},
        {IsolateSandboxedIframesGrouping::kPerDocument, "per-document"}};
BASE_FEATURE_ENUM_PARAM(IsolateSandboxedIframesGrouping,
                        kIsolateSandboxedIframesGroupingParam,
                        &kIsolateSandboxedIframes,
                        "grouping",
                        IsolateSandboxedIframesGrouping::kPerOrigin,
                        &isolated_sandboxed_iframes_grouping_types);

// Serves as killswitch for migrating CanvasRenderingContext2D::IsPaintable()
// from checking the existence of the canvas' Canvas2DLayerBridge to checking
// for the existence of its resource provider as well as the associated
// necessary change of changing GetOrCreateCanvasResourceProvider() away
// from using GetOrCreateCanvas2DLayerBridge() for 2D contexts.
// NOTE: Do not check this feature directly: Check
// CheckProviderInCanvas2DRenderingContextIsPaintable() instead.
BASE_FEATURE(kIsPaintableChecksResourceProviderInsteadOfBridge,
             "IsPaintableChecksResourceProviderInsteadOfBridge",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kKalmanDirectionCutOff,
             "KalmanDirectionCutOff",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKalmanHeuristics,
             "KalmanHeuristics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kKeepAliveInBrowserMigration,
             "KeepAliveInBrowserMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionReportingInBrowserMigration,
             "AttributionReportingInBrowserMigration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLimitLayerMergeDistance,
             "LimitLayerMergeDistance",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(size_t,
                   kLayerMergeDistanceLimit,
                   &kLimitLayerMergeDistance,
                   "limit",
                   0x10000000);

BASE_FEATURE(kLCPCriticalPathPredictor,
             "LCPCriticalPathPredictor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kLCPCriticalPathAdjustImageLoadPriority,
                   &kLCPCriticalPathPredictor,
                   "lcpp_adjust_image_load_priority",
                   false);

BASE_FEATURE_PARAM(size_t,
                   kLCPCriticalPathPredictorMaxElementLocatorLength,
                   &kLCPCriticalPathPredictor,
                   "lcpp_max_element_locator_length",
                   1024);

BASE_FEATURE_PARAM(bool,
                   kLCPCriticalPathAdjustImageLoadPriorityOverrideFirstNBoost,
                   &kLCPCriticalPathPredictor,
                   "lcpp_adjust_image_load_priority_override_first_n_boost",
                   false);

BASE_FEATURE_PARAM(double,
                   kLcppAdjustImageLoadPriorityConfidenceThreshold,
                   &kLCPCriticalPathPredictor,
                   "lcpp_adjust_image_load_priority_confidence_threshold",
                   0);

const base::FeatureParam<LcppRecordedLcpElementTypes>::Option
    lcpp_recorded_element_types[] = {
        {LcppRecordedLcpElementTypes::kAll, "all"},
        {LcppRecordedLcpElementTypes::kImageOnly, "image_only"},
};
BASE_FEATURE_ENUM_PARAM(LcppRecordedLcpElementTypes,
                        kLCPCriticalPathPredictorRecordedLcpElementTypes,
                        &kLCPCriticalPathPredictor,
                        "lcpp_recorded_lcp_element_types",
                        LcppRecordedLcpElementTypes::kImageOnly,
                        &lcpp_recorded_element_types);

const base::FeatureParam<LcppResourceLoadPriority>::Option
    lcpp_resource_load_priorities[] = {
        {LcppResourceLoadPriority::kMedium, "medium"},
        {LcppResourceLoadPriority::kHigh, "high"},
        {LcppResourceLoadPriority::kVeryHigh, "very_high"},
};
BASE_FEATURE_ENUM_PARAM(LcppResourceLoadPriority,
                        kLCPCriticalPathPredictorImageLoadPriority,
                        &kLCPCriticalPathPredictor,
                        "lcpp_image_load_priority",
                        LcppResourceLoadPriority::kVeryHigh,
                        &lcpp_resource_load_priorities);

BASE_FEATURE_PARAM(
    bool,
    kLCPCriticalPathPredictorImageLoadPriorityEnabledForHTMLImageElement,
    &kLCPCriticalPathPredictor,
    "lcpp_enable_image_load_priority_for_htmlimageelement",
    false);

BASE_FEATURE_PARAM(int,
                   kLCPCriticalPathPredictorMaxHostsToTrack,
                   &kLCPCriticalPathPredictor,
                   "lcpp_max_hosts_to_track",
                   100);

BASE_FEATURE_PARAM(int,
                   kLCPCriticalPathPredictorSlidingWindowSize,
                   &kLCPCriticalPathPredictor,
                   "lcpp_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPCriticalPathPredictorMaxHistogramBuckets,
                   &kLCPCriticalPathPredictor,
                   "lcpp_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPScriptObserver,
             "LCPScriptObserver",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_ENUM_PARAM(LcppResourceLoadPriority,
                        kLCPScriptObserverScriptLoadPriority,
                        &kLCPScriptObserver,
                        "lcpscriptobserver_script_load_priority",
                        LcppResourceLoadPriority::kVeryHigh,
                        &lcpp_resource_load_priorities);

BASE_FEATURE_ENUM_PARAM(LcppResourceLoadPriority,
                        kLCPScriptObserverImageLoadPriority,
                        &kLCPScriptObserver,
                        "lcpscriptobserver_image_load_priority",
                        LcppResourceLoadPriority::kVeryHigh,
                        &lcpp_resource_load_priorities);

BASE_FEATURE_PARAM(size_t,
                   kLCPScriptObserverMaxUrlLength,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_script_max_url_length",
                   1024);

BASE_FEATURE_PARAM(size_t,
                   kLCPScriptObserverMaxUrlCountPerOrigin,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_script_max_url_count_per_origin",
                   5);

BASE_FEATURE_PARAM(bool,
                   kLCPScriptObserverAdjustImageLoadPriority,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_adjust_image_load_priority",
                   false);

BASE_FEATURE_PARAM(int,
                   kLCPScriptObserverSlidingWindowSize,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPScriptObserverMaxHistogramBuckets,
                   &kLCPScriptObserver,
                   "lcpscriptobserver_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPTimingPredictorPrerender2,
             "LCPTimingPredictorPrerender2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kLCPTimingPredictorSlidingWindowSize,
                   &kLCPTimingPredictorPrerender2,
                   "lcp_timing_predictor_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPTimingPredictorMaxHistogramBuckets,
                   &kLCPTimingPredictorPrerender2,
                   "lcp_timing_predictor_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPAutoPreconnectLcpOrigin,
             "LCPPAutoPreconnectLcpOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(double,
                   kLCPPAutoPreconnectFrequencyThreshold,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_frequency_threshold",
                   0.5);

BASE_FEATURE_PARAM(int,
                   kkLCPPAutoPreconnectMaxPreconnectOriginsCount,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_max_origins",
                   2);

BASE_FEATURE_PARAM(int,
                   kLCPPAutoPreconnectSlidingWindowSize,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPPAutoPreconnectMaxHistogramBuckets,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_max_histogram_buckets",
                   10);

BASE_FEATURE_PARAM(bool,
                   kLCPPAutoPreconnectRecordAllOrigins,
                   &kLCPPAutoPreconnectLcpOrigin,
                   "lcpp_preconnect_record_all_origins",
                   false);

BASE_FEATURE(kLCPPDeferUnusedPreload,
             "LCPPDeferUnusedPreload",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<LcppDeferUnusedPreloadExcludedResourceType>::Option
    lcpp_defer_unused_preload_excluded_resource_type[] = {
        {LcppDeferUnusedPreloadExcludedResourceType::kNone, "none"},
        {LcppDeferUnusedPreloadExcludedResourceType::kStyleSheet, "stylesheet"},
        {LcppDeferUnusedPreloadExcludedResourceType::kScript, "script"},
        {LcppDeferUnusedPreloadExcludedResourceType::kMock, "mock"},
};

BASE_FEATURE_ENUM_PARAM(LcppDeferUnusedPreloadExcludedResourceType,
                        kLcppDeferUnusedPreloadExcludedResourceType,
                        &kLCPPDeferUnusedPreload,
                        "excluded_resource_type",
                        LcppDeferUnusedPreloadExcludedResourceType::kNone,
                        &lcpp_defer_unused_preload_excluded_resource_type);

BASE_FEATURE_PARAM(double,
                   kLCPPDeferUnusedPreloadFrequencyThreshold,
                   &kLCPPDeferUnusedPreload,
                   "lcpp_unused_preload_frequency_threshold",
                   0.5);

const base::FeatureParam<LcppDeferUnusedPreloadPreloadedReason>::Option
    lcpp_defer_unused_preload_preloaded_reason[] = {
        {LcppDeferUnusedPreloadPreloadedReason::kAll, "all"},
        {LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly,
         "link_preload"},
        {LcppDeferUnusedPreloadPreloadedReason::kBrowserSpeculativePreloadOnly,
         "speculative_preload"},
};

BASE_FEATURE_ENUM_PARAM(LcppDeferUnusedPreloadPreloadedReason,
                        kLcppDeferUnusedPreloadPreloadedReason,
                        &kLCPPDeferUnusedPreload,
                        "preloaded_reason",
                        LcppDeferUnusedPreloadPreloadedReason::kAll,
                        &lcpp_defer_unused_preload_preloaded_reason);

const base::FeatureParam<LcppDeferUnusedPreloadTiming>::Option
    lcpp_defer_unused_preload_timing[] = {
        {LcppDeferUnusedPreloadTiming::kPostTask, "post_task"},
        {LcppDeferUnusedPreloadTiming::kLcpTimingPredictor,
         "lcp_timing_predictor"},
        {LcppDeferUnusedPreloadTiming::kLcpTimingPredictorWithPostTask,
         "lcp_timing_predictor_with_post_task"},
};

BASE_FEATURE_ENUM_PARAM(LcppDeferUnusedPreloadTiming,
                        kLcppDeferUnusedPreloadTiming,
                        &kLCPPDeferUnusedPreload,
                        "load_timing",
                        LcppDeferUnusedPreloadTiming::kPostTask,
                        &lcpp_defer_unused_preload_timing);

BASE_FEATURE_PARAM(int,
                   kLCPPDeferUnusedPreloadSlidingWindowSize,
                   &kLCPPDeferUnusedPreload,
                   "lcpp_unused_preload_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPPDeferUnusedPreloadMaxHistogramBuckets,
                   &kLCPPDeferUnusedPreload,
                   "lcpp_unused_preload_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPFontURLPredictor,
             "LCPPFontURLPredictor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kLCPPFontURLPredictorMaxUrlLength,
                   &kLCPPFontURLPredictor,
                   "lcpp_max_font_url_length",
                   1024);

BASE_FEATURE_PARAM(size_t,
                   kLCPPFontURLPredictorMaxUrlCountPerOrigin,
                   &kLCPPFontURLPredictor,
                   "lcpp_max_font_url_count_per_origin",
                   10);

BASE_FEATURE_PARAM(double,
                   kLCPPFontURLPredictorFrequencyThreshold,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_url_frequency_threshold",
                   0.5);

BASE_FEATURE_PARAM(int,
                   kLCPPFontURLPredictorMaxPreloadCount,
                   &kLCPPFontURLPredictor,
                   "lcpp_max_font_url_to_preload",
                   5);

BASE_FEATURE_PARAM(bool,
                   kLCPPFontURLPredictorEnablePrefetch,
                   &kLCPPFontURLPredictor,
                   "lcpp_enable_font_prefetch",
                   false);

// Negative value is used for disabling this threshold.
BASE_FEATURE_PARAM(double,
                   kLCPPFontURLPredictorThresholdInMbps,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_prefetch_threshold",
                   -1);

const base::FeatureParam<std::string> kLCPPFontURLPredictorExcludedHosts{
    &kLCPPFontURLPredictor, "lcpp_font_prefetch_excluded_hosts", ""};

BASE_FEATURE_PARAM(bool,
                   kLCPPCrossSiteFontPredictionAllowed,
                   &kLCPPFontURLPredictor,
                   "lcpp_cross_site_font_prediction_allowed",
                   false);

BASE_FEATURE_PARAM(int,
                   kLCPPFontURLPredictorSlidingWindowSize,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLCPPFontURLPredictorMaxHistogramBuckets,
                   &kLCPPFontURLPredictor,
                   "lcpp_font_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPInitiatorOrigin,
             "LCPPInitiatorOrigin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kLcppInitiatorOriginHistogramSlidingWindowSize,
                   &kLCPPInitiatorOrigin,
                   "lcpp_initiator_origin_histogram_sliding_window_size",
                   10000);

BASE_FEATURE_PARAM(int,
                   kLcppInitiatorOriginMaxHistogramBuckets,
                   &kLCPPInitiatorOrigin,
                   "lcpp_initiator_origin_max_histogram_buckets",
                   100);

BASE_FEATURE(kLCPPLazyLoadImagePreload,
             "LCPPLazyLoadImagePreload",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If true, do not make a preload request.
BASE_FEATURE_PARAM(bool,
                   kLCPPLazyLoadImagePreloadDryRun,
                   &kLCPPLazyLoadImagePreload,
                   "lcpp_lazy_load_image_preload_dry_run",
                   false);

const base::FeatureParam<
    LcppPreloadLazyLoadImageType>::Option lcpp_preload_lazy_load_image[] = {
    {LcppPreloadLazyLoadImageType::kNone, "none"},
    {LcppPreloadLazyLoadImageType::kNativeLazyLoading, "native_lazy_loading"},
    {LcppPreloadLazyLoadImageType::kCustomLazyLoading, "custom_lazy_loading"},
    {LcppPreloadLazyLoadImageType::kAll, "all"},
};
BASE_FEATURE_ENUM_PARAM(LcppPreloadLazyLoadImageType,
                        kLCPCriticalPathPredictorPreloadLazyLoadImageType,
                        &kLCPPLazyLoadImagePreload,
                        "lcpp_preload_lazy_load_image_type",
                        LcppPreloadLazyLoadImageType::kNativeLazyLoading,
                        &lcpp_preload_lazy_load_image);

BASE_FEATURE(kPreloadSystemFonts,
             "PreloadSystemFonts",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPreloadSystemFontsTargets{
    &kPreloadSystemFonts, "preload_system_fonts_targets", "[]"};

BASE_FEATURE_PARAM(int,
                   kPreloadSystemFontsRequiredMemoryGB,
                   &kPreloadSystemFonts,
                   "preload_system_fonts_required_memory_gb",
                   4);

BASE_FEATURE(kLCPPMultipleKey,
             "LCPPMultipleKey",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kLCPPMultipleKeyMaxPathLength,
                   &kLCPPMultipleKey,
                   "lcpp_multiple_key_max_path_length",
                   15);

const base::FeatureParam<LcppMultipleKeyTypes>::Option
    lcpp_multiple_key_types[] = {
        {LcppMultipleKeyTypes::kDefault, "default"},
        {LcppMultipleKeyTypes::kLcppKeyStat, "lcpp_key_stat"},
};

BASE_FEATURE_ENUM_PARAM(LcppMultipleKeyTypes,
                        kLcppMultipleKeyType,
                        &kLCPPMultipleKey,
                        "lcpp_multiple_key_type",
                        LcppMultipleKeyTypes::kLcppKeyStat,
                        &lcpp_multiple_key_types);

BASE_FEATURE_PARAM(int,
                   kLcppMultipleKeyHistogramSlidingWindowSize,
                   &kLCPPMultipleKey,
                   "lcpp_multiple_key_histogram_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kLcppMultipleKeyMaxHistogramBuckets,
                   &kLCPPMultipleKey,
                   "lcpp_multiple_key_max_histogram_buckets",
                   10);

BASE_FEATURE(kLCPPPrefetchSubresource,
             "LCPPPrefetchSubresource",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLCPPPrefetchSubresourceAsync,
             "LCPPPrefetchSubresourceAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHttpDiskCachePrewarming,
             "HttpDiskCachePrewarming",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kHttpDiskCachePrewarmingMaxUrlLength,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_max_url_length",
                   1024);

BASE_FEATURE_PARAM(int,
                   kHttpDiskCachePrewarmingHistorySize,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_history_size",
                   1024);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kHttpDiskCachePrewarmingReprewarmPeriod,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_reprewarm_period",
                   base::Minutes(10));

BASE_FEATURE_PARAM(bool,
                   kHttpDiskCachePrewarmingTriggerOnNavigation,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_trigger_on_navigation",
                   true);

BASE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingTriggerOnPointerDownOrHover,
    &kHttpDiskCachePrewarming,
    "http_disk_cache_prewarming_trigger_on_pointer_down_or_hover",
    true);

BASE_FEATURE_PARAM(
    bool,
    kHttpDiskCachePrewarmingUseReadAndDiscardBodyOption,
    &kHttpDiskCachePrewarming,
    "http_disk_cache_prewarming_use_read_and_discard_body_option",
    false);

BASE_FEATURE_PARAM(bool,
                   kHttpDiskCachePrewarmingSkipDuringBrowserStartup,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_skip_during_browser_startup",
                   true);

BASE_FEATURE_PARAM(int,
                   kHttpDiskCachePrewarmingSlidingWindowSize,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_sliding_window_size",
                   1000);

BASE_FEATURE_PARAM(int,
                   kHttpDiskCachePrewarmingMaxHistogramBuckets,
                   &kHttpDiskCachePrewarming,
                   "http_disk_cache_prewarming_max_histogram_buckets",
                   10);

BASE_FEATURE(kLegacyParsingOfXContentTypeOptions,
             "LegacyParsingOfXContentTypeOptions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A feature to reduce the set of resources fetched by No-State Prefetch.
BASE_FEATURE(kLightweightNoStatePrefetch,
             "LightweightNoStatePrefetch",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLinkPreview, "LinkPreview", base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<LinkPreviewTriggerType>::Option
    link_preview_trigger_type_options[] = {
        {LinkPreviewTriggerType::kAltClick, "alt_click"},
        {LinkPreviewTriggerType::kAltHover, "alt_hover"},
        {LinkPreviewTriggerType::kLongPress, "long_press"}};
BASE_FEATURE_ENUM_PARAM(LinkPreviewTriggerType,
                        kLinkPreviewTriggerType,
                        &kLinkPreview,
                        "trigger_type",
                        LinkPreviewTriggerType::kAltHover,
                        &link_preview_trigger_type_options);

// A feature to control whether the loading phase should be extended beyond
// First Meaningful Paint by a configurable buffer.
BASE_FEATURE(kLoadingPhaseBufferTimeAfterFirstMeaningfulPaint,
             "LoadingPhaseBufferTimeAfterFirstMeaningfulPaint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Makes network loading tasks unfreezable so that they can be processed while
// the page is frozen.
BASE_FEATURE(kLoadingTasksUnfreezable,
             "LoadingTasksUnfreezable",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
             "LogUnexpectedIPCPostedToBackForwardCachedDocuments",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow low latency canvas 2D to be in overlay (generally meaning scanned out
// directly to display), even if regular canvas are not in overlay
// (Canvas2DImageChromium is disabled).
BASE_FEATURE(kLowLatencyCanvas2dImageChromium,
             "LowLatencyCanvas2dImageChromium",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
);

// Allow low latency WebGL to be in overlay (generally meaning scanned out
// directly to display), even if regular canvas are not in overlay
// (WebGLImageChromium is disabled).
BASE_FEATURE(kLowLatencyWebGLImageChromium,
             "LowLatencyWebGLImageChromium",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLowPriorityAsyncScriptExecution,
             "LowPriorityAsyncScriptExecution",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kTimeoutForLowPriorityAsyncScriptExecution,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_timeout",
                   base::Milliseconds(0));

// kLowPriorityAsyncScriptExecution will be disabled after document elapsed more
// than |low_pri_async_exec_feature_limit|. Zero value means no limit.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kLowPriorityAsyncScriptExecutionFeatureLimitParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_feature_limit",
                   base::Seconds(0));

// kLowPriorityAsyncScriptExecution will be applied only for cross site scripts.
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionCrossSiteOnlyParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_cross_site_only",
                   false);

BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionMainFrameOnlyParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_main_frame_only",
                   false);

// kLowPriorityAsyncScriptExecution will exclude scripts that influence LCP
// element.
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionExcludeLcpInfluencersParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_exclude_lcp_influencers",
                   false);

// kLowPriorityAsyncScriptExecution will exclude scripts on pages where LCP
// element isn't directly embedded in HTML.
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionDisableWhenLcpNotInHtmlParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_disable_when_lcp_not_in_html",
                   false);

// kLowPriorityAsyncScriptExecution will use the specified priority as a lower
// task priority.
const base::FeatureParam<AsyncScriptPrioritisationType>::Option
    async_script_prioritisation_types[] = {
        {AsyncScriptPrioritisationType::kHigh, "high"},
        {AsyncScriptPrioritisationType::kLow, "low"},
        {AsyncScriptPrioritisationType::kBestEffort, "best_effort"},
};
BASE_FEATURE_ENUM_PARAM(AsyncScriptPrioritisationType,
                        kLowPriorityAsyncScriptExecutionLowerTaskPriorityParam,
                        &kLowPriorityAsyncScriptExecution,
                        "low_pri_async_exec_lower_task_priority",
                        AsyncScriptPrioritisationType::kBestEffort,
                        &async_script_prioritisation_types);
// kLowPriorityAsyncScriptExecution will change evaluation schedule for the
// specified target.
BASE_FEATURE_ENUM_PARAM(AsyncScriptExperimentalSchedulingTarget,
                        kLowPriorityAsyncScriptExecutionTargetParam,
                        &kLowPriorityAsyncScriptExecution,
                        "low_pri_async_exec_target",
                        AsyncScriptExperimentalSchedulingTarget::kBoth,
                        &async_script_experimental_scheduling_targets);
// If true, kLowPriorityAsyncScriptExecution will not change the script
// evaluation timing for the non parser inserted script.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionExcludeNonParserInsertedParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_exclude_non_parser_inserted",
    false);
// If true, kLowPriorityAsyncScriptExecution will not change the script
// evaluation timing for the scripts that were added via document.write().
BASE_FEATURE_PARAM(bool,
                   kLowPriorityAsyncScriptExecutionExcludeDocumentWriteParam,
                   &kLowPriorityAsyncScriptExecution,
                   "low_pri_async_exec_exclude_document_write",
                   false);

// kLowPriorityAsyncScriptExecution will be opted-out when FetchPriorityHint is
// low.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutLowFetchPriorityHintParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_opt_out_low_fetch_priority_hint",
    false);
// kLowPriorityAsyncScriptExecution will be opted-out when FetchPriorityHint is
// auto.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutAutoFetchPriorityHintParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_opt_out_auto_fetch_priority_hint",
    false);
// kLowPriorityAsyncScriptExecution will be opted-out when FetchPriorityHint is
// high.
BASE_FEATURE_PARAM(
    bool,
    kLowPriorityAsyncScriptExecutionOptOutHighFetchPriorityHintParam,
    &kLowPriorityAsyncScriptExecution,
    "low_pri_async_exec_opt_out_high_fetch_priority_hint",
    false);

BASE_FEATURE(kMixedContentAutoupgrade,
             "AutoupgradeMixedContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMemoryCacheStrongReference,
             "MemoryCacheStrongReference",
// Finch study showed no improvement on Android for strong memory cache.
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE_PARAM(int,
                   kMemoryCacheStrongReferenceTotalSizeThresholdParam,
                   &kMemoryCacheStrongReference,
                   "memory_cache_strong_ref_total_size_threshold",
                   15 * 1024 * 1024);
BASE_FEATURE_PARAM(int,
                   kMemoryCacheStrongReferenceResourceSizeThresholdParam,
                   &kMemoryCacheStrongReference,
                   "memory_cache_strong_ref_resource_size_threshold",
                   3 * 1024 * 1024);

BASE_FEATURE(kMemoryPurgeOnFreezeLimit,
             "MemoryPurgeOnFreezeLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMemorySaverModeRenderTuning,
             "MemorySaverModeRenderTuning",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kAvailableMemoryThresholdParamMb,
                   &kMemorySaverModeRenderTuning,
                   "available_memory_threshold_mb",
                   740);

BASE_FEATURE(kMHTML_Improvements,
             "MHTML_Improvements",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to control the collection of anchor element metrics (crbug.com/856683).
// If kNavigationPredictor is enabled, then metrics of anchor elements
// in the first viewport after the page load and the metrics of the clicked
// anchor element will be extracted and recorded.
// Note that the desktop roll out is being done separately from android. See
// https://crbug.com/40258405
BASE_FEATURE(kNavigationPredictor,
             "NavigationPredictor",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kPredictorTrafficClientEnabledPercent,
                   &kNavigationPredictor,
                   "traffic_client_enabled_percent",
#if BUILDFLAG(IS_ANDROID)
                   100
#else
                   5
#endif
);

// Used to control the collection of new viewport related anchor element
// metrics. Metrics will not be recorded if either this or kNavigationPredictor
// is disabled.
BASE_FEATURE(kNavigationPredictorNewViewportFeatures,
             "NavigationPredictorNewViewportFeatures",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kNoForcedFrameUpdatesForWebTests,
             "NoForcedFrameUpdatesForWebTests",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNoThrottlingVisibleAgent,
             "NoThrottlingVisibleAgent",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNoThrowForCSPBlockedWorker,
             "NoThrowForCSPBlockedWorker",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kOpenAllUrlsOrFilesOnDrop,
             "OpenAllUrlsOrFilesOnDrop",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizeHTMLElementUrls,
             "OptimizeHTMLElementUrls",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(size_t,
                   kDocumentURLCacheSize,
                   &kOptimizeHTMLElementUrls,
                   "cache_size",
                   100);

BASE_FEATURE(kOriginAgentClusterDefaultEnabled,
             "OriginAgentClusterDefaultEnable",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOriginTrialStateHostApplyFeatureDiff,
             "OriginTrialStateHostApplyFeatureDiff",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable defer commits to avoid flash of unstyled content, for all navigations.
BASE_FEATURE(kPaintHolding, "PaintHolding", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kParkableImagesToDisk,
             "ParkableImagesToDisk",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// A parameter to exclude or not exclude CanvasFontCache from
// PartialLowModeOnMidRangeDevices. This is used to see how
// CanvasFontCache affects graphics smoothness and renderer memory usage.
BASE_FEATURE_PARAM(bool,
                   kPartialLowEndModeExcludeCanvasFontCache,
                   &base::features::kPartialLowEndModeOnMidRangeDevices,
                   "exclude-canvas-font-cache",
                   false);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

// When enabled, this flag partitions the :visited link hashtable by
// <link url, top-level site, frame origin>
BASE_FEATURE(kPartitionVisitedLinkDatabase,
             "PartitionVisitedLinkDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the use of the PaintCache for Path2D objects that are rasterized
// out of process.  Has no effect when kCanvasOopRasterization is disabled.
BASE_FEATURE(kPath2DPaintCache,
             "Path2DPaintCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDedicatedWorkerAblationStudyEnabled,
             "DedicatedWorkerAblationStudyEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kDedicatedWorkerStartDelayInMs,
                   &kDedicatedWorkerAblationStudyEnabled,
                   "DedicatedWorkerStartDelayInMs",
                   0);

BASE_FEATURE(kUseAncestorRenderFrameForWorker,
             "UseAncestorRenderFrameForWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrecompileInlineScripts,
             "PrecompileInlineScripts",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether we should composite a PLSA (paint layer scrollable area) even if it
// means losing lcd text.
BASE_FEATURE(kPreferCompositingToLCDText,
             "PreferCompositingToLCDText",
// On Android we never have LCD text. On Chrome OS we prefer composited
// scrolling for better scrolling performance.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPrefetchFontLookupTables,
             "PrefetchFontLookupTables",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);
#endif

// Prefetch request properties are updated to be privacy-preserving. See
// crbug.com/988956.
BASE_FEATURE(kPrefetchPrivacyChanges,
             "PrefetchPrivacyChanges",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreloadingHeuristicsMLModel,
             "PreloadingHeuristicsMLModel",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelTimerStartDelay,
                   &kPreloadingHeuristicsMLModel,
                   "timer_start_delay",
                   0);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelTimerInterval,
                   &kPreloadingHeuristicsMLModel,
                   "timer_interval",
                   100);
// The default max hover time of 10s covers the 98th percentile of hovering
// cases that are relevant to the model.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kPreloadingModelMaxHoverTime,
                   &kPreloadingHeuristicsMLModel,
                   "max_hover_time",
                   base::Seconds(10));
BASE_FEATURE_PARAM(bool,
                   kPreloadingModelEnactCandidates,
                   &kPreloadingHeuristicsMLModel,
                   "enact_candidates",
                   false);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelPrefetchModerateThreshold,
                   &kPreloadingHeuristicsMLModel,
                   "prefetch_moderate_threshold",
                   50);
BASE_FEATURE_PARAM(int,
                   kPreloadingModelPrerenderModerateThreshold,
                   &kPreloadingHeuristicsMLModel,
                   "prerender_moderate_threshold",
                   50);

BASE_FEATURE(kPreloadingViewportHeuristics,
             "PreloadingViewportHeuristics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Firing pagehide events for intended prerender cancellation. See
// crbug.com/353628449 for more details.
BASE_FEATURE(kPageHideEventForPrerender2,
             "PageHideEventForPrerender2",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrerender2MaxNumOfRunningSpeculationRules[] =
    "max_num_of_running_speculation_rules";

BASE_FEATURE(kPrerender2MemoryControls,
             "Prerender2MemoryControls",
             base::FEATURE_ENABLED_BY_DEFAULT);
const char kPrerender2MemoryThresholdParamName[] = "memory_threshold_in_mb";
const char kPrerender2MemoryAcceptablePercentOfSystemMemoryParamName[] =
    "acceptable_percent_of_system_memory";

BASE_FEATURE(kPrerender2EarlyDocumentLifecycleUpdate,
             "Prerender2EarlyDocumentLifecycleUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable limiting previews loading hints to specific resource types.
BASE_FEATURE(kPreviewsResourceLoadingHintsSpecificResourceTypes,
             "PreviewsResourceLoadingHintsSpecificResourceTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kPrewarmDefaultFontFamilies,
             "PrewarmDefaultFontFamilies",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(bool,
                   kPrewarmStandard,
                   &kPrewarmDefaultFontFamilies,
                   "prewarm_standard",
                   false);
BASE_FEATURE_PARAM(bool,
                   kPrewarmFixed,
                   &kPrewarmDefaultFontFamilies,
                   "prewarm_fixed",
                   false);
BASE_FEATURE_PARAM(bool,
                   kPrewarmSerif,
                   &kPrewarmDefaultFontFamilies,
                   "prewarm_serif",
                   true);
BASE_FEATURE_PARAM(bool,
                   kPrewarmSansSerif,
                   &kPrewarmDefaultFontFamilies,
                   "prewarm_sans_serif",
                   true);
BASE_FEATURE_PARAM(bool,
                   kPrewarmCursive,
                   &kPrewarmDefaultFontFamilies,
                   "prewarm_cursive",
                   false);
BASE_FEATURE_PARAM(bool,
                   kPrewarmFantasy,
                   &kPrewarmDefaultFontFamilies,
                   "prewarm_fantasy",
                   false);
#endif

// Enables the Private Aggregation API.
BASE_FEATURE(kPrivateAggregationApi,
             "PrivateAggregationApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Selectively allows the JavaScript API to be disabled in just one of the
// contexts. The Protected Audience param's name has not been updated (from
// "fledge") for consistency across versions
BASE_FEATURE_PARAM(bool,
                   kPrivateAggregationApiEnabledInSharedStorage,
                   &kPrivateAggregationApi,
                   "enabled_in_shared_storage",
                   /*default_value=*/true);
BASE_FEATURE_PARAM(bool,
                   kPrivateAggregationApiEnabledInProtectedAudience,
                   &kPrivateAggregationApi,
                   "enabled_in_fledge",
                   /*default_value=*/true);

// Selectively allows the debug mode to be disabled while leaving the rest of
// the API in place. If disabled, any `enableDebugMode()` calls will essentially
// have no effect.
BASE_FEATURE_PARAM(bool,
                   kPrivateAggregationApiDebugModeEnabledAtAll,
                   &kPrivateAggregationApi,
                   "debug_mode_enabled_at_all",
                   /*default_value=*/true);

// Adds some additional functionality (new reserved event types, base values)
// to things enabled by
// kPrivateAggregationApiEnabledInProtectedAudience.
BASE_FEATURE(kPrivateAggregationApiProtectedAudienceAdditionalExtensions,
             "PrivateAggregationApiProtectedAudienceAdditionalExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProcessHtmlDataImmediately,
             "ProcessHtmlDataImmediately",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelyChildFrame,
                   &kProcessHtmlDataImmediately,
                   "child",
                   false);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelyFirstChunk,
                   &kProcessHtmlDataImmediately,
                   "first",
                   false);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelyMainFrame,
                   &kProcessHtmlDataImmediately,
                   "main",
                   false);

BASE_FEATURE_PARAM(bool,
                   kProcessHtmlDataImmediatelySubsequentChunks,
                   &kProcessHtmlDataImmediately,
                   "rest",
                   false);

BASE_FEATURE(kProduceCompileHints2,
             "ProduceCompileHints2",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kProduceCompileHintsNoiseLevel,
                   &kProduceCompileHints2,
                   "noise-probability",
                   0.5);
BASE_FEATURE_PARAM(double,
                   kProduceCompileHintsDataProductionLevel,
                   &kProduceCompileHints2,
                   "data-production-probability",
                   0.005);
BASE_FEATURE(kForceProduceCompileHints,
             "ForceProduceCompileHints",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kConsumeCompileHints,
             "ConsumeCompileHints",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLocalCompileHints,
             "LocalCompileHints",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kQuoteEmptySecChUaStringHeadersConsistently,
             "QuoteEmptySecChUaStringHeadersConsistently",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Reduce the amount of information in the default 'referer' header for
// cross-origin requests.
BASE_FEATURE(kReducedReferrerGranularity,
             "ReducedReferrerGranularity",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kUserAgentFrozenBuildVersion,
                   &kReduceUserAgentMinorVersion,
                   "build_version",
                   "0");

// Whether `blink::MemoryCache` and `blink::ResourceFetcher` release their
// strong references to resources on memory pressure.
BASE_FEATURE(kReleaseResourceStrongReferencesOnMemoryPressure,
             "ReleaseResourceStrongReferencesOnMemoryPressure",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether `blink::Resource` deletes its decoded data on memory pressure.
BASE_FEATURE(kReleaseResourceDecodedDataOnMemoryPressure,
             "ReleaseResourceDecodedDataOnMemoryPressure",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRenderBlockingFonts,
             "RenderBlockingFonts",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(int,
                   kMaxBlockingTimeMsForRenderBlockingFonts,
                   &features::kRenderBlockingFonts,
                   "max-blocking-time",
                   1500);

BASE_FEATURE_PARAM(int,
                   kMaxFCPDelayMsForRenderBlockingFonts,
                   &features::kRenderBlockingFonts,
                   "max-fcp-delay",
                   100);

BASE_FEATURE(kRenderSizeInScoreAdBrowserSignals,
             "RenderSizeInScoreAdBrowserSignals",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingInputEvents,
             "ResamplingInputEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kResamplingScrollEvents,
             "ResamplingScrollEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kResourceFetcherStoresStrongReferences,
             "ResourceFetcherStoresStrongReferences",
             base::FEATURE_DISABLED_BY_DEFAULT);

// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BASE_FEATURE(kSafelistFTPToRegisterProtocolHandler,
             "SafelistFTPToRegisterProtocolHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);

// https://html.spec.whatwg.org/multipage/system-state.html#safelisted-scheme
BASE_FEATURE(kSafelistPaytoToRegisterProtocolHandler,
             "SafelistPaytoToRegisterProtocolHandler",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPausePagesPerBrowsingContextGroup,
             "PausePagesPerBrowsingContextGroup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowHudDisplayForPausedPages,
             "ShowHudDisplayForPausedPages",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls script streaming for http and https scripts.
BASE_FEATURE(kScriptStreaming,
             "ScriptStreaming",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables script streaming for non-http scripts.
BASE_FEATURE(kScriptStreamingForNonHTTP,
             "ScriptStreamingForNonHTTP",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables sending Sec-Purpose: "prefetch" header for rel="prefetch".
BASE_FEATURE(kSecPurposePrefetchHeaderRelPrefetch,
             "SecPurposePrefetchHeaderRelPrefetch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSelectiveInOrderScript,
             "SelectiveInOrderScript",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSelectiveInOrderScriptTarget,
             "SelectiveInOrderScriptTarget",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kSelectiveInOrderScriptAllowList{
    &kSelectiveInOrderScriptTarget, "allow_list", ""};

// When enabled, the SubresourceFilter receives calls from the ResourceLoader
// to perform additional checks against any aliases found from DNS CNAME records
// for the requested URL.
BASE_FEATURE(kSendCnameAliasesToSubresourceFilterFromRenderer,
             "SendCnameAliasesToSubresourceFilterFromRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Experiment of the delay from navigation to starting an update of a service
// worker's script.
BASE_FEATURE(kServiceWorkerUpdateDelay,
             "ServiceWorkerUpdateDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, beacons (and friends) have ResourceLoadPriority::kLow,
// not ResourceLoadPriority::kVeryLow.
BASE_FEATURE(kSetLowPriorityForBeacon,
             "SetLowPriorityForBeacon",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, calling setInterval(..., 0) will not clamp to 1ms.
// Tracking bug: https://crbug.com/402694.
BASE_FEATURE(kSetIntervalWithoutClamp,
             "SetIntervalWithoutClamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageWorkletSharedBackingThreadImplementation,
             "SharedStorageWorkletSharedBackingThreadImplementation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageCreateWorkletCustomDataOrigin,
             "SharedStorageCreateWorkletCustomDataOrigin",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageSelectURLSavedQueries,
             "SharedStorageSelectURLSavedQueries",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSharedStorageAPIEnableWALForDatabase,
             "SharedStorageAPIEnableWALForDatabase",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kSkipTouchEventFilterTypeParamName[] = "type";
const char kSkipTouchEventFilterTypeParamValueDiscrete[] = "discrete";
const char kSkipTouchEventFilterTypeParamValueAll[] = "all";
const char kSkipTouchEventFilterFilteringProcessParamName[] =
    "skip_filtering_process";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowser[] = "browser";
const char kSkipTouchEventFilterFilteringProcessParamValueBrowserAndRenderer[] =
    "browser_and_renderer";

BASE_FEATURE(kSpeculativeImageDecodes,
             "SpeculativeImageDecodes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable service worker warming-up feature. (https://crbug.com/1431792)
BASE_FEATURE(kSpeculativeServiceWorkerWarmUp,
             "SpeculativeServiceWorkerWarmUp",
             base::FEATURE_ENABLED_BY_DEFAULT);

// kSpeculativeServiceWorkerWarmUp warms up service workers up to this max
// count.
BASE_FEATURE_PARAM(int,
                   kSpeculativeServiceWorkerWarmUpMaxCount,
                   &kSpeculativeServiceWorkerWarmUp,
                   "sw_warm_up_max_count",
                   2);

// Duration to keep worker warmed-up.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSpeculativeServiceWorkerWarmUpDuration,
                   &kSpeculativeServiceWorkerWarmUp,
                   "sw_warm_up_duration",
                   base::Minutes(5));

// Warms up service workers when a pointerover event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerover{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerover", true};

// Warms up service workers when a pointerdown event is triggered on an anchor.
const base::FeatureParam<bool> kSpeculativeServiceWorkerWarmUpOnPointerdown{
    &kSpeculativeServiceWorkerWarmUp, "sw_warm_up_on_pointerdown", true};

// (crbug.com/352578800): Enables building a sysnthetic response by
// ServiceWorker. For navigation requests, the pre-learned static response
// header is returned in parallel with dispatching the network request.
BASE_FEATURE(kServiceWorkerSyntheticResponse,
             "ServiceWorkerSyntheticResponse",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Define the allowed websites to enable SyntheticResponse. Allowed urls are
// expected to be passed as a comma separated string.
// e.g. https://example1.test,https://example2.test/foo?query
BASE_FEATURE_PARAM(std::string,
                   kServiceWorkerSyntheticResponseAllowedUrls,
                   &kServiceWorkerSyntheticResponse,
                   "allowed_urls",
                   "");

// If enabled, force renderer process foregrounded from CommitNavigation to
// DOMContentLoad (crbug/351953350).
BASE_FEATURE(kBoostRenderProcessForLoading,
             "BoostRenderProcessForLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

// An empty json array means that this feature is applied unconditionally. If
// specified, it means that the specified URLs will be the target of the new
// behavior.
BASE_FEATURE_PARAM(std::string,
                   kBoostRenderProcessForLoadingTargetUrls,
                   &kBoostRenderProcessForLoading,
                   "target_urls",
                   "[]");

// If true is specified, kBoostRenderProcessForLoading feature also prioritizes
// the renderer process that is used for prerendering. This is a part of an
// ablation study. See https://crbug.com/351953350.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizePrerendering,
                   &kBoostRenderProcessForLoading,
                   "prioritize_prerendering",
                   false);

// If true is specified, kBoostRenderProcessForLoading feature only prioritizes
// the renderer process that is used for prerendering. This is a part of an
// ablation study. See https://crbug.com/351953350.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizePrerenderingOnly,
                   &kBoostRenderProcessForLoading,
                   "prioritize_prerendering_only",
                   false);

// If true is specified, kBoostRenderProcessForLoading feature also prioritizes
// the renderer process for restore cases.
BASE_FEATURE_PARAM(bool,
                   kBoostRenderProcessForLoadingPrioritizeRestore,
                   &kBoostRenderProcessForLoading,
                   "prioritize_restore",
                   false);

// Freeze scheduler task queues in background after allowed grace time.
// "stop" is a legacy name.
BASE_FEATURE(kStopInBackground,
             "stop-in-background",
// b/248036988 - Disable this for Chromecast on Android builds to prevent apps
// that play audio in the background from stopping.
#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CAST_ANDROID) && \
    !BUILDFLAG(IS_DESKTOP_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Reduces the work done during renderer initialization.
BASE_FEATURE(kStreamlineRendererInit,
             "StreamlineRendererInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSubSampleWindowProxyUsageMetrics,
             "SubSampleWindowProxyUsageMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedBodyLoader,
             "ThreadedBodyLoader",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThreadedPreloadScanner,
             "ThreadedPreloadScanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kThrottleFrameRateOnInitialization,
                   &features::kRenderBlockingFullFrameRate,
                   "throttle-frame-rate-on-initialization",
                   false);

// Enable throttling of fetch() requests from service workers in the
// installing state.  The limit of 3 was chosen to match the limit
// in background main frames.  In addition, trials showed that this
// did not cause excessive timeouts and resulted in a net improvement
// in successful install rate on some platforms.
BASE_FEATURE(kThrottleInstallingServiceWorker,
             "ThrottleInstallingServiceWorker",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kInstallingServiceWorkerOutstandingThrottledLimit,
                   &kThrottleInstallingServiceWorker,
                   "limit",
                   3);

// Throttles Javascript timer wake ups of unimportant frames (cross origin
// frames with small proportion of the page's visible area and no user
// activation) on foreground pages.
BASE_FEATURE(kThrottleUnimportantFrameTimers,
             "ThrottleUnimportantFrameTimers",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Interval between Javascript timer wake ups for unimportant frames (small
// cross origin frames with no user activation) when the
// "ThrottleUnimportantFrameTimers" feature is enabled.
BASE_FEATURE_PARAM(int,
                   kUnimportantFrameTimersThrottledWakeUpIntervalMills,
                   &features::kThrottleUnimportantFrameTimers,
                   "unimportant_frame_timers_throttled_wake_up_interval_millis",
                   32);
// The percentage of the page's visible area below which a frame is considered
// small. Only small frames can be throttled by ThrottleUnimportantFrameTimers.
BASE_FEATURE_PARAM(int,
                   kLargeFrameSizePercentThreshold,
                   &features::kThrottleUnimportantFrameTimers,
                   "large_frame_size_percent_threshold",
                   75);

BASE_FEATURE(kTimedHTMLParserBudget,
             "TimedHTMLParserBudget",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Changes behavior of User-Agent Client Hints to send blank headers when the
// User-Agent string is overridden, instead of disabling the headers altogether.
BASE_FEATURE(kUACHOverrideBlank,
             "UACHOverrideBlank",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the body of `EmulateLoadStartedForInspector` is executed only
// once per Resource per ResourceFetcher, and thus duplicated network load
// entries in DevTools caused by `EmulateLoadStartedForInspector` are removed.
// https://crbug.com/1502591
BASE_FEATURE(kEmulateLoadStartedForInspectorOncePerResource,
             "kEmulateLoadStartedForInspectorOncePerResource",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the usage of unload handlers causes a blocklisted reason for
// BFCache. The purpose is to capture their source location.
BASE_FEATURE(kUnloadBlocklisted,
             "UnloadBlocklisted",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When BeginMainFrame() is throttled, whether input-related BeginMainFrame()s
// are marked urgent, and thus unthtrottled.
BASE_FEATURE(kUrgentMainFrameForInput,
             "UrgentMainFrameForInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses page viewport instead of frame viewport in the Largest Contentful Paint
// heuristic where images occupying the full viewport are ignored.
BASE_FEATURE(kUsePageViewportInLCP,
             "UsePageViewportInLCP",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabling this will cause parkable strings to use Snappy for compression iff
// kCompressParkableStrings is enabled.
BASE_FEATURE(kUseSnappyForParkableStrings,
             "UseSnappyForParkableStrings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the zstd compression algorithm for ParkableString compression.
BASE_FEATURE(kUseZstdForParkableStrings,
             "UseZstdForParkableStrings",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allows to tweak the compression / speed tradeoff.
//
// According to https://github.com/facebook/zstd, level 1 should be:
// - Much faster than zlib, with a similar compression ratio
// - Roughly as fast as snappy, with a better compression ratio.
//
// And even -3 should be smaller *and* faster than snappy.
BASE_FEATURE_PARAM(int,
                   kZstdCompressionLevel,
                   &features::kUseZstdForParkableStrings,
                   "compression_level",
                   1);

BASE_FEATURE(kUseThreadPoolForMediaStreamVideoTaskRunner,
             "UseThreadPoolForMediaStreamVideoTaskRunner",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVSyncDecoding,
             "VSyncDecoding",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kVSyncDecodingHiddenOccludedTickDuration,
                   &kVSyncDecoding,
                   "occluded_tick_duration",
                   base::Hertz(10));

BASE_FEATURE(kVSyncEncoding,
             "VSyncEncoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcUseCaptureBeginTimestamp,
             "WebRtcUseCaptureBeginTimestamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebRtcAudioSinkUseTimestampAligner,
             "WebRtcAudioSinkUseTimestampAligner",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable borderless mode for desktop PWAs. go/borderless-mode
BASE_FEATURE(kWebAppBorderless,
             "WebAppBorderless",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls scope extensions feature in web apps. Controls parsing of
// "scope_extensions" field in web app manifests. See explainer for more
// information:
// https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
BASE_FEATURE(kWebAppEnableScopeExtensions,
             "WebAppEnableScopeExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls scope extensions feature in web apps. Enables parsing of "site"
// entries in "scope_extensions" field in web app manifests. See explainer for
// more information:
// https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
BASE_FEATURE(kWebAppEnableScopeExtensionsBySite,
             "WebAppEnableScopeExtensionsBySite",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls parsing of the "lock_screen" dictionary field and its "start_url"
// entry in web app manifests.  See explainer for more information:
// https://github.com/WICG/lock-screen/
// Note: the lock screen API and OS integration is separately controlled by
// the content feature `kWebLockScreenApi`.
BASE_FEATURE(kWebAppManifestLockScreen,
             "WebAppManifestLockScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow denormals in AudioWorklet and ScriptProcessorNode, to enable strict
// JavaScript denormal compliance.  See https://crbug.com/382005099.
BASE_FEATURE(kWebAudioAllowDenormalInProcessing,
             "WebAudioAllowDenormalInProcessing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Parameters can be used to control to which latency hints the feature is
// applied.
BASE_FEATURE_PARAM(bool,
                   kWebAudioBypassOutputBufferingInteractive,
                   &kWebAudioBypassOutputBuffering,
                   "latency_interactive",
                   true);
BASE_FEATURE_PARAM(bool,
                   kWebAudioBypassOutputBufferingBalanced,
                   &kWebAudioBypassOutputBuffering,
                   "latency_balanced",
                   true);
BASE_FEATURE_PARAM(bool,
                   kWebAudioBypassOutputBufferingPlayback,
                   &kWebAudioBypassOutputBuffering,
                   "latency_playback",
                   true);
BASE_FEATURE_PARAM(bool,
                   kWebAudioBypassOutputBufferingExact,
                   &kWebAudioBypassOutputBuffering,
                   "latency_exact",
                   true);

// This feature flag controls whether the WebAudio destination resampler is
// bypassed. When enabled, if the WebAudio context's sample rate differs from
// the hardware's sample rate, the resampling step that normally occurs within
// the WebAudio destination node is skipped. This allows the AudioService to
// handle any necessary resampling, potentially reducing latency and overhead.
BASE_FEATURE(kWebAudioRemoveAudioDestinationResampler,
             "WebAudioRemoveAudioDestinationResampler",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

/// Enables cache-aware WebFonts loading. See https://crbug.com/570205.
// The feature is disabled on Android for WebView API issue discussed at
// https://crbug.com/942440.
BASE_FEATURE(kWebFontsCacheAwareTimeoutAdaption,
             "WebFontsCacheAwareTimeoutAdaption",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kWebRtcCombinedNetworkAndWorkerThread,
             "WebRtcCombinedNetworkAndWorkerThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/355256378): OpenH264 for encoding and FFmpeg for H264 decoding
// should be detangled such that software decoding can be enabled without
// software encoding.
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && \
    BUILDFLAG(ENABLE_OPENH264)
// Run-time feature for the |rtc_use_h264| encoder/decoder.
BASE_FEATURE(kWebRtcH264WithOpenH264FFmpeg,
             "WebRTC-H264WithOpenH264FFmpeg",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) &&
        // BUILDFLAG(ENABLE_OPENH264)

// Causes WebRTC to replace host ICE candidate IP addresses with generated
// names ending in ".local" and resolve them using mDNS.
// http://crbug.com/878465
BASE_FEATURE(kWebRtcHideLocalIpsWithMdns,
             "WebRtcHideLocalIpsWithMdns",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Causes WebRTC to not set the color space of video frames on the receive side
// in case it's unspecified. Otherwise we will guess that the color space is
// BT709. http://crbug.com/1129243
BASE_FEATURE(kWebRtcIgnoreUnspecifiedColorSpace,
             "WebRtcIgnoreUnspecifiedColorSpace",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Instructs WebRTC to honor the Min/Max Video Encode Accelerator dimensions.
BASE_FEATURE(kWebRtcUseMinMaxVEADimensions,
             "WebRtcUseMinMaxVEADimensions",
// TODO(crbug.com/1008491): enable other platforms.
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Kill switch for crbug.com/407785197.
BASE_FEATURE(kWebRtcAllowDataChannelRecordingInWebrtcInternals,
             "WebRtcAllowDataChannelRecordingInWebrtcInternals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for https://crbug.com/338955051.
BASE_FEATURE(kWebUSBTransferSizeLimit,
             "WebUSBTransferSizeLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables small accelerated canvases for webview (crbug.com/1004304)
BASE_FEATURE(kWebviewAccelerateSmallCanvases,
             "WebviewAccelerateSmallCanvases",
             base::FEATURE_DISABLED_BY_DEFAULT);

// WorkerThread termination procedure (prepare and shutdown) runs sequentially
// in the same task without calling another cross thread post task.
// Kill switch for crbug.com/409059706.
BASE_FEATURE(kWorkerThreadSequentialShutdown,
             "WorkerThreadSequentialShutdown",
             base::FEATURE_ENABLED_BY_DEFAULT);

// WorkerThread termination respects the current thread termination request.
BASE_FEATURE(kWorkerThreadRespectTermRequest,
             "WorkerThreadRespectTermRequest",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNoReferrerForPreloadFromSubresource,
             "NoReferrerForPreloadFromSubresource",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When adding new features or constants for features, please keep the features
// sorted by identifier name (e.g. `kAwesomeFeature`), and the constants for
// that feature grouped with the associated feature.
//
// When defining feature params for auto-generated features (e.g. from
// `RuntimeEnabledFeatures)`, they should still be ordered in this section based
// on the identifier name of the generated feature.

// ---------------------------------------------------------------------------
// Helper functions for querying feature status. Please define any features or
// constants for features in the section above.

bool IsAllowURNsInIframeEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kAllowURNsInIframes);
}

bool IsCanvas2DHibernationEnabled() {
  return base::FeatureList::IsEnabled(features::kCanvas2DHibernation);
}

bool DisplayWarningDeprecateURNIframesUseFencedFrames() {
  return base::FeatureList::IsEnabled(
      blink::features::kDisplayWarningDeprecateURNIframesUseFencedFrames);
}

bool IsFencedFramesEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kFencedFrames);
}

bool IsParkableStringsToDiskEnabled() {
  // Always enabled as soon as compression is enabled.
  return base::FeatureList::IsEnabled(kCompressParkableStrings);
}

bool IsParkableImagesToDiskEnabled() {
  return base::FeatureList::IsEnabled(kParkableImagesToDisk);
}

bool IsSetIntervalWithoutClampEnabled() {
  return base::FeatureList::IsEnabled(features::kSetIntervalWithoutClamp);
}

bool IsUnloadBlocklisted() {
  return base::FeatureList::IsEnabled(kUnloadBlocklisted);
}

bool ParkableStringsUseSnappy() {
  return base::FeatureList::IsEnabled(kUseSnappyForParkableStrings);
}

bool IsKeepAliveURLLoaderServiceEnabled() {
  return base::FeatureList::IsEnabled(kKeepAliveInBrowserMigration) ||
         base::FeatureList::IsEnabled(kFetchLaterAPI);
}

bool IsLinkPreviewTriggerTypeEnabled(LinkPreviewTriggerType type) {
  return base::FeatureList::IsEnabled(blink::features::kLinkPreview) &&
         type == blink::features::kLinkPreviewTriggerType.Get();
}

// DO NOT ADD NEW FEATURES HERE.
//
// The section above is for helper functions for querying feature status. The
// section below should have nothing. Please add new features in the giant block
// of features that already exist in this file, trying to keep newly-added
// features in sorted order.
//
// DO NOT ADD NEW FEATURES HERE.

}  // namespace blink::features
