// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ui_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/webui/flags/feature_entry.h"
#include "ui/base/ui_base_features.h"

namespace features {

// Enables the tab dragging fallback when full window dragging is not supported
// by the platform (e.g. Wayland). See https://crbug.com/896640
BASE_FEATURE(kAllowWindowDragUsingSystemDragDrop,
             "AllowWindowDragUsingSystemDragDrop",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the use of WGC for the Eye Dropper screen capture.
BASE_FEATURE(kAllowEyeDropperWGCScreenCapture,
             "AllowEyeDropperWGCScreenCapture",
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_WIN)
);

// When enabled, clicks outside the omnibox and its popup will close an open
// omnibox popup.
BASE_FEATURE(kCloseOmniboxPopupOnInactiveAreaClick,
             "CloseOmniboxPopupOnInactiveAreaClick",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables updated copy and modified behavior for the default browser prompt.
BASE_FEATURE(kDefaultBrowserPromptRefresh,
             "DefaultBrowserPromptRefresh",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

const base::FeatureParam<bool> kShowDefaultBrowserInfoBar{
    &kDefaultBrowserPromptRefresh, "show_info_bar", true};

const base::FeatureParam<bool> kShowDefaultBrowserAppMenuItem{
    &kDefaultBrowserPromptRefresh, "show_app_menu_item", true};

const base::FeatureParam<base::TimeDelta> kRepromptDuration{
    &kDefaultBrowserPromptRefresh, "reprompt_duration", base::Days(60)};

const base::FeatureParam<int> kMaxPromptCount{&kDefaultBrowserPromptRefresh,
                                              "max_prompt_count", -1};

// Create new Extensions app menu option (removing "More Tools -> Extensions")
// with submenu to manage extensions and visit chrome web store.
BASE_FEATURE(kExtensionsMenuInAppMenu,
             "ExtensionsMenuInAppMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsExtensionMenuInRootAppMenu() {
  return base::FeatureList::IsEnabled(kExtensionsMenuInAppMenu);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables the feature to remove the last confirmation dialog when relaunching
// to update Chrome.
BASE_FEATURE(kFewerUpdateConfirmations,
             "FewerUpdateConfirmations",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Controls whether we use a different UX for simple extensions overriding
// settings.
BASE_FEATURE(kLightweightExtensionOverrideConfirmations,
             "LightweightExtensionOverrideConfirmations",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Preloads a WebContents with a Top Chrome WebUI on BrowserView initialization,
// so that it can be shown instantly at a later time when necessary.
BASE_FEATURE(kPreloadTopChromeWebUI,
             "PreloadTopChromeWebUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPreloadTopChromeWebUIModeName[] = "preload-mode";
const char kPreloadTopChromeWebUIModePreloadOnWarmupName[] =
    "preload-on-warmup";
const char kPreloadTopChromeWebUIModePreloadOnMakeContentsName[] =
    "preload-on-make-contents";
constexpr base::FeatureParam<PreloadTopChromeWebUIMode>::Option
    kPreloadTopChromeWebUIModeOptions[] = {
        {PreloadTopChromeWebUIMode::kPreloadOnWarmup,
         kPreloadTopChromeWebUIModePreloadOnWarmupName},
        {PreloadTopChromeWebUIMode::kPreloadOnMakeContents,
         kPreloadTopChromeWebUIModePreloadOnMakeContentsName},
};
const base::FeatureParam<PreloadTopChromeWebUIMode> kPreloadTopChromeWebUIMode{
    &kPreloadTopChromeWebUI, kPreloadTopChromeWebUIModeName,
    PreloadTopChromeWebUIMode::kPreloadOnMakeContents,
    &kPreloadTopChromeWebUIModeOptions};

const char kPreloadTopChromeWebUISmartPreloadName[] = "smart-preload";
const base::FeatureParam<bool> kPreloadTopChromeWebUISmartPreload{
    &kPreloadTopChromeWebUI, kPreloadTopChromeWebUISmartPreloadName, false};

const char kPreloadTopChromeWebUIDelayPreloadName[] = "delay-preload";
const base::FeatureParam<bool> kPreloadTopChromeWebUIDelayPreload{
    &kPreloadTopChromeWebUI, kPreloadTopChromeWebUIDelayPreloadName, false};

const char kPreloadTopChromeWebUIExcludeOriginsName[] = "exclude-origins";
const base::FeatureParam<std::string> kPreloadTopChromeWebUIExcludeOrigins{
    &kPreloadTopChromeWebUI, kPreloadTopChromeWebUIExcludeOriginsName, ""};

// Enables exiting browser fullscreen (users putting the browser itself into the
// fullscreen mode via the browser UI or shortcuts) with press-and-hold Esc.
#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPressAndHoldEscToExitBrowserFullscreen,
             "PressAndHoldEscToExitBrowserFullscreen",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// When enabled, a scrim is shown behind window modal dialogs to cover the
// entire browser window. This gives user a visual cue that the browser window
// is not interactable.
BASE_FEATURE(kScrimForBrowserWindowModal,
             "ScrimForBrowserWindowModal",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, a scrim is shown behind tab modal dialogs to cover the content
// area. This gives user a visual cue that the content area is not interactable.
BASE_FEATURE(KScrimForTabModal,
             "ScrimForTabModal",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSideBySide, "SideBySide", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSidePanelResizing,
             "SidePanelResizing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabDuplicateMetrics,
             "TabDuplicateMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables buttons when scrolling the tabstrip https://crbug.com/951078
BASE_FEATURE(kTabScrollingButtonPosition,
             "TabScrollingButtonPosition",
             base::FEATURE_ENABLED_BY_DEFAULT);
const char kTabScrollingButtonPositionParameterName[] = "buttonPosition";

// Enables tabs to be frozen when collapsed.
// https://crbug.com/1110108
BASE_FEATURE(kTabGroupsCollapseFreezing,
             "TabGroupsCollapseFreezing",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables preview images in tab-hover cards.
// https://crbug.com/928954
BASE_FEATURE(kTabHoverCardImages,
             "TabHoverCardImages",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

const char kTabHoverCardImagesNotReadyDelayParameterName[] =
    "page_not_ready_delay";
const char kTabHoverCardImagesLoadingDelayParameterName[] =
    "page_loading_delay";
const char kTabHoverCardImagesLoadedDelayParameterName[] = "page_loaded_delay";
const char kTabHoverCardImagesCrossfadePreviewAtParameterName[] =
    "crossfade_preview_at";
const char kTabHoverCardAdditionalMaxWidthDelay[] =
    "additional_max_width_delay";

BASE_FEATURE(kTabOrganization,
             "TabOrganization",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabOrganization() {
  return base::FeatureList::IsEnabled(features::kTabOrganization);
}

BASE_FEATURE(kTabstripDeclutter,
             "TabstripDeclutter",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kTabstripDeclutterStaleThresholdDuration{
        &kTabstripDeclutter, "stale_threshold_duration", base::Days(7)};

const base::FeatureParam<base::TimeDelta> kTabstripDeclutterTimerInterval{
    &kTabstripDeclutter, "declutter_timer_interval", base::Minutes(10)};

const base::FeatureParam<base::TimeDelta> kTabstripDeclutterNudgeTimerInterval{
    &kTabstripDeclutter, "nudge_timer_interval", base::Minutes(6 * 60)};

bool IsTabstripDeclutterEnabled() {
  return base::FeatureList::IsEnabled(features::kTabstripDeclutter);
}

BASE_FEATURE(kTabstripDedupe,
             "TabstripDedupe",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsTabstripDedupeEnabled() {
  return IsTabstripDeclutterEnabled() &&
         base::FeatureList::IsEnabled(features::kTabstripDedupe);
}

BASE_FEATURE(kTabOrganizationAppMenuItem,
             "TabOrganizationAppMenuItem",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationModelStrategy,
             "TabOrganizationModelStrategy",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationUserInstruction,
             "TabOrganizationUserInstruction",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabOrganizationEnableNudgeForEnterprise,
             "TabOrganizationEnableNudgeForEnterprise",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kTabOrganizationTriggerPeriod{
    &kTabOrganization, "trigger_period", base::Hours(6)};

const base::FeatureParam<double> kTabOrganizationTriggerBackoffBase{
    &kTabOrganization, "backoff_base", 2.0};

const base::FeatureParam<double> kTabOrganizationTriggerThreshold{
    &kTabOrganization, "trigger_threshold", 7.0};

const base::FeatureParam<double> kTabOrganizationTriggerSensitivityThreshold{
    &kTabOrganization, "trigger_sensitivity_threshold", 0.5};

const base::FeatureParam<bool> KTabOrganizationTriggerDemoMode{
    &kTabOrganization, "trigger_demo_mode", false};

// Enables creating a web app window when tearing off a tab with a url
// controlled by a web app.
BASE_FEATURE(kTearOffWebAppTabOpensWebAppWindow,
             "TearOffWebAppTabOpensWebAppWindow",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !defined(ANDROID)
BASE_FEATURE(kToolbarPinning,
             "ToolbarPinning",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsToolbarPinningEnabled() {
  return base::FeatureList::IsEnabled(kToolbarPinning);
}

BASE_FEATURE(kPinnedCastButton,
             "PinnedCastButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPinnableDownloadsButton,
             "PinnableDownloadsButton",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Enables enterprise profile badging for managed profiles on the toolbar
// avatar. On managed profiles, a "Work" or "School" label will be used in the
// toolbar.
BASE_FEATURE(kEnterpriseProfileBadgingForAvatar,
             "EnterpriseProfileBadgingForAvatar",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables enterprise profile badging for managed profiles on the toolbar avatar
// and in the profile menu. On managed profiles, a building icon will be used as
// a badge in the profile menu.
BASE_FEATURE(kEnterpriseProfileBadgingForMenu,
             "EnterpriseProfileBadgingForMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables enterprise profile badging for managed profiles on the toolbar avatar
// and in the profile menu when the policies are set. This acts as a kill
// switch. This has no effect if `kEnterpriseProfileBadging` is enabled.
BASE_FEATURE(kEnterpriseProfileBadgingPolicies,
             "EnterpriseProfileBadgingPolicies",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables showing the EnterpriseCustomLabel` instead of the cloud policy
// manager in the managed disclaimer "Managed by..." in the profile and app
// menus.
BASE_FEATURE(kEnterpriseManagementDisclaimerUsesCustomLabel,
             "EnterpriseManagementDisclaimerUsesCustomLabel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseUpdatedProfileCreationScreen,
             "EnterpriseUpdatedProfileCreationScreen",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kManagedProfileRequiredInterstitial,
             "ManagedProfileRequiredInterstitial",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables a web-based tab strip. See https://crbug.com/989131. Note this
// feature only works when the ENABLE_WEBUI_TAB_STRIP buildflag is enabled.
BASE_FEATURE(kWebUITabStrip,
             "WebUITabStrip",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// The default value of this flag is aligned with platform behavior to handle
// context menu with touch.
// TODO(crbug.com/40796475): Enable this flag for all platforms after launch.
BASE_FEATURE(kWebUITabStripContextMenuAfterTap,
             "WebUITabStripContextMenuAfterTap",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kViewsFirstRunDialog,
             "ViewsFirstRunDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kViewsJSAppModalDialog,
             "ViewsJSAppModalDialog",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kUsePortalAccentColor,
             "UsePortalAccentColor",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPageSpecificDataDialogRelatedInstalledAppsSection,
             "PageSpecificDataDialogRelatedInstalledAppsSection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableManagementPromotionBanner,
             "EnableManagementPromotionBanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePolicyPromotionBanner,
             "EnablePolicyPromotionBanner",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInlineFullscreenPerfExperiment,
             "InlineFullscreenPerfExperiment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPageActionsMigration,
             "PageActionsMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCompositorLoadingAnimations,
             "CompositorLoadingAnimations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFedCmContinueWithoutName,
             "FedCmContinueWithoutName",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kByDateHistoryInSidePanel,
             "ByDateHistoryInSidePanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
