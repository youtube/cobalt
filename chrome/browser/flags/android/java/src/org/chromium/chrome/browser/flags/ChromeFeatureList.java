// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.base.BaseFeatures;
import org.chromium.base.FeatureMap;
import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.base.MutableIntParamWithSafeDefault;
import org.chromium.base.MutableParamWithSafeDefault;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.BooleanCachedFeatureParam;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.components.cached_flags.DoubleCachedFeatureParam;
import org.chromium.components.cached_flags.IntCachedFeatureParam;
import org.chromium.components.cached_flags.StringCachedFeatureParam;

import java.util.List;
import java.util.Map;

/**
 * A list of feature flags exposed to Java.
 *
 * <p>This class lists flags exposed to Java as String constants. They should match
 * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc.
 *
 * <p>This class also provides convenience methods to access values of flags and their field trial
 * parameters through {@link ChromeFeatureMap}.
 *
 * <p>Chrome-layer {@link CachedFlag}s and {@link MutableFlagWithSafeDefault}s are instantiated
 * here, as well as {@link CachedFeatureParam}s and {@link MutableParamWithSafeDefault}s.
 */
@NullMarked
public abstract class ChromeFeatureList {

    /** Prevent instantiation. */
    private ChromeFeatureList() {}

    /**
     * Convenience method to check Chrome-layer feature flags, see {@link
     * FeatureMap#isEnabledInNative(String)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static boolean isEnabled(String featureName) {
        return ChromeFeatureMap.isEnabled(featureName);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeature(String, String)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static String getFieldTrialParamByFeature(String featureName, String paramName) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamByFeature(featureName, paramName);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeatureAsBoolean(String, String, boolean)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        return ChromeFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsBoolean(featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeatureAsInt(String, String, int)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        return ChromeFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsInt(featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamByFeatureAsDouble(String, String, double)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        return ChromeFeatureMap.getInstance()
                .getFieldTrialParamByFeatureAsDouble(featureName, paramName, defaultValue);
    }

    /**
     * Convenience method to get Chrome-layer feature field trial params, see {@link
     * FeatureMap#getFieldTrialParamsForFeature(String)}.
     *
     * <p>Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in chrome/browser/flags/android/chrome_feature_list.cc
     */
    public static Map<String, String> getFieldTrialParamsForFeature(String featureName) {
        return ChromeFeatureMap.getInstance().getFieldTrialParamsForFeature(featureName);
    }

    public static BooleanCachedFeatureParam newBooleanCachedFeatureParam(
            String featureName, String variationName, boolean defaultValue) {
        return new BooleanCachedFeatureParam(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static DoubleCachedFeatureParam newDoubleCachedFeatureParam(
            String featureName, String variationName, double defaultValue) {
        return new DoubleCachedFeatureParam(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static IntCachedFeatureParam newIntCachedFeatureParam(
            String featureName, String variationName, int defaultValue) {
        return new IntCachedFeatureParam(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    public static StringCachedFeatureParam newStringCachedFeatureParam(
            String featureName, String variationName, String defaultValue) {
        return new StringCachedFeatureParam(
                ChromeFeatureMap.getInstance(), featureName, variationName, defaultValue);
    }

    private static CachedFlag newCachedFlag(String featureName, boolean defaultValue) {
        return newCachedFlag(featureName, defaultValue, defaultValue);
    }

    private static CachedFlag newCachedFlag(
            String featureName, boolean defaultValue, boolean defaultValueInTests) {
        return new CachedFlag(
                ChromeFeatureMap.getInstance(), featureName, defaultValue, defaultValueInTests);
    }

    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return ChromeFeatureMap.getInstance().mutableFlagWithSafeDefault(featureName, defaultValue);
    }

    // Feature names.
    /* Alphabetical: */
    public static final String ACT_USER_BYPASS_UX = "ActUserBypassUx";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY =
            "AdaptiveButtonInTopToolbarPageSummary";
    public static final String ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2 =
            "AdaptiveButtonInTopToolbarCustomizationV2";
    public static final String ALLOW_NEW_INCOGNITO_TAB_INTENTS = "AllowNewIncognitoTabIntents";
    public static final String ALWAYS_BLOCK_3PCS_INCOGNITO = "AlwaysBlock3pcsIncognito";
    public static final String ANDROID_DISCONNECT_FILE_CHOOSER_ON_TAB_DEACTIVATE_KILL_SWITCH =
            "AndroidDisconnectFileChooserOnTabDeactivateKillSwitch";
    public static final String ANDROID_APP_INTEGRATION = "AndroidAppIntegration";
    public static final String ANDROID_APP_INTEGRATION_MODULE = "AndroidAppIntegrationModule";
    public static final String ANDROID_APP_INTEGRATION_V2 = "AndroidAppIntegrationV2";
    public static final String NEW_TAB_PAGE_CUSTOMIZATION = "NewTabPageCustomization";
    public static final String ANDROID_APP_INTEGRATION_WITH_FAVICON =
            "AndroidAppIntegrationWithFavicon";
    public static final String ANDROID_BOOKMARK_BAR = "AndroidBookmarkBar";
    public static final String ANDROID_BOTTOM_TOOLBAR = "AndroidBottomToolbar";
    public static final String ANDROID_DUMP_ON_SCROLL_WITHOUT_RESOURCE =
            "AndroidDumpOnScrollWithoutResource";
    public static final String ANDROID_ELEGANT_TEXT_HEIGHT = "AndroidElegantTextHeight";
    public static final String ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD =
            "AndroidNoVisibleHintForDifferentTLD";
    public static final String ANDROID_OPEN_PDF_INLINE_BACKPORT = "AndroidOpenPdfInlineBackport";
    public static final String ANDROID_PDF_ASSIST_CONTENT = "AndroidPdfAssistContent";
    public static final String ANDROID_TAB_DECLUTTER = "AndroidTabDeclutter";
    public static final String ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE =
            "AndroidTabDeclutterArchiveAllButActiveTab";
    public static final String ANDROID_TAB_DECLUTTER_ARCHIVE_DUPLICATE_TABS =
            "AndroidTabDeclutterArchiveDuplicateTabs";
    public static final String ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS =
            "AndroidTabDeclutterArchiveTabGroups";
    public static final String ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH =
            "AndroidTabDeclutterDedupeTabIdsKillSwitch";
    public static final String ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH =
            "AndroidTabDeclutterRescueKillswitch";
    public static final String ANDROID_TAB_SKIP_SAVE_TABS_TASK_KILLSWITCH =
            "AndroidTabSkipSaveTabsTaskKillswitch";
    public static final String ANIMATED_IMAGE_DRAG_SHADOW = "AnimatedImageDragShadow";
    public static final String APP_SPECIFIC_HISTORY = "AppSpecificHistory";
    public static final String ASYNC_NOTIFICATION_MANAGER = "AsyncNotificationManager";
    public static final String AUTOFILL_ALLOW_NON_HTTP_ACTIVATION =
            "AutofillAllowNonHttpActivation";
    public static final String AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS =
            "AutofillEnableCardBenefitsForAmericanExpress";
    public static final String AUTOFILL_ENABLE_CARD_PRODUCT_NAME = "AutofillEnableCardProductName";
    public static final String AUTOFILL_ENABLE_LOCAL_IBAN = "AutofillEnableLocalIban";
    public static final String AUTOFILL_ENABLE_SERVER_IBAN = "AutofillEnableServerIban";
    public static final String AUTOFILL_ENABLE_CVC_STORAGE = "AutofillEnableCvcStorageAndFilling";
    public static final String AUTOFILL_ENABLE_PAYMENT_SETTINGS_CARD_PROMO_AND_SCAN_CARD =
            "AutofillEnablePaymentSettingsCardPromoAndScanCard";
    public static final String AUTOFILL_ENABLE_PAYMENT_SETTINGS_SERVER_CARD_SAVE =
            "AutofillEnablePaymentSettingsServerCardSave";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_ADDRESS_PROFILES =
            "AutofillEnableRankingFormulaAddressProfiles";
    public static final String AUTOFILL_ENABLE_RANKING_FORMULA_CREDIT_CARDS =
            "AutofillEnableRankingFormulaCreditCards";
    public static final String AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS =
            "AutofillEnableSyncingOfPixBankAccounts";
    public static final String AUTOFILL_ENABLE_VERVE_CARD_SUPPORT =
            "AutofillEnableVerveCardSupport";
    public static final String AUTOFILL_DEEP_LINK_AUTOFILL_OPTIONS =
            "AutofillDeepLinkAutofillOptions";
    public static final String AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID =
            "AutofillVirtualViewStructureAndroid";
    public static final String AUTOFILL_THIRD_PARTY_MODE_CONTENT_PROVIDER =
            "AutofillThirdPartyModeContentProvider";
    public static final String AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID =
            "AutofillEnableSecurityTouchEventFilteringAndroid";
    public static final String AUTOFILL_SYNC_EWALLET_ACCOUNTS = "AutofillSyncEwalletAccounts";
    public static final String AUTOMOTIVE_FULLSCREEN_TOOLBAR_IMPROVEMENTS =
            "AutomotiveFullscreenToolbarImprovements";
    public static final String AVOID_SELECTED_TAB_FOCUS_ON_LAYOUT_DONE_SHOWING =
            "AvoidSelectedTabFocusOnLayoutDoneShowing";
    public static final String BACKGROUND_THREAD_POOL = "BackgroundThreadPool";
    public static final String BACK_FORWARD_CACHE = "BackForwardCache";
    public static final String BACK_FORWARD_TRANSITIONS = "BackForwardTransitions";
    public static final String BCIV_BOTTOM_CONTROLS = "AndroidBcivBottomControls";
    public static final String BCIV_WITH_SUPPRESSION = "AndroidBcivWithSuppression";
    public static final String BCIV_ZERO_BROWSER_FRAMES = "AndroidBcivZeroBrowserFrames";
    public static final String BLOCK_INTENTS_WHILE_LOCKED = "BlockIntentsWhileLocked";
    public static final String BOARDING_PASS_DETECTOR = "BoardingPassDetector";
    public static final String BOOKMARK_PANE_ANDROID = "BookmarkPaneAndroid";
    public static final String BOTTOM_BROWSER_CONTROLS_REFACTOR = "BottomBrowserControlsRefactor";
    public static final String BROWSER_CONTROLS_EARLY_RESIZE = "BrowserControlsEarlyResize";
    public static final String BROWSER_CONTROLS_IN_VIZ = "AndroidBrowserControlsInViz";
    public static final String BROWSING_DATA_MODEL = "BrowsingDataModel";
    public static final String CACHE_ACTIVITY_TASKID = "CacheActivityTaskID";
    public static final String CAPTIVE_PORTAL_CERTIFICATE_LIST = "CaptivePortalCertificateList";
    public static final String CCT_ADAPTIVE_BUTTON = "CCTAdaptiveButton";
    public static final String CCT_AUTH_TAB = "CCTAuthTab";
    public static final String CCT_AUTH_TAB_DISABLE_ALL_EXTERNAL_INTENTS =
            "CCTAuthTabDisableAllExternalIntents";
    public static final String CCT_AUTH_TAB_ENABLE_HTTPS_REDIRECTS =
            "CCTAuthTabEnableHttpsRedirects";
    public static final String CCT_AUTO_TRANSLATE = "CCTAutoTranslate";
    public static final String CCT_BEFORE_UNLOAD = "CCTBeforeUnload";
    public static final String CCT_BLOCK_TOUCHES_DURING_ENTER_ANIMATION =
            "CCTBlockTouchesDuringEnterAnimation";
    public static final String CCT_CLIENT_DATA_HEADER = "CCTClientDataHeader";
    public static final String CCT_EARLY_NAV = "CCTEarlyNav";
    public static final String CCT_EPHEMERAL_MEDIA_VIEWER_EXPERIMENT =
            "CCTEphemeralMediaViewerExperiment";
    public static final String CCT_EPHEMERAL_MODE = "CCTEphemeralMode";
    public static final String CCT_EXTEND_TRUSTED_CDN_PUBLISHER = "CCTExtendTrustedCdnPublisher";
    public static final String CCT_FRE_IN_SAME_TASK = "CCTFreInSameTask";
    public static final String CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY =
            "CCTIncognitoAvailableToThirdParty";
    public static final String CCT_MINIMIZED = "CCTMinimized";
    public static final String CCT_MINIMIZED_ENABLED_BY_DEFAULT = "CCTMinimizedEnabledByDefault";
    public static final String CCT_NAVIGATIONAL_PREFETCH = "CCTNavigationalPrefetch";
    public static final String CCT_NESTED_SECURITY_ICON = "CCTNestedSecurityIcon";
    public static final String CCT_INTENT_FEATURE_OVERRIDES = "CCTIntentFeatureOverrides";

    public static final String CCT_GOOGLE_BOTTOM_BAR = "CCTGoogleBottomBar";
    public static final String CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS =
            "CCTGoogleBottomBarVariantLayouts";
    public static final String CCT_OPEN_IN_BROWSER_BUTTON_IF_ALLOWED_BY_EMBEDDER =
            "CCTOpenInBrowserButtonIfAllowedByEmbedder";
    public static final String CCT_OPEN_IN_BROWSER_BUTTON_IF_ENABLED_BY_EMBEDDER =
            "CCTOpenInBrowserButtonIfEnabledByEmbedder";
    // NOTE: Do not query this feature directly, use
    // WarmupManager#isCCTPrewarmTabFeatureEnabled.
    public static final String CCT_PREWARM_TAB = "CCTPrewarmTab";
    public static final String CCT_REPORT_PARALLEL_REQUEST_STATUS =
            "CCTReportParallelRequestStatus";
    public static final String CCT_REPORT_PRERENDER_EVENTS = "CCTReportPrerenderEvents";
    public static final String CCT_RESIZABLE_FOR_THIRD_PARTIES = "CCTResizableForThirdParties";
    public static final String CCT_REVAMPED_BRANDING = "CCTRevampedBranding";
    public static final String CCT_TAB_MODAL_DIALOG = "CCTTabModalDialog";
    public static final String CHANGE_UNFOCUSED_PRIORITY = "ChangeUnfocusedPriority";
    public static final String CHROME_SURVEY_NEXT_ANDROID = "ChromeSurveyNextAndroid";
    public static final String CHROME_SHARE_PAGE_INFO = "ChromeSharePageInfo";
    public static final String CLANK_STARTUP_LATENCY_INJECTION = "ClankStartupLatencyInjection";
    public static final String CLANK_WHATS_NEW = "ClankWhatsNew";
    public static final String CLEAR_BROWSING_DATA_ANDROID_SURVEY =
            "ClearBrowsingDataAndroidSurvey";
    public static final String COLLECT_ANDROID_FRAME_TIMELINE_METRICS =
            "CollectAndroidFrameTimelineMetrics";
    public static final String COMMAND_LINE_ON_NON_ROOTED = "CommandLineOnNonRooted";
    public static final String COMMERCE_MERCHANT_VIEWER = "CommerceMerchantViewer";
    public static final String CONTEXTUAL_PAGE_ACTIONS = "ContextualPageActions";
    public static final String CONTEXTUAL_PAGE_ACTION_READER_MODE =
            "ContextualPageActionReaderMode";
    public static final String CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION =
            "ContextualSearchDisableOnlineDetection";
    public static final String CONTEXTUAL_SEARCH_SUPPRESS_SHORT_VIEW =
            "ContextualSearchSuppressShortView";
    public static final String CONTEXT_MENU_SYS_UI_MATCHES_ACTIVITY =
            "ContextMenuSysUiMatchesActivity";
    public static final String CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS =
            "ContextMenuTranslateWithGoogleLens";
    public static final String CONTROLS_VISIBILITY_FROM_NAVIGATIONS =
            "ControlsVisibilityFromNavigations";
    public static final String CORMORANT = "Cormorant";
    public static final String CROSS_DEVICE_TAB_PANE_ANDROID = "CrossDeviceTabPaneAndroid";
    public static final String DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING =
            "DarkenWebsitesCheckboxInThemesSetting";
    public static final String DATA_SHARING = "DataSharing";
    public static final String COLLABORATION_FLOW_ANDROID = "CollaborationFlowAndroid";
    public static final String DATA_SHARING_JOIN_ONLY_FOR_TESTING = "DataSharingJoinOnly";
    public static final String DEFAULT_BROWSER_PROMO_ANDROID = "DefaultBrowserPromoAndroid";
    public static final String DEFAULT_BROWSER_PROMO_ANDROID2 = "DefaultBrowserPromoAndroid2";
    public static final String DEVICE_AUTHENTICATOR_ANDROIDX = "DeviceAuthenticatorAndroidx";
    public static final String DETAILED_LANGUAGE_SETTINGS = "DetailedLanguageSettings";
    public static final String DISABLE_INSTANCE_LIMIT = "DisableInstanceLimit";
    public static final String DISABLE_LIST_TAB_SWITCHER = "DisableListTabSwitcher";
    public static final String DISCO_FEED_ENDPOINT = "DiscoFeedEndpoint";
    public static final String DISPLAY_WILDCARD_CONTENT_SETTINGS =
            "DisplayWildcardInContentSettings";
    public static final String DRAW_CUTOUT_EDGE_TO_EDGE = "DrawCutoutEdgeToEdge";
    public static final String DRAW_KEY_NATIVE_EDGE_TO_EDGE = "DrawKeyNativeEdgeToEdge";
    public static final String DYNAMIC_SAFE_AREA_INSETS = "DynamicSafeAreaInsets";
    public static final String EDGE_TO_EDGE_BOTTOM_CHIN = "EdgeToEdgeBottomChin";
    public static final String EDGE_TO_EDGE_EVERYWHERE = "EdgeToEdgeEverywhere";
    public static final String EDGE_TO_EDGE_SAFE_AREA_CONSTRAINT = "EdgeToEdgeSafeAreaConstraint";
    public static final String EDGE_TO_EDGE_WEB_OPT_IN = "EdgeToEdgeWebOptIn";
    public static final String EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_CARD =
            "EducationalTipDefaultBrowserPromoCard";
    public static final String EDUCATIONAL_TIP_MODULE = "EducationalTipModule";
    public static final String EMPTY_TAB_LIST_ANIMATION_KILL_SWITCH =
            "EmptyTabListAnimationKillSwitch";
    public static final String ENABLE_DISCOUNT_INFO_API = "EnableDiscountInfoApi";
    public static final String ENABLE_X_AXIS_ACTIVITY_TRANSITION = "EnableXAxisActivityTransition";
    public static final String ENABLE_CLIPBOARD_DATA_CONTROLS_ANDROID =
            "EnableClipboardDataControlsAndroid";
    public static final String ESB_AI_STRING_UPDATE = "EsbAiStringUpdate";
    public static final String FEED_CONTAINMENT = "FeedContainment";
    public static final String FEED_FOLLOW_UI_UPDATE = "FeedFollowUiUpdate";
    public static final String FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE =
            "FeedImageMemoryCacheSizePercentage";
    public static final String FEED_LOADING_PLACEHOLDER = "FeedLoadingPlaceholder";
    public static final String FEED_LOW_MEMORY_IMPROVEMENT = "FeedLowMemoryImprovement";
    public static final String FEED_SHOW_SIGN_IN_COMMAND = "FeedShowSignInCommand";
    public static final String FILLING_PASSWORDS_FROM_ANY_ORIGIN = "FillingPasswordsFromAnyOrigin";
    public static final String FINGERPRINTING_PROTECTION_UX = "FingerprintingProtectionUx";
    public static final String FLOATING_SNACKBAR = "FloatingSnackbar";
    public static final String FORCE_BROWSER_CONTROLS_UPON_EXITING_FULLSCREEN =
            "ForceBrowserControlsUponExitingFullscreen";
    public static final String FORCE_DISABLE_EXTENDED_SYNC_PROMOS =
            "ForceDisableExtendedSyncPromos";
    public static final String FORCE_LIST_TAB_SWITCHER = "ForceListTabSwitcher";
    public static final String FORCE_STARTUP_SIGNIN_PROMO = "ForceStartupSigninPromo";
    public static final String FORCE_TRANSLUCENT_NOTIFICATION_TRAMPOLINE =
            "ForceTranslucentNotificationTrampoline";
    public static final String FORCE_WEB_CONTENTS_DARK_MODE = "WebContentsForceDark";
    public static final String FULLSCREEN_INSETS_API_MIGRATION = "FullscreenInsetsApiMigration";
    public static final String FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE =
            "FullscreenInsetsApiMigrationOnAutomotive";
    public static final String GROUP_NEW_TAB_WITH_PARENT = "GroupNewTabWithParent";
    public static final String LOCK_BACK_PRESS_HANDLER_AT_START = "LockBackPressHandlerAtStart";
    public static final String HASH_PREFIX_REAL_TIME_LOOKUPS =
            "SafeBrowsingHashPrefixRealTimeLookups";
    public static final String HIDE_TABLET_TOOLBAR_DOWNLOAD_BUTTON =
            "HideTabletToolbarDownloadButton";
    public static final String HISTORY_JOURNEYS = "Journeys";
    public static final String HISTORY_PANE_ANDROID = "HistoryPaneAndroid";
    public static final String HTTPS_FIRST_BALANCED_MODE = "HttpsFirstBalancedMode";
    public static final String INCOGNITO_SCREENSHOT = "IncognitoScreenshot";
    public static final String INSTALL_MESSAGE_THROTTLE = "InstallMessageThrottle";
    public static final String IP_PROTECTION_V1 = "IpProtectionV1";
    public static final String IP_PROTECTION_UX = "IpProtectionUx";
    public static final String LEGACY_TAB_STATE_DEPRECATION = "LegacyTabStateDeprecation";
    public static final String LENS_ON_QUICK_ACTION_SEARCH_WIDGET = "LensOnQuickActionSearchWidget";
    public static final String LINKED_SERVICES_SETTING = "LinkedServicesSetting";
    public static final String LOADING_PREDICTOR_LIMIT_PRECONNECT_SOCKET_COUNT =
            "LoadingPredictorLimitPreconnectSocketCount";
    public static final String LOGIN_DB_DEPRECATION_ANDROID = "LoginDbDeprecationAndroid";
    public static final String LOGO_POLISH = "LogoPolish";
    public static final String LOGO_POLISH_ANIMATION_KILL_SWITCH = "LogoPolishAnimationKillSwitch";
    public static final String LOOKALIKE_NAVIGATION_URL_SUGGESTIONS_UI =
            "LookalikeUrlNavigationSuggestionsUI";
    public static final String MAGIC_STACK_ANDROID = "MagicStackAndroid";
    public static final String MAYLAUNCHURL_USES_SEPARATE_STORAGE_PARTITION =
            "MayLaunchUrlUsesSeparateStoragePartition";
    public static final String MOST_VISITED_TILES_CUSTOMIZATION = "MostVisitedTilesCustomization";
    public static final String MOST_VISITED_TILES_RESELECT = "MostVisitedTilesReselect";
    public static final String MUlTI_INSTANCE_APPLICATION_STATUS_CLEANUP =
            "MultiInstanceApplicationStatusCleanup";
    public static final String NATIVE_PAGE_TRANSITION_HARDWARE_CAPTURE =
            "NativePageTransitionHardwareCapture";
    public static final String NAV_BAR_COLOR_ANIMATION = "NavBarColorAnimation";
    public static final String NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND =
            "NavBarColorMatchesTabBackground";
    public static final String NEW_TAB_SEARCH_ENGINE_URL_ANDROID = "NewTabSearchEngineUrlAndroid";
    public static final String NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2 =
            "NewTabPageAndroidTriggerForPrerender2";
    public static final String NOTIFICATION_ONE_TAP_UNSUBSCRIBE = "NotificationOneTapUnsubscribe";
    public static final String NOTIFICATION_PERMISSION_VARIANT = "NotificationPermissionVariant";
    public static final String NOTIFICATION_PERMISSION_BOTTOM_SHEET =
            "NotificationPermissionBottomSheet";
    public static final String NOTIFICATION_TRAMPOLINE = "NotificationTrampoline";
    public static final String OMAHA_MIN_SDK_VERSION_ANDROID = "OmahaMinSdkVersionAndroid";
    public static final String OMNIBOX_CACHE_SUGGESTION_RESOURCES =
            "OmniboxCacheSuggestionResources";
    public static final String AVOID_RELAYOUT_DURING_FOCUS_ANIMATION =
            "AvoidRelayoutDuringFocusAnimation";
    public static final String OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS =
            "OptimizationGuidePushNotifications";
    public static final String PAGE_INFO_ABOUT_THIS_SITE_MORE_LANGS =
            "PageInfoAboutThisSiteMoreLangs";
    public static final String PAGE_CONTENT_PROVIDER = "PageContentProvider";
    public static final String PAINT_PREVIEW_DEMO = "PaintPreviewDemo";
    public static final String PARTNER_CUSTOMIZATIONS_UMA = "PartnerCustomizationsUma";
    public static final String BIOMETRIC_AUTH_IDENTITY_CHECK = "BiometricAuthIdentityCheck";
    public static final String PASSWORD_FORM_GROUPED_AFFILIATIONS =
            "PasswordFormGroupedAffiliations";
    public static final String PASSWORD_LEAK_TOGGLE_MOVE = "PasswordLeakToggleMove";
    public static final String PERMISSION_DEDICATED_CPSS_SETTING_ANDROID =
            "PermissionDedicatedCpssSettingAndroid";
    public static final String PERMISSION_SITE_SETTING_RADIO_BUTTON =
            "PermissionSiteSettingsRadioButton";
    public static final String PLUS_ADDRESSES_ENABLED = "PlusAddressesEnabled";
    public static final String PLUS_ADDRESS_ANDROID_OPEN_GMS_CORE_MANAGEMENT_PAGE =
            "PlusAddressAndroidOpenGmsCoreManagementPage";
    public static final String POWER_SAVING_MODE_BROADCAST_RECEIVER_IN_BACKGROUND =
            "PowerSavingModeBroadcastReceiverInBackground";
    public static final String PREFETCH_BROWSER_INITIATED_TRIGGERS =
            "PrefetchBrowserInitiatedTriggers";
    public static final String PRERENDER2 = "Prerender2";
    public static final String PRECONNECT_ON_TAB_CREATION = "PreconnectOnTabCreation";
    public static final String PRICE_ANNOTATIONS = "PriceAnnotations";
    public static final String PRICE_CHANGE_MODULE = "PriceChangeModule";
    public static final String PRICE_INSIGHTS = "PriceInsights";
    public static final String PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE =
            "PrivacySandboxActivityTypeStorage";
    public static final String PRIVACY_SANDBOX_NOTICE_ACTION_DEBOUNCING_ANDROID =
            "PrivacySandboxNoticeActionDebouncingAndroid";
    public static final String PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS =
            "PrivacySandboxAdsApiUxEnhancements";
    public static final String PRIVACY_SANDBOX_ADS_NOTICE_CCT = "PrivacySandboxAdsNoticeCCT";
    public static final String PRIVACY_SANDBOX_EQUALIZED_PROMPT_BUTTONS =
            "PrivacySandboxEqualizedPromptButtons";
    public static final String PRIVACY_SANDBOX_FPS_UI = "PrivacySandboxFirstPartySetsUI";
    public static final String PRIVACY_SANDBOX_RELATED_WEBSITE_SETS_UI =
            "PrivacySandboxRelatedWebsiteSetsUi";
    public static final String PRIVACY_SANDBOX_SETTINGS_4 = "PrivacySandboxSettings4";
    public static final String PRIVACY_SANDBOX_SENTIMENT_SURVEY = "PrivacySandboxSentimentSurvey";
    public static final String PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY =
            "PrivacySandboxCctAdsNoticeSurvey";
    public static final String PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY =
            "PrivacySandboxAdTopicsContentParity";
    public static final String PRIVACY_SANDBOX_PRIVACY_POLICY = "PrivacySandboxPrivacyPolicy";
    public static final String PUSH_MESSAGING_DISALLOW_SENDER_IDS =
            "PushMessagingDisallowSenderIDs";
    public static final String PWA_UPDATE_DIALOG_FOR_ICON = "PwaUpdateDialogForIcon";
    public static final String PWA_RESTORE_UI = "PwaRestoreUi";
    public static final String PWA_RESTORE_UI_AT_STARTUP = "PwaRestoreUiAtStartup";
    public static final String QUICK_DELETE_ANDROID_SURVEY = "QuickDeleteAndroidSurvey";
    public static final String QUIET_NOTIFICATION_PROMPTS = "QuietNotificationPrompts";
    public static final String READALOUD = "ReadAloud";
    public static final String READALOUD_BACKGROUND_PLAYBACK = "ReadAloudBackgroundPlayback";
    public static final String READALOUD_IN_OVERFLOW_MENU_IN_CCT = "ReadAloudInOverflowMenuInCCT";
    public static final String READALOUD_IN_MULTI_WINDOW = "ReadAloudInMultiWindow";
    public static final String READALOUD_PLAYBACK = "ReadAloudPlayback";
    public static final String READALOUD_TAP_TO_SEEK = "ReadAloudTapToSeek";
    public static final String READALOUD_IPH_MENU_BUTTON_HIGHLIGHT_CCT =
            "ReadAloudIPHMenuButtonHighlightCCT";
    public static final String RECORD_SUPPRESSION_METRICS = "RecordSuppressionMetrics";
    public static final String REDIRECT_EXPLICIT_CTA_INTENTS_TO_EXISTING_ACTIVITY =
            "RedirectExplicitCTAIntentsToExistingActivity";
    public static final String REENGAGEMENT_NOTIFICATION = "ReengagementNotification";
    public static final String RELATED_SEARCHES_SWITCH = "RelatedSearchesSwitch";
    public static final String RELATED_SEARCHES_ALL_LANGUAGE = "RelatedSearchesAllLanguage";
    public static final String RENAME_JOURNEYS = "RenameJourneys";
    public static final String RIGHT_EDGE_GOES_FORWARD_GESTURE_NAV =
            "RightEdgeGoesForwardGestureNav";

    public static final String SAFETY_HUB = "SafetyHub";
    public static final String SAFETY_HUB_ANDROID_ORGANIC_SURVEY = "SafetyHubAndroidOrganicSurvey";
    public static final String SAFETY_HUB_ANDROID_SURVEY = "SafetyHubAndroidSurvey";
    public static final String SAFETY_HUB_ANDROID_SURVEY_V2 = "SafetyHubAndroidSurveyV2";
    public static final String SAFETY_HUB_FOLLOWUP = "SafetyHubFollowup";
    public static final String SAFETY_HUB_LOCAL_PASSWORDS_MODULE = "SafetyHubLocalPasswordsModule";
    public static final String SAFETY_HUB_MAGIC_STACK = "SafetyHubMagicStack";
    public static final String SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS =
            "SafetyHubWeakAndReusedPasswords";
    public static final String SAFE_BROWSING_DELAYED_WARNINGS = "SafeBrowsingDelayedWarnings";
    public static final String SAFE_BROWSING_EXTENDED_REPORTING_REMOVE_PREF_DEPENDENCY =
            "ExtendedReportingRemovePrefDependency";
    public static final String SEARCH_IN_CCT = "SearchInCCT";
    public static final String SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING =
            "SearchInCCTAlternateTapHandling";
    public static final String SETTINGS_SINGLE_ACTIVITY = "SettingsSingleActivity";
    public static final String SHARE_CUSTOM_ACTIONS_IN_CCT = "ShareCustomActionsInCCT";
    public static final String SHOW_NEW_TAB_ANIMATIONS = "ShowNewTabAnimations";
    public static final String SEARCH_RESUMPTION_MODULE_ANDROID = "SearchResumptionModuleAndroid";
    public static final String SEED_ACCOUNTS_REVAMP = "SeedAccountsRevamp";
    public static final String SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER =
            "SegmentationPlatformAndroidHomeModuleRanker";

    public static final String SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2 =
            "SegmentationPlatformAndroidHomeModuleRankerV2";

    public static final String SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER =
            "SegmentationPlatformEphemeralCardRanker";

    public static final String SEND_TAB_TO_SELF_V2 = "SendTabToSelfV2";
    public static final String SENSITIVE_CONTENT = "SensitiveContent";
    public static final String SENSITIVE_CONTENT_WHILE_SWITCHING_TABS =
            "SensitiveContentWhileSwitchingTabs";
    public static final String SHOW_WARNINGS_FOR_SUSPICIOUS_NOTIFICATIONS =
            "ShowWarningsForSuspiciousNotifications";
    public static final String SKIP_ISOLATED_SPLIT_PRELOAD = "SkipIsolatedSplitPreload";
    public static final String SMALLER_TAB_STRIP_TITLE_LIMIT = "SmallerTabStripTitleLimit";
    public static final String SMART_SUGGESTION_FOR_LARGE_DOWNLOADS =
            "SmartSuggestionForLargeDownloads";
    public static final String SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY =
            "SplitCacheByNetworkIsolationKey";
    public static final String START_SURFACE_RETURN_TIME = "StartSurfaceReturnTime";
    public static final String STOP_APP_INDEXING_REPORT = "StopAppIndexingReport";
    public static final String SUGGESTION_ANSWERS_COLOR_REVERSE = "SuggestionAnswersColorReverse";
    public static final String SUPPRESS_TOOLBAR_CAPTURES = "SuppressToolbarCaptures";
    public static final String SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END =
            "SuppressToolbarCapturesAtGestureEnd";
    public static final String ENABLE_BATCH_UPLOAD_FROM_SETTINGS = "EnableBatchUploadFromSettings";
    public static final String SYNC_ENABLE_PASSWORDS_SYNC_ERROR_MESSAGE_ALTERNATIVE =
            "SyncEnablePasswordsSyncErrorMessageAlternative";
    public static final String TAB_CLOSURE_METHOD_REFACTOR = "TabClosureMethodRefactor";
    public static final String TAB_DRAG_DROP_ANDROID = "TabDragDropAndroid";
    public static final String TAB_GROUP_PANE_ANDROID = "TabGroupPaneAndroid";
    public static final String TAB_GROUP_SYNC_ANDROID = "TabGroupSyncAndroid";
    public static final String TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH =
            "TabGroupSyncAutoOpenKillSwitch";
    public static final String TAB_RESUMPTION_MODULE_ANDROID = "TabResumptionModuleAndroid";
    public static final String TAB_STRIP_CONTEXT_MENU = "TabStripContextMenuAndroid";
    public static final String TAB_STRIP_GROUP_COLLAPSE = "TabStripGroupCollapseAndroid";
    public static final String TAB_STRIP_GROUP_DRAG_DROP_ANDROID = "TabStripGroupDragDropAndroid";
    public static final String TAB_STRIP_GROUP_REORDER = "TabStripGroupReorderAndroid";
    public static final String TAB_STRIP_INCOGNITO_MIGRATION = "TabStripIncognitoMigration";
    public static final String TAB_STRIP_LAYOUT_OPTIMIZATION = "TabStripLayoutOptimization";
    public static final String TAB_STRIP_TRANSITION_IN_DESKTOP_WINDOW =
            "TabStripTransitionInDesktopWindow";
    public static final String TAB_STATE_FLAT_BUFFER = "TabStateFlatBuffer";
    public static final String TAB_SWITCHER_COLOR_BLEND_ANIMATE = "TabSwitcherColorBlendAnimate";
    public static final String TAB_SWITCHER_FOREIGN_FAVICON_SUPPORT =
            "TabSwitcherForeignFaviconSupport";
    public static final String TAB_SWITCHER_FULL_NEW_TAB_BUTTON = "TabSwitcherFullNewTabButton";
    public static final String TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_FINISHING =
            "TabWindowManagerIndexReassignmentActivityFinishing";
    public static final String TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_IN_SAME_TASK =
            "TabWindowManagerIndexReassignmentActivityInSameTask";
    public static final String TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_NOT_IN_APP_TASKS =
            "TabWindowManagerIndexReassignmentActivityNotInAppTasks";
    public static final String TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH =
            "TabWindowManagerReportIndicesMismatch";
    public static final String TASK_MANAGER_CLANK = "TaskManagerClank";
    public static final String TEST_DEFAULT_DISABLED = "TestDefaultDisabled";
    public static final String TEST_DEFAULT_ENABLED = "TestDefaultEnabled";
    public static final String TILE_CONTEXT_MENU_REFACTOR = "TileContextMenuRefactor";
    public static final String TINKER_TANK_BOTTOM_SHEET = "TinkerTankBottomSheet";
    public static final String TOOLBAR_SCROLL_ABLATION = "AndroidToolbarScrollAblation";
    public static final String TRACE_BINDER_IPC = "TraceBinderIpc";
    public static final String TRACKING_PROTECTION_3PCD = "TrackingProtection3pcd";
    public static final String TRACKING_PROTECTION_USER_BYPASS_PWA =
            "TrackingProtectionUserBypassPwa";
    public static final String TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER =
            "TrackingProtectionUserBypassPwaTrigger";
    public static final String TRACKING_PROTECTION_CONTENT_SETTING_UB_CONTROL =
            "TrackingProtectionContentSettingUbControl";
    public static final String TRANSLATE_MESSAGE_UI = "TranslateMessageUI";
    public static final String TRANSLATE_TFLITE = "TFLiteLanguageDetectionEnabled";
    public static final String
            UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING =
                    "UnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning";
    public static final String UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING =
            "UnifiedPasswordManagerLocalPasswordsMigrationWarning";
    public static final String UNO_PHASE_2_FOLLOW_UP = "UnoPhase2FollowUp";
    public static final String UPDATE_COMPOSTIROR_FOR_SURFACE_CONTROL =
            "UpdateCompositorForSurfaceControl";
    public static final String USE_ALTERNATE_HISTORY_SYNC_ILLUSTRATION =
            "UseAlternateHistorySyncIllustration";
    public static final String USE_CHIME_ANDROID_SDK = "UseChimeAndroidSdk";
    public static final String USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID =
            "UseLibunwindstackNativeUnwinderAndroid";
    public static final String VISITED_URL_RANKING_SERVICE = "VisitedURLRankingService";
    public static final String VOICE_SEARCH_AUDIO_CAPTURE_POLICY = "VoiceSearchAudioCapturePolicy";
    public static final String WEB_APK_BACKUP_AND_RESTORE_BACKEND = "WebApkBackupAndRestoreBackend";
    public static final String WEB_APK_INSTALL_FAILURE_NOTIFICATION =
            "WebApkInstallFailureNotification";
    public static final String WEB_APK_MIN_SHELL_APK_VERSION = "WebApkMinShellVersion";
    public static final String WEB_FEED_AWARENESS = "WebFeedAwareness";
    public static final String WEB_FEED_ONBOARDING = "WebFeedOnboarding";
    public static final String WEB_FEED_SORT = "WebFeedSort";
    public static final String WEB_OTP_CROSS_DEVICE_SIMPLE_STRING = "WebOtpCrossDeviceSimpleString";
    public static final String XSURFACE_METRICS_REPORTING = "XsurfaceMetricsReporting";
    public static final String POST_GET_MEMORY_PRESSURE_TO_BACKGROUND =
            BaseFeatures.POST_GET_MY_MEMORY_STATE_TO_BACKGROUND;

    /* Alphabetical: */
    public static final CachedFlag sAndroidAppIntegration =
            newCachedFlag(ANDROID_APP_INTEGRATION, true);
    public static final CachedFlag sAndroidAppIntegrationModule =
            newCachedFlag(ANDROID_APP_INTEGRATION_MODULE, false, true);
    public static final CachedFlag sAndroidAppIntegrationV2 =
            newCachedFlag(ANDROID_APP_INTEGRATION_V2, false, true);
    public static final CachedFlag sAndroidTabSkipSaveTabsKillswitch =
            newCachedFlag(ANDROID_TAB_SKIP_SAVE_TABS_TASK_KILLSWITCH, true, true);
    public static final CachedFlag sNewTabPageCustomization =
            newCachedFlag(NEW_TAB_PAGE_CUSTOMIZATION, false);
    public static final CachedFlag sAndroidAppIntegrationWithFavicon =
            newCachedFlag(ANDROID_APP_INTEGRATION_WITH_FAVICON, false, true);
    public static final CachedFlag sAndroidBottomToolbar =
            newCachedFlag(ANDROID_BOTTOM_TOOLBAR, false, true);
    public static final CachedFlag sAndroidElegantTextHeight =
            newCachedFlag(ANDROID_ELEGANT_TEXT_HEIGHT, true);
    public static final CachedFlag sAndroidTabDeclutterDedupeTabIdsKillSwitch =
            newCachedFlag(ANDROID_TAB_DECLUTTER_DEDUPE_TAB_IDS_KILL_SWITCH, true);
    public static final CachedFlag sAppSpecificHistory = newCachedFlag(APP_SPECIFIC_HISTORY, true);
    public static final CachedFlag sAsyncNotificationManager =
            newCachedFlag(ASYNC_NOTIFICATION_MANAGER, false, true);
    public static final CachedFlag sBlockIntentsWhileLocked =
            newCachedFlag(BLOCK_INTENTS_WHILE_LOCKED, false);
    public static final CachedFlag sBookmarkPaneAndroid =
            newCachedFlag(BOOKMARK_PANE_ANDROID, false);
    public static final CachedFlag sCctAdaptiveButton = newCachedFlag(CCT_ADAPTIVE_BUTTON, false);
    public static final CachedFlag sCctAuthTab = newCachedFlag(CCT_AUTH_TAB, true);
    public static final CachedFlag sCctAuthTabDisableAllExternalIntents =
            newCachedFlag(CCT_AUTH_TAB_DISABLE_ALL_EXTERNAL_INTENTS, false);
    public static final CachedFlag sCctAuthTabEnableHttpsRedirects =
            newCachedFlag(CCT_AUTH_TAB_ENABLE_HTTPS_REDIRECTS, true);
    public static final CachedFlag sCctAutoTranslate = newCachedFlag(CCT_AUTO_TRANSLATE, true);
    public static final CachedFlag sCctBlockTouchesDuringEnterAnimation =
            newCachedFlag(CCT_BLOCK_TOUCHES_DURING_ENTER_ANIMATION, true);
    public static final CachedFlag sCCTEphemeralMediaViewerExperiment =
            newCachedFlag(
                    CCT_EPHEMERAL_MEDIA_VIEWER_EXPERIMENT,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCctEphemeralMode = newCachedFlag(CCT_EPHEMERAL_MODE, true);
    public static final CachedFlag sCctFreInSameTask = newCachedFlag(CCT_FRE_IN_SAME_TASK, true);
    public static final CachedFlag sCctIncognitoAvailableToThirdParty =
            newCachedFlag(CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY, false);
    public static final CachedFlag sCctIntentFeatureOverrides =
            newCachedFlag(CCT_INTENT_FEATURE_OVERRIDES, true);
    public static final CachedFlag sCctMinimized = newCachedFlag(CCT_MINIMIZED, true);
    public static final CachedFlag sCctNavigationalPrefetch =
            newCachedFlag(
                    CCT_NAVIGATIONAL_PREFETCH,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCctGoogleBottomBar =
            newCachedFlag(
                    CCT_GOOGLE_BOTTOM_BAR,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCctGoogleBottomBarVariantLayouts =
            newCachedFlag(CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS, false);
    public static final CachedFlag sCctResizableForThirdParties =
            newCachedFlag(CCT_RESIZABLE_FOR_THIRD_PARTIES, true);
    public static final CachedFlag sCctOpenInBrowserButtonIfAllowedByEmbedder =
            newCachedFlag(CCT_OPEN_IN_BROWSER_BUTTON_IF_ALLOWED_BY_EMBEDDER, false);
    public static final CachedFlag sCctOpenInBrowserButtonIfEnabledByEmbedder =
            newCachedFlag(CCT_OPEN_IN_BROWSER_BUTTON_IF_ENABLED_BY_EMBEDDER, true);
    public static final CachedFlag sCctRevampedBranding =
            newCachedFlag(CCT_REVAMPED_BRANDING, false);
    public static final CachedFlag sCctNestedSecurityIcon =
            newCachedFlag(CCT_NESTED_SECURITY_ICON, false);
    public static final CachedFlag sCctTabModalDialog = newCachedFlag(CCT_TAB_MODAL_DIALOG, true);
    public static final CachedFlag sClankStartupLatencyInjection =
            newCachedFlag(CLANK_STARTUP_LATENCY_INJECTION, false);
    public static final CachedFlag sCollectAndroidFrameTimelineMetrics =
            newCachedFlag(
                    COLLECT_ANDROID_FRAME_TIMELINE_METRICS,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sCommandLineOnNonRooted =
            newCachedFlag(COMMAND_LINE_ON_NON_ROOTED, false);
    public static final CachedFlag sCrossDeviceTabPaneAndroid =
            newCachedFlag(CROSS_DEVICE_TAB_PANE_ANDROID, false);
    public static final CachedFlag sDisableInstanceLimit =
            newCachedFlag(
                    DISABLE_INSTANCE_LIMIT,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sDisableListTabSwitcher =
            newCachedFlag(
                    DISABLE_LIST_TAB_SWITCHER,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sDrawKeyNativeEdgeToEdge =
            newCachedFlag(DRAW_KEY_NATIVE_EDGE_TO_EDGE, true);
    public static final CachedFlag sEdgeToEdgeBottomChin =
            newCachedFlag(
                    EDGE_TO_EDGE_BOTTOM_CHIN,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sEdgeToEdgeEverywhere =
            newCachedFlag(EDGE_TO_EDGE_EVERYWHERE, false);
    public static final CachedFlag sEdgeToEdgeWebOptIn =
            newCachedFlag(EDGE_TO_EDGE_WEB_OPT_IN, true);
    public static final CachedFlag sEducationalTipDefaultBrowserPromoCard =
            newCachedFlag(EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_CARD, false);
    public static final CachedFlag sEducationalTipModule =
            newCachedFlag(EDUCATIONAL_TIP_MODULE, false, true);
    public static final CachedFlag sEnableDiscountInfoApi =
            newCachedFlag(ENABLE_DISCOUNT_INFO_API, false);
    public static final CachedFlag sEnableXAxisActivityTransition =
            newCachedFlag(ENABLE_X_AXIS_ACTIVITY_TRANSITION, false);
    public static final CachedFlag sEsbAiStringUpdate =
            newCachedFlag(
                    ESB_AI_STRING_UPDATE,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final CachedFlag sFloatingSnackbar = newCachedFlag(FLOATING_SNACKBAR, false);
    public static final CachedFlag sForceListTabSwitcher =
            newCachedFlag(FORCE_LIST_TAB_SWITCHER, false);
    public static final CachedFlag sForceTranslucentNotificationTrampoline =
            newCachedFlag(FORCE_TRANSLUCENT_NOTIFICATION_TRAMPOLINE, false);
    public static final CachedFlag sFullscreenInsetsApiMigration =
            newCachedFlag(FULLSCREEN_INSETS_API_MIGRATION, false);
    public static final CachedFlag sFullscreenInsetsApiMigrationOnAutomotive =
            newCachedFlag(FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE, true);
    public static final CachedFlag sHideTabletToolbarDownloadButton =
            newCachedFlag(
                    HIDE_TABLET_TOOLBAR_DOWNLOAD_BUTTON,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sHistoryPaneAndroid = newCachedFlag(HISTORY_PANE_ANDROID, false);
    public static final CachedFlag sLegacyTabStateDeprecation =
            newCachedFlag(
                    LEGACY_TAB_STATE_DEPRECATION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);
    public static final CachedFlag sLockBackPressHandlerAtStart =
            newCachedFlag(LOCK_BACK_PRESS_HANDLER_AT_START, true);
    public static final CachedFlag sLogoPolish = newCachedFlag(LOGO_POLISH, true);
    public static final CachedFlag sLogoPolishAnimationKillSwitch =
            newCachedFlag(LOGO_POLISH_ANIMATION_KILL_SWITCH, true);
    public static final CachedFlag sMagicStackAndroid = newCachedFlag(MAGIC_STACK_ANDROID, true);
    public static final CachedFlag sMostVisitedTilesCustomization =
            newCachedFlag(MOST_VISITED_TILES_CUSTOMIZATION, false);
    public static final CachedFlag sMostVisitedTilesReselect =
            newCachedFlag(MOST_VISITED_TILES_RESELECT, false);
    public static final CachedFlag sMultiInstanceApplicationStatusCleanup =
            newCachedFlag(MUlTI_INSTANCE_APPLICATION_STATUS_CLEANUP, false);
    public static final CachedFlag sNavBarColorAnimation =
            newCachedFlag(NAV_BAR_COLOR_ANIMATION, false);
    public static final CachedFlag sNavBarColorMatchesTabBackground =
            newCachedFlag(NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND, true);
    public static final CachedFlag sNewTabPageAndroidTriggerForPrerender2 =
            newCachedFlag(NEW_TAB_PAGE_ANDROID_TRIGGER_FOR_PRERENDER2, true);
    public static final CachedFlag sNotificationTrampoline =
            newCachedFlag(NOTIFICATION_TRAMPOLINE, false);
    public static final CachedFlag sPowerSavingModeBroadcastReceiverInBackground =
            newCachedFlag(
                    POWER_SAVING_MODE_BROADCAST_RECEIVER_IN_BACKGROUND,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sPriceChangeModule = newCachedFlag(PRICE_CHANGE_MODULE, true);
    public static final CachedFlag sPriceInsights =
            newCachedFlag(
                    PRICE_INSIGHTS, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sOptimizationGuidePushNotifications =
            newCachedFlag(OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, true);
    public static final CachedFlag sPaintPreviewDemo = newCachedFlag(PAINT_PREVIEW_DEMO, false);
    public static final CachedFlag sPostGetMyMemoryStateToBackground =
            newCachedFlag(POST_GET_MEMORY_PRESSURE_TO_BACKGROUND, true);
    public static final CachedFlag sPrefetchBrowserInitiatedTriggers =
            newCachedFlag(PREFETCH_BROWSER_INITIATED_TRIGGERS, true);
    public static final CachedFlag sRedirectExplicitCTAIntentsToExistingActivity =
            newCachedFlag(REDIRECT_EXPLICIT_CTA_INTENTS_TO_EXISTING_ACTIVITY, true);

    public static final CachedFlag sRightEdgeGoesForwardGestureNav =
            newCachedFlag(RIGHT_EDGE_GOES_FORWARD_GESTURE_NAV, false);

    public static final CachedFlag sSafetyHubMagicStack =
            newCachedFlag(SAFETY_HUB_MAGIC_STACK, false);
    public static final CachedFlag sSafetyHubWeakAndReusedPasswords =
            newCachedFlag(SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS, false);
    public static final CachedFlag sSearchInCCT =
            newCachedFlag(
                    SEARCH_IN_CCT, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sSearchInCCTAlternateTapHandling =
            newCachedFlag(SEARCH_IN_CCT_ALTERNATE_TAP_HANDLING, false);
    public static final CachedFlag sSettingsSingleActivity =
            newCachedFlag(SETTINGS_SINGLE_ACTIVITY, false);
    public static final CachedFlag sSkipIsolatedSplitPreload =
            newCachedFlag(
                    SKIP_ISOLATED_SPLIT_PRELOAD,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sSmallerTabStripTitleLimit =
            newCachedFlag(SMALLER_TAB_STRIP_TITLE_LIMIT, true);
    public static final CachedFlag sStartSurfaceReturnTime =
            newCachedFlag(START_SURFACE_RETURN_TIME, true);
    public static final CachedFlag sTabClosureMethodRefactor =
            newCachedFlag(TAB_CLOSURE_METHOD_REFACTOR, false);
    public static final CachedFlag sTabDragDropAsWindowAndroid =
            newCachedFlag(TAB_DRAG_DROP_ANDROID, false);
    public static final CachedFlag sTabGroupPaneAndroid =
            newCachedFlag(TAB_GROUP_PANE_ANDROID, /* defaultValue= */ true);
    public static final CachedFlag sTabResumptionModuleAndroid =
            newCachedFlag(
                    TAB_RESUMPTION_MODULE_ANDROID,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTabStateFlatBuffer =
            newCachedFlag(
                    TAB_STATE_FLAT_BUFFER,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTabStripIncognitoMigration =
            newCachedFlag(
                    TAB_STRIP_INCOGNITO_MIGRATION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTabStripLayoutOptimization =
            newCachedFlag(
                    TAB_STRIP_LAYOUT_OPTIMIZATION,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);
    public static final CachedFlag sTabStripGroupCollapse =
            newCachedFlag(TAB_STRIP_GROUP_COLLAPSE, /* defaultValue= */ true);
    public static final CachedFlag sTabWindowManagerIndexReassignmentActivityFinishing =
            newCachedFlag(TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_FINISHING, true);
    public static final CachedFlag sTabWindowManagerIndexReassignmentActivityInSameTask =
            newCachedFlag(TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_IN_SAME_TASK, true);
    public static final CachedFlag sTabWindowManagerIndexReassignmentActivityNotInAppTasks =
            newCachedFlag(TAB_WINDOW_MANAGER_INDEX_REASSIGNMENT_ACTIVITY_NOT_IN_APP_TASKS, true);
    public static final CachedFlag sTabWindowManagerReportIndicesMismatch =
            newCachedFlag(TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH, true);
    public static final CachedFlag sTestDefaultDisabled =
            newCachedFlag(TEST_DEFAULT_DISABLED, false);
    public static final CachedFlag sTestDefaultEnabled = newCachedFlag(TEST_DEFAULT_ENABLED, true);
    public static final CachedFlag sTraceBinderIpc =
            newCachedFlag(
                    TRACE_BINDER_IPC, /* defaultValue= */ false, /* defaultValueInTests= */ true);
    public static final CachedFlag sUseChimeAndroidSdk =
            newCachedFlag(USE_CHIME_ANDROID_SDK, false);
    public static final CachedFlag sUseLibunwindstackNativeUnwinderAndroid =
            newCachedFlag(USE_LIBUNWINDSTACK_NATIVE_UNWINDER_ANDROID, true);
    public static final CachedFlag sWebApkMinShellApkVersion =
            newCachedFlag(WEB_APK_MIN_SHELL_APK_VERSION, true);

    public static final List<CachedFlag> sFlagsCachedFullBrowser =
            List.of(
                    sAndroidAppIntegration,
                    sAndroidAppIntegrationModule,
                    sAndroidAppIntegrationV2,
                    sAndroidAppIntegrationWithFavicon,
                    sAndroidTabSkipSaveTabsKillswitch,
                    sAndroidBottomToolbar,
                    sAndroidElegantTextHeight,
                    sAndroidTabDeclutterDedupeTabIdsKillSwitch,
                    sAppSpecificHistory,
                    sAsyncNotificationManager,
                    sBlockIntentsWhileLocked,
                    sBookmarkPaneAndroid,
                    sCctAdaptiveButton,
                    sCctAuthTab,
                    sCctAuthTabDisableAllExternalIntents,
                    sCctAuthTabEnableHttpsRedirects,
                    sCctAutoTranslate,
                    sCctBlockTouchesDuringEnterAnimation,
                    sCCTEphemeralMediaViewerExperiment,
                    sCctEphemeralMode,
                    sCctFreInSameTask,
                    sCctIncognitoAvailableToThirdParty,
                    sCctIntentFeatureOverrides,
                    sCctMinimized,
                    sCctNavigationalPrefetch,
                    sCctGoogleBottomBar,
                    sCctGoogleBottomBarVariantLayouts,
                    sCctOpenInBrowserButtonIfAllowedByEmbedder,
                    sCctOpenInBrowserButtonIfEnabledByEmbedder,
                    sCctResizableForThirdParties,
                    sCctRevampedBranding,
                    sCctNestedSecurityIcon,
                    sCctTabModalDialog,
                    sClankStartupLatencyInjection,
                    sCollectAndroidFrameTimelineMetrics,
                    sCommandLineOnNonRooted,
                    sCrossDeviceTabPaneAndroid,
                    sDisableInstanceLimit,
                    sDisableListTabSwitcher,
                    sDrawKeyNativeEdgeToEdge,
                    sEdgeToEdgeBottomChin,
                    sEdgeToEdgeEverywhere,
                    sEdgeToEdgeWebOptIn,
                    sEducationalTipDefaultBrowserPromoCard,
                    sEducationalTipModule,
                    sEnableDiscountInfoApi,
                    sEnableXAxisActivityTransition,
                    sEsbAiStringUpdate,
                    sFloatingSnackbar,
                    sForceListTabSwitcher,
                    sForceTranslucentNotificationTrampoline,
                    sFullscreenInsetsApiMigration,
                    sFullscreenInsetsApiMigrationOnAutomotive,
                    sHideTabletToolbarDownloadButton,
                    sHistoryPaneAndroid,
                    sLockBackPressHandlerAtStart,
                    sLogoPolish,
                    sLogoPolishAnimationKillSwitch,
                    sNotificationTrampoline,
                    sMagicStackAndroid,
                    sMostVisitedTilesCustomization,
                    sMostVisitedTilesReselect,
                    sMultiInstanceApplicationStatusCleanup,
                    sNavBarColorAnimation,
                    sNavBarColorMatchesTabBackground,
                    sNewTabPageAndroidTriggerForPrerender2,
                    sNewTabPageCustomization,
                    sPowerSavingModeBroadcastReceiverInBackground,
                    sPriceChangeModule,
                    sPriceInsights,
                    sOptimizationGuidePushNotifications,
                    sPaintPreviewDemo,
                    sPostGetMyMemoryStateToBackground,
                    sPrefetchBrowserInitiatedTriggers,
                    sRedirectExplicitCTAIntentsToExistingActivity,
                    sRightEdgeGoesForwardGestureNav,
                    sSafetyHubMagicStack,
                    sSafetyHubWeakAndReusedPasswords,
                    sSearchInCCT,
                    sSearchInCCTAlternateTapHandling,
                    sLegacyTabStateDeprecation,
                    sSettingsSingleActivity,
                    sSkipIsolatedSplitPreload,
                    sSmallerTabStripTitleLimit,
                    sStartSurfaceReturnTime,
                    sTabClosureMethodRefactor,
                    sTabDragDropAsWindowAndroid,
                    sTabGroupPaneAndroid,
                    sTabResumptionModuleAndroid,
                    sTabStateFlatBuffer,
                    sTabStripGroupCollapse,
                    sTabStripIncognitoMigration,
                    sTabStripLayoutOptimization,
                    sTabWindowManagerIndexReassignmentActivityFinishing,
                    sTabWindowManagerIndexReassignmentActivityInSameTask,
                    sTabWindowManagerIndexReassignmentActivityNotInAppTasks,
                    sTabWindowManagerReportIndicesMismatch,
                    sTraceBinderIpc,
                    sUseChimeAndroidSdk,
                    sUseLibunwindstackNativeUnwinderAndroid,
                    sWebApkMinShellApkVersion);

    public static final List<CachedFlag> sFlagsCachedInMinimalBrowser = List.of();

    public static final List<CachedFlag> sTestCachedFlags =
            List.of(sTestDefaultDisabled, sTestDefaultEnabled);

    public static final Map<String, CachedFlag> sAllCachedFlags =
            CachedFlag.createCachedFlagMap(
                    List.of(
                            sFlagsCachedFullBrowser,
                            sFlagsCachedInMinimalBrowser,
                            sTestCachedFlags));

    // MutableFlagWithSafeDefault instances.
    /* Alphabetical: */
    public static final MutableFlagWithSafeDefault sAdaptiveButtonInTopToolbarCustomizationV2 =
            newMutableFlagWithSafeDefault(ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, false);
    public static final MutableFlagWithSafeDefault sAndroidBookmarkBar =
            newMutableFlagWithSafeDefault(ANDROID_BOOKMARK_BAR, false);
    public static final MutableFlagWithSafeDefault sAndroidDumpOnScrollWithoutResource =
            newMutableFlagWithSafeDefault(ANDROID_DUMP_ON_SCROLL_WITHOUT_RESOURCE, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutter =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER, true);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterArchiveAllButActiveTab =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_ARCHIVE_ALL_BUT_ACTIVE, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterArchiveDuplicateTabs =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_ARCHIVE_DUPLICATE_TABS, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterArchiveTabGroups =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS, false);
    public static final MutableFlagWithSafeDefault sAndroidTabDeclutterRescueKillSwitch =
            newMutableFlagWithSafeDefault(ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH, true);
    public static final MutableFlagWithSafeDefault sBottomBrowserControlsRefactor =
            newMutableFlagWithSafeDefault(BOTTOM_BROWSER_CONTROLS_REFACTOR, true);
    public static final MutableFlagWithSafeDefault sBcivBottomControls =
            newMutableFlagWithSafeDefault(BCIV_BOTTOM_CONTROLS, false);
    public static final MutableFlagWithSafeDefault sBcivWithSuppression =
            newMutableFlagWithSafeDefault(BCIV_WITH_SUPPRESSION, false);
    public static final MutableFlagWithSafeDefault sBcivZeroBrowserFrames =
            newMutableFlagWithSafeDefault(BCIV_ZERO_BROWSER_FRAMES, false);
    public static final MutableFlagWithSafeDefault sBrowserControlsInViz =
            newMutableFlagWithSafeDefault(BROWSER_CONTROLS_IN_VIZ, true);
    public static final MutableFlagWithSafeDefault sBrowserControlsEarlyResize =
            newMutableFlagWithSafeDefault(BROWSER_CONTROLS_EARLY_RESIZE, false);
    public static final MutableFlagWithSafeDefault sClearBrowsingDataAndroidSurvey =
            newMutableFlagWithSafeDefault(CLEAR_BROWSING_DATA_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sControlsVisibilityFromNavigations =
            newMutableFlagWithSafeDefault(CONTROLS_VISIBILITY_FROM_NAVIGATIONS, true);
    public static final MutableFlagWithSafeDefault sDynamicSafeAreaInsets =
            newMutableFlagWithSafeDefault(DYNAMIC_SAFE_AREA_INSETS, false);
    public static final MutableFlagWithSafeDefault sEdgeToEdgeSafeAreaConstraint =
            newMutableFlagWithSafeDefault(EDGE_TO_EDGE_SAFE_AREA_CONSTRAINT, false);
    // Defaulted to true in native, but since it is being used as a kill switch set the default
    // value pre-native to false as it is safer if the feature needs to be killed via Finch config.
    public static final MutableFlagWithSafeDefault sEmptyTabListAnimationKillSwitch =
            newMutableFlagWithSafeDefault(EMPTY_TAB_LIST_ANIMATION_KILL_SWITCH, false);
    public static final MutableFlagWithSafeDefault sForceBrowserControlsUponExitingFullscreen =
            newMutableFlagWithSafeDefault(FORCE_BROWSER_CONTROLS_UPON_EXITING_FULLSCREEN, true);
    public static final MutableFlagWithSafeDefault sIncognitoScreenshot =
            newMutableFlagWithSafeDefault(INCOGNITO_SCREENSHOT, false);
    public static final MutableFlagWithSafeDefault sNoVisibleHintForDifferentTLD =
            newMutableFlagWithSafeDefault(ANDROID_NO_VISIBLE_HINT_FOR_DIFFERENT_TLD, true);
    public static final MutableFlagWithSafeDefault sQuickDeleteAndroidSurvey =
            newMutableFlagWithSafeDefault(QUICK_DELETE_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sReadAloudTapToSeek =
            newMutableFlagWithSafeDefault(READALOUD_TAP_TO_SEEK, false);
    public static final MutableFlagWithSafeDefault sRecordSuppressionMetrics =
            newMutableFlagWithSafeDefault(RECORD_SUPPRESSION_METRICS, true);
    public static final MutableFlagWithSafeDefault sSafetyHub =
            newMutableFlagWithSafeDefault(SAFETY_HUB, true);
    public static final MutableFlagWithSafeDefault sSafetyHubAndroidOrganicSurvey =
            newMutableFlagWithSafeDefault(SAFETY_HUB_ANDROID_ORGANIC_SURVEY, false);
    public static final MutableFlagWithSafeDefault sSafetyHubAndroidSurvey =
            newMutableFlagWithSafeDefault(SAFETY_HUB_ANDROID_SURVEY, false);
    public static final MutableFlagWithSafeDefault sSafetyHubAndroidSurveyV2 =
            newMutableFlagWithSafeDefault(SAFETY_HUB_ANDROID_SURVEY_V2, false);
    public static final MutableFlagWithSafeDefault sSafetyHubFollowup =
            newMutableFlagWithSafeDefault(SAFETY_HUB_FOLLOWUP, true);
    public static final MutableFlagWithSafeDefault sShowNewTabAnimations =
            newMutableFlagWithSafeDefault(SHOW_NEW_TAB_ANIMATIONS, false);
    public static final MutableFlagWithSafeDefault sSuppressionToolbarCaptures =
            newMutableFlagWithSafeDefault(SUPPRESS_TOOLBAR_CAPTURES, false);
    public static final MutableFlagWithSafeDefault sSuppressToolbarCapturesAtGestureEnd =
            newMutableFlagWithSafeDefault(SUPPRESS_TOOLBAR_CAPTURES_AT_GESTURE_END, false);
    public static final MutableFlagWithSafeDefault sTabSwitcherColorBlendAnimate =
            newMutableFlagWithSafeDefault(TAB_SWITCHER_COLOR_BLEND_ANIMATE, true);
    public static final MutableFlagWithSafeDefault sTabSwitcherForeignFaviconSupport =
            newMutableFlagWithSafeDefault(TAB_SWITCHER_FOREIGN_FAVICON_SUPPORT, false);
    public static final MutableFlagWithSafeDefault sTabSwitcherFullNewTabButton =
            newMutableFlagWithSafeDefault(TAB_SWITCHER_FULL_NEW_TAB_BUTTON, false);
    public static final MutableFlagWithSafeDefault sToolbarScrollAblation =
            newMutableFlagWithSafeDefault(TOOLBAR_SCROLL_ABLATION, false);
    public static final MutableFlagWithSafeDefault sVoiceSearchAudioCapturePolicy =
            newMutableFlagWithSafeDefault(VOICE_SEARCH_AUDIO_CAPTURE_POLICY, false);

    // CachedFeatureParam instances.
    /* Alphabetical order by feature name, arbitrary order by param name: */
    public static final BooleanCachedFeatureParam sCctAdaptiveButtonEnableOpenInBrowser =
            newBooleanCachedFeatureParam(CCT_ADAPTIVE_BUTTON, "open_in_browser", false);
    public static final BooleanCachedFeatureParam sCctAdaptiveButtonEnableVoice =
            newBooleanCachedFeatureParam(CCT_ADAPTIVE_BUTTON, "voice", false);
    public static final IntCachedFeatureParam sAndroidAppIntegrationV2ContentTtlHours =
            newIntCachedFeatureParam(ANDROID_APP_INTEGRATION_V2, "content_ttl_hours", 168);
    public static final BooleanCachedFeatureParam sAndroidAppIntegrationWithFaviconSkipDeviceCheck =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "skip_device_check", false);
    public static final BooleanCachedFeatureParam sAndroidAppIntegrationModuleForceCardShow =
            newBooleanCachedFeatureParam(ANDROID_APP_INTEGRATION_MODULE, "force_card_shown", false);

    public static final BooleanCachedFeatureParam sAndroidAppIntegrationModuleShowThirdPartyCard =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_MODULE, "show_third_party_card", false);

    public static final IntCachedFeatureParam sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs =
            newIntCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "schedule_delay_time_ms", 0);
    public static final BooleanCachedFeatureParam sAndroidAppIntegrationWithFaviconUseLargeFavicon =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "use_large_favicon", false);
    public static final IntCachedFeatureParam
            sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber =
                    newIntCachedFeatureParam(
                            ANDROID_APP_INTEGRATION_WITH_FAVICON, "zero_state_favicon_number", 5);

    public static final BooleanCachedFeatureParam sAndroidAppIntegrationWithFaviconSkipSchemaCheck =
            newBooleanCachedFeatureParam(
                    ANDROID_APP_INTEGRATION_WITH_FAVICON, "skip_schema_check", false);
    public static final IntCachedFeatureParam sCctAuthTabEnableHttpsRedirectsVerificationTimeoutMs =
            newIntCachedFeatureParam(
                    CCT_AUTH_TAB_ENABLE_HTTPS_REDIRECTS, "verification_timeout_ms", 10_000);

    /**
     * Parameter that lists a pipe ("|") separated list of package names from which the {@link
     * EXTRA_AUTO_TRANSLATE_LANGUAGE} should be allowed. This defaults to a single list item
     * consisting of the package name of the Android Google Search App.
     */
    public static final StringCachedFeatureParam sCctAutoTranslatePackageNamesAllowlist =
            newStringCachedFeatureParam(
                    CCT_AUTO_TRANSLATE,
                    "package_names_allowlist",
                    "com.google.android.googlequicksearchbox");

    /**
     * Parameter that, if true, indicates that the {@link EXTRA_AUTO_TRANSLATE_LANGUAGE} should be
     * automatically allowed from any first party package name.
     */
    public static final BooleanCachedFeatureParam sCctAutoTranslateAllowAllFirstParties =
            newBooleanCachedFeatureParam(CCT_AUTO_TRANSLATE, "allow_all_first_parties", false);

    public static final IntCachedFeatureParam sCctMinimizedIconVariant =
            newIntCachedFeatureParam(CCT_MINIMIZED, "icon_variant", 0);
    // Devices from this OEM--and potentially others--sometimes crash when we call
    // `Activity#enterPictureInPictureMode` on Android R. So, we disable the feature on those
    // devices. See: https://crbug.com/1519164.
    public static final StringCachedFeatureParam
            sCctMinimizedEnabledByDefaultManufacturerExcludeList =
                    newStringCachedFeatureParam(
                            CCT_MINIMIZED_ENABLED_BY_DEFAULT,
                            "manufacturer_exclude_list",
                            "xiaomi");

    /**
     * A cached parameter used for specifying the height of the Google Bottom Bar in DP, when its
     * variant is NO_VARIANT.
     */
    public static final IntCachedFeatureParam sCctGoogleBottomBarNoVariantHeightDp =
            newIntCachedFeatureParam(
                    CCT_GOOGLE_BOTTOM_BAR, "google_bottom_bar_no_variant_height_dp", 64);

    /**
     * A cached parameter representing the order of Google Bottom Bar buttons based on experiment
     * configuration.
     */
    public static final StringCachedFeatureParam sCctGoogleBottomBarButtonList =
            newStringCachedFeatureParam(CCT_GOOGLE_BOTTOM_BAR, "google_bottom_bar_button_list", "");

    /**
     * A cached parameter used for specifying the height of the Google Bottom Bar in DP, when its
     * variant is SINGLE_DECKER.
     */
    public static final IntCachedFeatureParam
            sCctGoogleBottomBarVariantLayoutsSingleDeckerHeightDp =
                    newIntCachedFeatureParam(
                            CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                            "google_bottom_bar_single_decker_height_dp",
                            62);

    /**
     * A cached parameter representing the Google Bottom Bar layout variants value based on
     * experiment configuration.
     */
    public static final IntCachedFeatureParam sCctGoogleBottomBarVariantLayoutsVariantLayout =
            newIntCachedFeatureParam(
                    CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                    "google_bottom_bar_variant_layout",
                    1 /* GoogleBottomBarVariantLayoutType.DOUBLE_DECKER */);

    /**
     * A cached boolean parameter to decide whether to check if Google is Chrome's default search
     * engine.
     */
    public static final BooleanCachedFeatureParam
            sCctGoogleBottomBarVariantLayoutsGoogleDseCheckEnabled =
                    newBooleanCachedFeatureParam(
                            CCT_GOOGLE_BOTTOM_BAR_VARIANT_LAYOUTS,
                            "google_bottom_bar_variant_is_google_default_search_engine_check_enabled",
                            false);

    public static final StringCachedFeatureParam sCctResizableForThirdPartiesAllowlistEntries =
            newStringCachedFeatureParam(CCT_RESIZABLE_FOR_THIRD_PARTIES, "allowlist_entries", "");
    public static final StringCachedFeatureParam sCctResizableForThirdPartiesDenylistEntries =
            newStringCachedFeatureParam(CCT_RESIZABLE_FOR_THIRD_PARTIES, "denylist_entries", "");
    public static final StringCachedFeatureParam sCctResizableForThirdPartiesDefaultPolicy =
            newStringCachedFeatureParam(
                    CCT_RESIZABLE_FOR_THIRD_PARTIES, "default_policy", "use-denylist");

    /**
     * A cached parameter representing the amount of latency to inject during Clank startup based on
     * experiment configuration.
     */
    public static final IntCachedFeatureParam sClankStartupLatencyInjectionAmountMs =
            newIntCachedFeatureParam(
                    CLANK_STARTUP_LATENCY_INJECTION, "latency_injection_amount_millis", 0);

    /**
     * The parameter to control how quickly the JankTracker should be enabled. Some JankTracker
     * implementations can be quite heavy (including thread creation and the link). To avoid
     * impacting start up we support delaying the construction of the JankTracker.
     *
     * <p>3000ms is enough to push us past the median start up time without losing to much of feed
     * interactions.
     */
    public static final IntCachedFeatureParam
            sCollectAndroidFrameTimelineMetricsJankTrackerDelayedStartMs =
                    newIntCachedFeatureParam(
                            COLLECT_ANDROID_FRAME_TIMELINE_METRICS, "delayed_start_ms", 3000);

    /** Cached param whether we disable e2e on the recent tabs page. */
    public static final BooleanCachedFeatureParam sDrawKeyNativeEdgeToEdgeDisableRecentTabsE2e =
            newBooleanCachedFeatureParam(
                    DRAW_KEY_NATIVE_EDGE_TO_EDGE, "disable_recent_tabs_e2e", false);

    /** Cached param whether we disable e2e on the CCT media viewer. */
    public static final BooleanCachedFeatureParam sDrawKeyNativeEdgeToEdgeDisableCctMediaViewerE2e =
            newBooleanCachedFeatureParam(
                    DRAW_KEY_NATIVE_EDGE_TO_EDGE, "disable_cct_media_viewer_e2e", false);

    /** Cached param whether we disable e2e on the hub. */
    public static final BooleanCachedFeatureParam sDrawKeyNativeEdgeToEdgeDisableHubE2e =
            newBooleanCachedFeatureParam(DRAW_KEY_NATIVE_EDGE_TO_EDGE, "disable_hub_e2e", false);

    /** Cached param whether we disable e2e on new tab page. */
    public static final BooleanCachedFeatureParam sDrawKeyNativeEdgeToEdgeDisableNtpE2e =
            newBooleanCachedFeatureParam(DRAW_KEY_NATIVE_EDGE_TO_EDGE, "disable_ntp_e2e", false);

    /** Cached param whether we disable e2e on incognito new tab page. See crbug.com/368675202 */
    public static final BooleanCachedFeatureParam sDrawKeyNativeEdgeToEdgeDisableIncognitoNtpE2e =
            newBooleanCachedFeatureParam(
                    DRAW_KEY_NATIVE_EDGE_TO_EDGE, "disable_incognito_ntp_e2e", false);

    /**
     * Cached param whether we disable animations for color changes to the edge-to-edge bottom chin.
     */
    public static final BooleanCachedFeatureParam
            sNavBarColorAnimationDisableBottomChinColorAnimation =
                    newBooleanCachedFeatureParam(
                            NAV_BAR_COLOR_ANIMATION, "disable_bottom_chin_color_animation", false);

    /**
     * Cached param whether we disable animations for color changes to the navigation bar for the
     * edge-to-edge layout used in edge-to-edge-everywhere.
     */
    public static final BooleanCachedFeatureParam
            sNavBarColorAnimationDisableEdgeToEdgeLayoutColorAnimation =
                    newBooleanCachedFeatureParam(
                            NAV_BAR_COLOR_ANIMATION,
                            "disable_edge_to_edge_layout_color_animation",
                            false);

    /**
     * Param for the OEMs that need an exception for min versions. Its value should be a comma
     * separated list of integers, and its index should match {@link #sEdgeToEdgeBottomChinOemList}.
     */
    public static final StringCachedFeatureParam sEdgeToEdgeBottomChinOemMinVersions =
            newStringCachedFeatureParam(
                    EDGE_TO_EDGE_BOTTOM_CHIN, "e2e_field_trial_oem_min_versions", "");

    /**
     * Param for the OEMs that need an exception for min versions. Its value should be a comma
     * separated list of manufacturer, and its index should match {@link
     * #sEdgeToEdgeBottomChinOemMinVersions}.
     */
    public static final StringCachedFeatureParam sEdgeToEdgeBottomChinOemList =
            newStringCachedFeatureParam(EDGE_TO_EDGE_BOTTOM_CHIN, "e2e_field_trial_oem_list", "");

    public static final BooleanCachedFeatureParam sEdgeToEdgeEverywhereIsDebugging =
            newBooleanCachedFeatureParam(EDGE_TO_EDGE_EVERYWHERE, "e2e_everywhere_debug", false);

    public static final BooleanCachedFeatureParam sLogoPolishMediumSize =
            newBooleanCachedFeatureParam(LOGO_POLISH, "polish_logo_size_medium", true);
    public static final BooleanCachedFeatureParam sLogoPolishLargeSize =
            newBooleanCachedFeatureParam(LOGO_POLISH, "polish_logo_size_large", false);
    public static final BooleanCachedFeatureParam sMagicStackAndroidShowAllModules =
            newBooleanCachedFeatureParam(MAGIC_STACK_ANDROID, "show_all_modules", false);
    public static final BooleanCachedFeatureParam mMostVisitedTilesReselectLaxSchemeHost =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_scheme_host", false);
    public static final BooleanCachedFeatureParam mMostVisitedTilesReselectLaxRef =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_ref", false);
    public static final BooleanCachedFeatureParam mMostVisitedTilesReselectLaxQuery =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_query", false);
    public static final BooleanCachedFeatureParam mMostVisitedTilesReselectLaxPath =
            newBooleanCachedFeatureParam(MOST_VISITED_TILES_RESELECT, "lax_path", false);
    public static final BooleanCachedFeatureParam
            sNavBarColorMatchesTabBackgroundColorAnimationDisabled =
                    newBooleanCachedFeatureParam(
                            NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND, "color_animation_disabled", true);
    public static final BooleanCachedFeatureParam sNewTabSearchEngineUrlAndroidSwapOutNtp =
            newBooleanCachedFeatureParam(NEW_TAB_SEARCH_ENGINE_URL_ANDROID, "swap_out_ntp", false);
    public static final IntCachedFeatureParam sNotificationTrampolineLongJobDurationMs =
            newIntCachedFeatureParam(NOTIFICATION_TRAMPOLINE, "long_job_duration_millis", 8 * 1000);
    public static final IntCachedFeatureParam sNotificationTrampolineNormalJobDurationMs =
            newIntCachedFeatureParam(
                    NOTIFICATION_TRAMPOLINE, "normal_job_duration_millis", 1 * 1000);
    public static final IntCachedFeatureParam sNotificationTrampolineImmediateJobDurationMs =
            newIntCachedFeatureParam(NOTIFICATION_TRAMPOLINE, "minimum_job_duration_millis", 10);
    public static final IntCachedFeatureParam sNotificationTrampolineTimeoutPriorNativeInitMs =
            newIntCachedFeatureParam(
                    NOTIFICATION_TRAMPOLINE, "timeout_in_millis_prior_native_init", 5 * 1000);
    public static final IntCachedFeatureParam sOmahaMinSdkVersionMinSdkVersion =
            newIntCachedFeatureParam(OMAHA_MIN_SDK_VERSION_ANDROID, "min_sdk_version", -1);

    /** The default cache size in Java for push notification. */
    public static final IntCachedFeatureParam sOptimizationGuidePushNotificationsMaxCacheSize =
            newIntCachedFeatureParam(OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, "max_cache_size", 100);

    public static final BooleanCachedFeatureParam
            sPriceChangeModuleSkipShoppingPersistedTabDataDelayedInit =
                    newBooleanCachedFeatureParam(
                            PRICE_CHANGE_MODULE,
                            "skip_shopping_persisted_tab_data_delayed_initialization",
                            true);

    /** Controls whether Referrer App ID is passed to Search Results Page via client= param. */
    public static final BooleanCachedFeatureParam sSearchinCctApplyReferrerId =
            newBooleanCachedFeatureParam(SEARCH_IN_CCT, "apply_referrer_id", false);

    /** Pipe ("|") separated list of package names allowed to use the interactive Omnibox. */
    // TODO(crbug.com/40239922): remove default value when no longer relevant.
    public static final StringCachedFeatureParam sSearchinCctOmniboxAllowedPackageNames =
            newStringCachedFeatureParam(
                    SEARCH_IN_CCT,
                    "omnibox_allowed_package_names",
                    BuildConfig.ENABLE_DEBUG_LOGS ? "org.chromium.customtabsclient" : "");

    public static final IntCachedFeatureParam sStartSurfaceReturnTimeTabletSecs =
            newIntCachedFeatureParam(
                    START_SURFACE_RETURN_TIME,
                    "start_surface_return_time_on_tablet_seconds",
                    14400); // 4 hours
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidShowDefaultReason =
            newBooleanCachedFeatureParam(
                    TAB_RESUMPTION_MODULE_ANDROID, "show_default_reason", false);
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidFetchHistoryBackend =
            newBooleanCachedFeatureParam(
                    TAB_RESUMPTION_MODULE_ANDROID, "fetch_history_backend", false);
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidDisableBlend =
            newBooleanCachedFeatureParam(TAB_RESUMPTION_MODULE_ANDROID, "disable_blend", false);
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidUseDefaultAppFilter =
            newBooleanCachedFeatureParam(
                    TAB_RESUMPTION_MODULE_ANDROID, "use_default_app_filter", false);
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidShowSeeMore =
            newBooleanCachedFeatureParam(TAB_RESUMPTION_MODULE_ANDROID, "show_see_more", false);
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidUseSalientImage =
            newBooleanCachedFeatureParam(TAB_RESUMPTION_MODULE_ANDROID, "use_salient_image", false);
    public static final IntCachedFeatureParam sTabResumptionModuleAndroidMaxTilesNumber =
            newIntCachedFeatureParam(TAB_RESUMPTION_MODULE_ANDROID, "max_tiles_number", 2);
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidEnableV2 =
            newBooleanCachedFeatureParam(TAB_RESUMPTION_MODULE_ANDROID, "enable_v2", false);
    public static final BooleanCachedFeatureParam sTabResumptionModuleAndroidCombineTabs =
            newBooleanCachedFeatureParam(
                    TAB_RESUMPTION_MODULE_ANDROID, "show_tabs_in_one_module", false);
    public static final BooleanCachedFeatureParam sTabStateFlatBufferMigrateStaleTabs =
            newBooleanCachedFeatureParam(TAB_STATE_FLAT_BUFFER, "migrate_stale_tabs", true);
    public static final IntCachedFeatureParam
            sTabWindowManagerReportIndicesMismatchTimeDiffThresholdMs =
                    newIntCachedFeatureParam(
                            TAB_WINDOW_MANAGER_REPORT_INDICES_MISMATCH,
                            "activity_creation_timestamp_diff_threshold_ms",
                            1000);

    /** Always register to push notification service. */
    public static final BooleanCachedFeatureParam sUseChimeAndroidSdkAlwaysRegister =
            newBooleanCachedFeatureParam(USE_CHIME_ANDROID_SDK, "always_register", false);

    public static final IntCachedFeatureParam sWebApkMinShellApkVersionValue =
            newIntCachedFeatureParam(WEB_APK_MIN_SHELL_APK_VERSION, "version", 146);

    /** All {@link CachedFeatureParam}s of features in this FeatureList */
    public static final List<CachedFeatureParam<?>> sParamsCached =
            List.of(
                    sAndroidAppIntegrationV2ContentTtlHours,
                    sAndroidAppIntegrationWithFaviconSkipDeviceCheck,
                    sAndroidAppIntegrationModuleForceCardShow,
                    sAndroidAppIntegrationModuleShowThirdPartyCard,
                    sAndroidAppIntegrationWithFaviconScheduleDelayTimeMs,
                    sAndroidAppIntegrationWithFaviconSkipSchemaCheck,
                    sAndroidAppIntegrationWithFaviconUseLargeFavicon,
                    sAndroidAppIntegrationWithFaviconZeroStateFaviconNumber,
                    sCctAdaptiveButtonEnableOpenInBrowser,
                    sCctAdaptiveButtonEnableVoice,
                    sCctAuthTabEnableHttpsRedirectsVerificationTimeoutMs,
                    sCctAutoTranslatePackageNamesAllowlist,
                    sCctAutoTranslateAllowAllFirstParties,
                    sCctMinimizedIconVariant,
                    sCctMinimizedEnabledByDefaultManufacturerExcludeList,
                    sCctGoogleBottomBarNoVariantHeightDp,
                    sCctGoogleBottomBarButtonList,
                    sCctGoogleBottomBarVariantLayoutsSingleDeckerHeightDp,
                    sCctGoogleBottomBarVariantLayoutsVariantLayout,
                    sCctGoogleBottomBarVariantLayoutsGoogleDseCheckEnabled,
                    sCctResizableForThirdPartiesAllowlistEntries,
                    sCctResizableForThirdPartiesDenylistEntries,
                    sCctResizableForThirdPartiesDefaultPolicy,
                    sClankStartupLatencyInjectionAmountMs,
                    sCollectAndroidFrameTimelineMetricsJankTrackerDelayedStartMs,
                    sDrawKeyNativeEdgeToEdgeDisableRecentTabsE2e,
                    sDrawKeyNativeEdgeToEdgeDisableCctMediaViewerE2e,
                    sDrawKeyNativeEdgeToEdgeDisableHubE2e,
                    sDrawKeyNativeEdgeToEdgeDisableNtpE2e,
                    sDrawKeyNativeEdgeToEdgeDisableIncognitoNtpE2e,
                    sEdgeToEdgeBottomChinOemMinVersions,
                    sEdgeToEdgeBottomChinOemList,
                    sEdgeToEdgeEverywhereIsDebugging,
                    sLogoPolishMediumSize,
                    sLogoPolishLargeSize,
                    sMagicStackAndroidShowAllModules,
                    mMostVisitedTilesReselectLaxSchemeHost,
                    mMostVisitedTilesReselectLaxRef,
                    mMostVisitedTilesReselectLaxQuery,
                    mMostVisitedTilesReselectLaxPath,
                    sNavBarColorAnimationDisableBottomChinColorAnimation,
                    sNavBarColorAnimationDisableEdgeToEdgeLayoutColorAnimation,
                    sNavBarColorMatchesTabBackgroundColorAnimationDisabled,
                    sNewTabSearchEngineUrlAndroidSwapOutNtp,
                    sNotificationTrampolineLongJobDurationMs,
                    sNotificationTrampolineNormalJobDurationMs,
                    sNotificationTrampolineImmediateJobDurationMs,
                    sNotificationTrampolineTimeoutPriorNativeInitMs,
                    sOmahaMinSdkVersionMinSdkVersion,
                    sOptimizationGuidePushNotificationsMaxCacheSize,
                    sPriceChangeModuleSkipShoppingPersistedTabDataDelayedInit,
                    sSearchinCctApplyReferrerId,
                    sSearchinCctOmniboxAllowedPackageNames,
                    sStartSurfaceReturnTimeTabletSecs,
                    sTabResumptionModuleAndroidShowDefaultReason,
                    sTabResumptionModuleAndroidFetchHistoryBackend,
                    sTabResumptionModuleAndroidDisableBlend,
                    sTabResumptionModuleAndroidUseDefaultAppFilter,
                    sTabResumptionModuleAndroidShowSeeMore,
                    sTabResumptionModuleAndroidUseSalientImage,
                    sTabResumptionModuleAndroidMaxTilesNumber,
                    sTabResumptionModuleAndroidEnableV2,
                    sTabResumptionModuleAndroidCombineTabs,
                    sTabStateFlatBufferMigrateStaleTabs,
                    sTabWindowManagerReportIndicesMismatchTimeDiffThresholdMs,
                    sUseChimeAndroidSdkAlwaysRegister,
                    sWebApkMinShellApkVersionValue);

    // Mutable*ParamWithSafeDefault instances.
    /* Alphabetical: */
    public static final MutableBooleanParamWithSafeDefault sShouldBlockCapturesForFullscreenParam =
            sSuppressionToolbarCaptures.newBooleanParam("block_for_fullscreen", false);
    public static final MutableBooleanParamWithSafeDefault sAndroidTabDeclutterArchiveEnabled =
            sAndroidTabDeclutter.newBooleanParam("android_tab_declutter_archive_enabled", true);
    public static final MutableIntParamWithSafeDefault sAndroidTabDeclutterArchiveTimeDeltaHours =
            sAndroidTabDeclutter.newIntParam(
                    "android_tab_declutter_archive_time_delta_hours", 21 * 24);
    public static final MutableBooleanParamWithSafeDefault sAndroidTabDeclutterAutoDeleteEnabled =
            sAndroidTabDeclutter.newBooleanParam(
                    "android_tab_declutter_auto_delete_enabled", false);
    public static final MutableIntParamWithSafeDefault
            sAndroidTabDeclutterAutoDeleteTimeDeltaHours =
                    sAndroidTabDeclutter.newIntParam(
                            "android_tab_declutter_auto_delete_time_delta_hours", 60 * 24);
    public static final MutableIntParamWithSafeDefault sAndroidTabDeclutterIntervalTimeDeltaHours =
            sAndroidTabDeclutter.newIntParam(
                    "android_tab_declutter_interval_time_delta_hours", 7 * 24);
    public static final MutableIntParamWithSafeDefault sAndroidTabDeclutterMaxSimultaneousArchives =
            sAndroidTabDeclutter.newIntParam(
                    "android_tab_declutter_max_simultaneous_archives", 100);
    public static final MutableIntParamWithSafeDefault
            sAndroidTabDeclutterIphMessageDismissThreshold =
                    sAndroidTabDeclutter.newIntParam(
                            "android_tab_declutter_iph_message_dismiss_threshold", 3);
    public static final MutableBooleanParamWithSafeDefault
            sDisableBottomControlsStackerYOffsetDispatching =
                    sBottomBrowserControlsRefactor.newBooleanParam(
                            "disable_bottom_controls_stacker_y_offset", true);
    public static final MutableIntParamWithSafeDefault sTabSwitcherColorBlendAnimateDurationMs =
            sTabSwitcherColorBlendAnimate.newIntParam("animation_duration_ms", 240);
    public static final MutableIntParamWithSafeDefault sTabSwitcherColorBlendAnimateInterpolator =
            sTabSwitcherColorBlendAnimate.newIntParam("animation_interpolator", 0);
}
