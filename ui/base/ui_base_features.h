// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_FEATURES_H_
#define UI_BASE_UI_BASE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// Keep sorted!

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kExperimentalFlingAnimation);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kFocusFollowsCursor);
#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSettingsShowsPerKeyboardSettings);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kInputMethodSettingsUiUpdate);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWindowsScrollingPersonality);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsPercentBasedScrollingEnabled();
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kSystemCaptionStyle);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kSystemKeyboardLock);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kUiCompositorScrollWithLayers);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUiGpuRasterizationEnabled();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kElasticOverscroll);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kApplyNativeOccludedRegionToWindowTracker);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kApplyNativeOcclusionToCompositor);
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorType[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorTypeRelease[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kApplyNativeOcclusionToCompositorTypeThrottle[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kCalculateNativeWinOcclusion);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kInputPaneOnScreenKeyboard);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kPointerEventsForTouch);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kScreenPowerListenerForNativeWinOcclusion);
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kTSFImeSupport);

// Returns true if the system should use WM_POINTER events for touch events.
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingWMPointerForTouch();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kImprovedKeyboardShortcuts);
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsImprovedKeyboardShortcutsEnabled();
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kDeprecateAltBasedSixPack);
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsDeprecateAltBasedSixPackEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kTouchTextEditingRedesign);
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsTouchTextEditingRedesignEnabled();

// Used to enable forced colors mode for web content.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kForcedColors);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsForcedColorsEnabled();

// Used to enable the eye-dropper in the refresh color-picker.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kEyeDropper);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsEyeDropperEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSystemCursorSizeSupported);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsSystemCursorSizeSupported();

// Used to enable the common select popup.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kUseCommonSelectPopup);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUseCommonSelectPopupEnabled();

// Used to enable keyboard accessible tooltips in in-page content
// (i.e., inside Blink). See
// ::views::features::kKeyboardAccessibleTooltipInViews for
// keyboard-accessible tooltips in Views UI.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kKeyboardAccessibleTooltip);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsKeyboardAccessibleTooltipEnabled();

// Used to enable gesture changes for notifications.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kNotificationGesturesUpdate);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsNotificationGesturesUpdateEnabled();

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kHandwritingGesture);

COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kDeprecateAltClick);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsDeprecateAltClickEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kNotificationsIgnoreRequireInteraction);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsNotificationsIgnoreRequireInteractionEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kShortcutCustomizationApp);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsShortcutCustomizationAppEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kShortcutCustomization);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsShortcutCustomizationEnabled();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kLacrosResourcesFileSharing);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kAlwaysConfirmComposition);

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSupportF11AndF12KeyShortcuts);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool AreF11AndF12ShortcutsEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Indicates whether DrmOverlayManager should used the synchronous API to
// perform pageflip tests.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kSynchronousPageFlipTesting);

COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsSynchronousPageFlipTestingEnabled();

// The type of predictor to use for the resampling events. These values are
// used as the 'predictor' feature param for
// |blink::features::kResamplingScrollEvents|.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictorNameLsq[];
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictorNameKalman[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictorNameLinearFirst[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictorNameLinearSecond[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictorNameLinearResampling[];
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictorNameEmpty[];

// Enables resampling of scroll events using an experimental latency of +3.3ms
// instead of the original -5ms.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kResamplingScrollEventsExperimentalPrediction);

// The type of prediction used. TimeBased uses a fixed timing, FramesBased uses
// a ratio of the vsync refresh rate. The timing/ratio can be changed on the
// command line through a `latency` param.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kPredictionTypeTimeBased[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictionTypeFramesBased[];
// The default values for `latency`
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictionTypeDefaultTime[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kPredictionTypeDefaultFramesRatio[];

// The type of filter to use for filtering events. These values are used as the
// 'filter' feature param for |blink::features::kFilteringScrollPrediction|.
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kFilterNameEmpty[];
COMPONENT_EXPORT(UI_BASE_FEATURES) extern const char kFilterNameOneEuro[];

// Android only feature, for swipe to move cursor.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kSwipeToMoveCursor);

// Enables UI debugging tools such as shortcuts.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kUIDebugTools);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsSwipeToMoveCursorEnabled();

// Enables Raw Draw.
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kRawDraw);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsUsingRawDraw();
COMPONENT_EXPORT(UI_BASE_FEATURES) double RawDrawTileSizeFactor();
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsRawDrawUsingMSAA();

COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kStylusSpecificTapSlop);

// This feature indicates that this device is approved for utilizing variable
// refresh rates. This flag is added by cros-config and not exposed in the
// chrome://flags UI.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kVariableRefreshRateAvailable);
// Enables the variable refresh rate feature for Borealis gaming only. If this
// flag is set by Finch, it requires the availability flag to also be true. If
// this flag is overridden by the user, then the availability flag is ignored.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kEnableVariableRefreshRate);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsVariableRefreshRateEnabled();
// Enables the variable refresh rate feature at all times.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kEnableVariableRefreshRateAlwaysOn);
COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsVariableRefreshRateAlwaysOn();

// Fixes b/265853952.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWaylandKeepSelectionFix);
// Fixes b/267944900.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kWaylandCancelComposition);

COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kLacrosColorManagement);
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool IsLacrosColorManagementEnabled();

// If enabled, Customize Chrome will be an option in the Unified Side Panel
// when on the New Tab Page.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kCustomizeChromeSidePanel);

// If both kCustomizeChromeSidePanelNoChromeRefresh2023 and
// kCustomizeChromeSidePanel are enabled, Customize Chrome will be an option in
// the Unified Side Panel when on the New Tab Page but Chrome Refresh 2023 will
// be disabled. This state is useful in the Customize Chrome holdback
// experiment.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kCustomizeChromeSidePanelNoChromeRefresh2023);

// Returns true if Customize Chrome is configured such that it supports Chrome
// Refresh 2023.
COMPONENT_EXPORT(UI_BASE_FEATURES)
bool CustomizeChromeSupportsChromeRefresh2023();

// Exposed for testing and flags integration. For actual checks please use
// IsChromeRefresh2023().
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kChromeRefresh2023);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kChromeRefreshSecondary2023);

enum class ChromeRefresh2023NTBVariation {
  kGM2Full = 0,
  kGM3OldIconNoBackground = 1,
  kGM3OldIconWithBackground = 2,
  kGM3NewIconNoBackground = 3,
  kGM3NewIconWithBackground = 4,
  kNoChoice = 5,
};
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kChromeRefresh2023NTB);
COMPONENT_EXPORT(UI_BASE_FEATURES)
extern const char kChromeRefresh2023NTBVariationKey[];
COMPONENT_EXPORT(UI_BASE_FEATURES)
ChromeRefresh2023NTBVariation GetChromeRefresh2023NTB();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kChromeRefresh2023TopChromeFont);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsChromeRefresh2023();

// Exposed for testing and flags integration. For actual checks please use
// IsChromeWebuiRefresh2023().
COMPONENT_EXPORT(UI_BASE_FEATURES) BASE_DECLARE_FEATURE(kChromeRefresh2023);
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kChromeWebuiRefresh2023);

COMPONENT_EXPORT(UI_BASE_FEATURES) bool IsChromeWebuiRefresh2023();

// If you are not Omnibox developer, you don't need to query CR2023 level.
// Otherwise, please ensure that Omnibox features are guarded by an OR; enabling
// either CR2023 Level2 or the feature-specific features should enable them
// respectively.
enum class ChromeRefresh2023Level {
  // ChromeRefresh2023 is disabled.
  kDisabled = 0,
  // Enables ChromeRefresh2023 without Omnibox changes.
  // Omnibox features can still be independently enabled by feature-specific
  // Features.
  kLevel1 = 1,
  // Enables ChromeRefresh2023 with full Omnibox changes.
  // Omnibox feature-specific features can be enabled on top of Level2 to gain
  // additional control using their FeatureParams.
  kLevel2 = 2,
};
COMPONENT_EXPORT(UI_BASE_FEATURES)
ChromeRefresh2023Level GetChromeRefresh2023Level();

COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kBubbleMetricsApi);

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kMacClipboardWriteImageWithPng);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_APPLE)
// Font Smoothing, a CoreText technique, simulates optical sizes to enhance text
// readability at smaller scales. In practice, it leads to an increased
// perception of text weight, creating discrepancies between renderings in UX
// design tools and actual macOS displays. This feature is only effective when
// ChromeRefresh2023 is enabled.
COMPONENT_EXPORT(UI_BASE_FEATURES)
BASE_DECLARE_FEATURE(kCr2023MacFontSmoothing);
#endif

}  // namespace features

#endif  // UI_BASE_UI_BASE_FEATURES_H_
