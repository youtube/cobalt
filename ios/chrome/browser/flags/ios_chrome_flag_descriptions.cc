// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAppStoreRatingName[] = "Enable the App Store Rating promo.";
const char kAppStoreRatingDescription[] =
    "When enabled, App Store Rating promo will be presented to eligible "
    "users.";

const char kAppStoreRatingLoosenedTriggersName[] =
    "Enable the App Store Rating promo's loosened triggers.";
const char kAppStoreRatingLoosenedTriggersDescription[] =
    "When enabled, App Store Rating promo will have loosened trigger "
    "requirements.";

const char kAutofillAccountProfilesStorageName[] =
    "Enable profile saving in Google Account";
const char kAutofillAccountProfilesStorageDescription[] =
    "When enabled, the profiles would be saved to the Google Account";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillDisableProfileUpdatesName[] =
    "Disables Autofill profile updates from form submissions";
const char kAutofillDisableProfileUpdatesDescription[] =
    "When enabled, Autofill will not apply updates to address profiles based "
    "on data extracted from submitted forms. For testing purposes.";

const char kAutofillDisableSilentProfileUpdatesName[] =
    "Disables Autofill silent profile updates from form submissions";
const char kAutofillDisableSilentProfileUpdatesDescription[] =
    "When enabled, Autofill will not apply silent updates to address profiles. "
    "For testing purposes.";

const char kAutofillEnableCardArtImageName[] = "Enable showing card art images";
const char kAutofillEnableCardArtImageDescription[] =
    "When enabled, card product images (instead of network icons) will be "
    "shown in Payments Autofill UI.";

const char kAutofillEnableMerchantDomainInUnmaskCardRequestName[] =
    "Enable sending merchant domain in server card unmask requests";
const char kAutofillEnableMerchantDomainInUnmaskCardRequestDescription[] =
    "When enabled, requests to unmask cards will include a top-level "
    "merchant_domain parameter populated with the last origin of the main "
    "frame.";

const char kAutofillEnablePaymentsMandatoryReauthOnBlingName[] =
    "Enable mandatory re-auth for payments autofill on Bling";
const char kAutofillEnablePaymentsMandatoryReauthOnBlingDescription[] =
    "When this and the kAutofillEnablePaymentsMandatoryReauth are both "
    "enabled, in use-cases where we would not have triggered any user-visible "
    "authentication to autofill payment methods, we will trigger a device "
    "authentication a device authentication on Bling.";

const char kAutofillEnableRankingFormulaAddressProfilesName[] =
    "Enable new Autofill suggestion ranking formula for address profiles";
const char kAutofillEnableRankingFormulaAddressProfilesDescription[] =
    "When enabled, Autofill will use a new ranking formula to rank Autofill "
    "address profile suggestions.";

const char kAutofillEnableRankingFormulaCreditCardsName[] =
    "Enable new Autofill suggestion ranking formula for credit cards";
const char kAutofillEnableRankingFormulaCreditCardsDescription[] =
    "When enabled, Autofill will use a new ranking formula to rank Autofill "
    "data model credit card suggestions.";

const char kAutofillEnableRemadeDownstreamMetricsName[] =
    "Enable remade Autofill Downstream metrics logging";
const char kAutofillEnableRemadeDownstreamMetricsDescription[] =
    "When enabled, some extra metrics logging for Autofill Downstream will "
    "start.";

const char kAutofillEnableSupportForAdminLevel2Name[] =
    "Enables parsing and filling of administrative area level 2 fields";
const char kAutofillEnableSupportForAdminLevel2Description[] =
    "When enabled, Autofill will parse, save and fill administrative area "
    "level 2 fields in forms.";

const char kAutofillEnableSupportForBetweenStreetsName[] =
    "Enables parsing and filling of between street fields";
const char kAutofillEnableSupportForBetweenStreetsDescription[] =
    "When enabled, Autofill will parse, save and fill between street "
    "fields in forms.";

const char kAutofillEnableSupportForLandmarkName[] =
    "Enables parsing and filling of Landmark fields";
const char kAutofillEnableSupportForLandmarkDescription[] =
    "When enabled, Autofill will parse, save and fill landmark fields in "
    "forms.";

const char kAutofillEnableCardProductNameName[] =
    "Enable showing card product name";
const char kAutofillEnableCardProductNameDescription[] =
    "When enabled, card product name (instead of issuer network) will be shown "
    "in Payments UI.";

const char kAutofillEnableVirtualCardsName[] =
    "Enable virtual card enrollment and retrieval";
const char kAutofillEnableVirtualCardsDescription[] =
    "When enabled, virtual card enrollment and retrieval will be available on "
    "Bling.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsName[] =
    "Parse standalone CVC fields for VCN card on file in forms";
const char kAutofillParseVcnCardOnFileStandaloneCvcFieldsDescription[] =
    "When enabled, Autofill will attempt to find standalone CVC fields for VCN "
    "card on file when parsing forms.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillSuggestServerCardInsteadOfLocalCardName[] =
    "Suggest Server card instead of Local card for deduped cards";
const char kAutofillSuggestServerCardInsteadOfLocalCardDescription[] =
    "When enabled, Autofill suggestions that consist of a local and server "
    "version of the same card will attempt to fill the server card upon "
    "selection instead of the local card.";

const char kAutofillUpdateChromeSettingsLinkToGPayWebName[] =
    "Update Chrome Settings Link to GPay Web";
const char kAutofillUpdateChromeSettingsLinkToGPayWebDescription[] =
    "When enabled, Chrome Settings link directs to GPay Web rather than "
    "Payments Center for payment methods management.";

const char kAutofillUpstreamAllowAdditionalEmailDomainsName[] =
    "Allow Autofill credit card upload save for select non-Google-based "
    "accounts";
const char kAutofillUpstreamAllowAdditionalEmailDomainsDescription[] =
    "When enabled, credit card upload is offered if the user's logged-in "
    "account's domain is from a common email provider.";

const char kAutofillUpstreamAllowAllEmailDomainsName[] =
    "Allow Autofill credit card upload save for all non-Google-based accounts";
const char kAutofillUpstreamAllowAllEmailDomainsDescription[] =
    "When enabled, credit card upload is offered without regard to the user's "
    "logged-in account's domain.";

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

const char kAutofillUseRendererIDsName[] =
    "Autofill logic uses unqiue renderer IDs";
const char kAutofillUseRendererIDsDescription[] =
    "When enabled, Autofill logic uses unique numeric renderer IDs instead "
    "of string form and field identifiers in form filling logic.";

const char kAutofillUseTwoDotsForLastFourDigitsName[] =
    "Autofill uses two '•' instead of four";
const char kAutofillUseTwoDotsForLastFourDigitsDescription[] =
    "When enabled, Autofill surfaces will show two '•' characters instead of "
    "four when displaying the last four digits of a card number";

const char kBottomOmniboxDefaultSettingName[] =
    "Bottom Omnibox Default Setting";
const char kBottomOmniboxDefaultSettingDescription[] =
    "Changes the default setting of the omnibox position. If the user "
    "hasn't already changed the setting, changes the omnibox position to top "
    "or bottom of the screen on iPhone. The default is top omnibox.";

const char kBottomOmniboxDeviceSwitcherResultsName[] =
    "Bottom omnibox device switcher results.";
const char kBottomOmniboxDeviceSwitcherResultsDescription[] =
    "Enabled by default. Retrieve device switcher results for the default "
    "omnibox position.";

const char kBottomOmniboxSteadyStateName[] = "Bottom Omnibox (Steady)";
const char kBottomOmniboxSteadyStateDescription[] =
    "Move the omnibox to the bottom in steady state";

const char kSpotlightDonateNewIntentsName[] = "Donate New Spotlight Intents";
const char kSpotlightDonateNewIntentsDescription[] =
    "Donates relevant intents to Siri when corresponding features are used";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

extern const char kAppleCalendarExperienceKitName[] =
    "Experience Kit Apple Calendar";
extern const char kAppleCalendarExperienceKitDescription[] =
    "When enabled, long pressing on dates will trigger Experience Kit Apple "
    "Calendar event handling.";

const char kClearUndecryptablePasswordsOnSyncName[] =
    "Enable cleaning up of password store on initial Sync";
const char kClearUndecryptablePasswordsOnSyncDescription[] =
    "Once password sync starts for the first time, the currently undecryptable "
    "passwords will be silently cleaned up";

const char kContentPushNotificationsName[] = "Content Push Notifications";
const char kContentPushNotificationsDescription[] =
    "Enables the content push notifications.";

extern const char kPhoneNumberName[] = "Phone number experience enable";
extern const char kPhoneNumberDescription[] =
    "When enabled, one tapping or long pressing on a phone number will trigger "
    "the phone number experience.";

const char kMeasurementsName[] = "Measurements experience enable";
const char kMeasurementsDescription[] =
    "When enabled, one tapping or long pressing on a measurement will trigger "
    "the measurement conversion experience.";

extern const char kEnableExpKitTextClassifierName[] =
    "Text Classifier in Experience Kit";
extern const char kEnableExpKitTextClassifierDescription[] =
    "When enabled, Experience Kit will use Text Classifier library in "
    "entity detection where possible.";

const char kEnableFamilyLinkControlsName[] = "Family Link parental controls";
const char kEnableFamilyLinkControlsDescription[] =
    "Enables parental controls from Family Link on supervised accounts "
    "signed-in to Chrome.";

extern const char kOneTapForMapsName[] = "Enable one Tap Experience for Maps";
extern const char kOneTapForMapsDescription[] =
    "Enables the one tap experience for maps experience kit.";

const char kUseAnnotationsForLanguageDetectionName[] =
    "Enable Shared Web Page Text Fetching";
const char kUseAnnotationsForLanguageDetectionDescription[] =
    "When enabled, both full page intent detection and language detection will "
    "use the same text manager to fetch web page text.";

const char kEnablePopoutOmniboxIpadName[] = "Popout omnibox (iPad)";
const char kEnablePopoutOmniboxIpadDescription[] =
    "Make omnibox popup appear in a detached rounded rectangle below the "
    "omnibox.";

extern const char kMagicStackName[] = "Enable Magic Stack";
extern const char kMagicStackDescription[] =
    "When enabled, a Magic Stack will be shown in the Home surface displaying "
    "a variety of modules.";

const char kCredentialProviderExtensionPromoName[] =
    "Enable the Credential Provider Extension promo.";
const char kCredentialProviderExtensionPromoDescription[] =
    "When enabled, Credential Provider Extension promo will be "
    "presented to eligible users.";

const char kDefaultBrowserVideoInSettingsName[] =
    "Show default browser video in settings experiment";
const char kDefaultBrowserVideoInSettingsDescription[] =
    "When enabled, default browser video will be displayed to user in "
    "settings.";

const char kDefaultBrowserBlueDotPromoName[] = "Default Browser Blue Dot Promo";
const char kDefaultBrowserBlueDotPromoDescription[] =
    "When enabled, a blue dot default browser promo will be shown to eligible "
    "users.";

const char kDefaultBrowserFullscreenPromoExperimentName[] =
    "Default Browser Fullscreen modal experiment";
const char kDefaultBrowserFullscreenPromoExperimentDescription[] =
    "When enabled, will show a modified default browser fullscreen modal promo "
    "UI.";

const char kDefaultBrowserIntentsShowSettingsName[] =
    "Default Browser Intents show settings";
const char kDefaultBrowserIntentsShowSettingsDescription[] =
    "When enabled, external apps can trigger the settings screen showing "
    "default browser tutorial.";

const char kDefaultBrowserRefactoringPromoManagerName[] =
    "Enable the refactoring of the full screen default browser promos to be "
    "included in the promo manager";
const char kDefaultBrowserRefactoringPromoManagerDescription[] =
    "When enabled, the full screen default browser promos will be be included "
    "and managed in the promo manager";

const char kDefaultBrowserPromoForceShowPromoName[] =
    "Skip default browser promo triggering criteria";
const char kDefaultBrowserPromoForceShowPromoDescription[] =
    "When enabled, the user will be able to chose which default browser promo "
    "will skip the triggering criteria. For this flag to effectively force a "
    "default browser promo, either enable kPromosManagerUsesFET and "
    "IPH_iOSPromoDefaultBrowser, or disable kPromosManagerUsesFET and enable "
    "Force Promo (Default Browser) in the system settings experimental flags.";

const char kDefaultBrowserTriggerCriteriaExperimentName[] =
    "Show default browser promo trigger criteria experiment";
const char kDefaultBrowserTriggerCriteriaExperimentDescription[] =
    "When enabled, default browser promo will be displayed to user without "
    "matching all the trigger criteria.";

const char kDefaultBrowserVideoPromoName[] =
    "Enable default browser video promo";
const char kDefaultBrowserVideoPromoDescription[] =
    "When enabled, the user will be presented a video promo showing how to set "
    "Chrome as default browser.";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDiscoverFeedSportCardName[] = "Sport card in Discover feed";
const char kDiscoverFeedSportCardDescription[] =
    "Enables the live sport card in the NTP's Discover feed";

const char kDynamicBackgroundColorName[] = "Use Dynamic theme color";
const char kDynamicBackgroundColorDescription[] =
    "When enabled, the toolbar background is using a dynamic color from the "
    "page's theme color. Takes precedence over Background color if both flags "
    "are enabled.";

const char kDynamicThemeColorName[] = "Use Dynamic background color";
const char kDynamicThemeColorDescription[] =
    "When enabled, the toolbar background is using a dynamic color from the "
    "page's underpage background color. If the color is white, it is ignored.";

const char kEnableDiscoverFeedTopSyncPromoName[] =
    "Enables the top of feed sync promo.";
const char kEnableDiscoverFeedTopSyncPromoDescription[] =
    "When enabled, a sync promotion will be presented to eligible users on top "
    "of the feed cards.";

const char kEnableFeedHeaderSettingsName[] =
    "Enables the feed header settings.";
const char kEnableFeedHeaderSettingsDescription[] =
    "When enabled, some UI elements of the feed header can be modified.";

const char kEnableFriendlierSafeBrowsingSettingsEnhancedProtectionName[] =
    "Enable friendlier safe browsing settings enhanced protection";
const char
    kEnableFriendlierSafeBrowsingSettingsEnhancedProtectionDescription[] =
        "Updates the text, layout, icons, and links on both the privacy guide "
        "and the security settings page.";

const char kEnableFriendlierSafeBrowsingSettingsStandardProtectionName[] =
    "Enable Friendlier Safe Browsing Settings for standard protection";
const char
    kEnableFriendlierSafeBrowsingSettingsStandardProtectionDescription[] =
        "Updates the text and layout on both the privacy guide and the "
        "security "
        "settings page.";

const char kEnableRedInterstitialFaceliftName[] = "Red interstitial facelift";
const char kEnableRedInterstitialFaceliftDescription[] =
    "Enables red interstitial facelift UI changes, including icon, string, and "
    "style changes.";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kEnableAutofillAddressSavePromptAddressVerificationName[] =
    "Autofill Address Save Prompts Address Verification";
const char kEnableAutofillAddressSavePromptAddressVerificationDescription[] =
    "Enable the address verification support in Autofill address save prompts.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnableBookmarksAccountStorageName[] =
    "Enable Bookmarks Account Storage";
const char kEnableBookmarksAccountStorageDescription[] =
    "Enable bookmarks account storage and related UI features.";

const char kEnableCompromisedPasswordsMutingName[] =
    "Enable the muting of compromised passwords in the Password Manager";
const char kEnableCompromisedPasswordsMutingDescription[] =
    "Enable the compromised password alert mutings in Password Manager to be "
    "respected in the app.";

const char kEnableDiscoverFeedDiscoFeedEndpointName[] =
    "Enable discover feed discofeed";
const char kEnableDiscoverFeedDiscoFeedEndpointDescription[] =
    "Enable using the discofeed endpoint for the discover feed.";

const char kEnableFeedAblationName[] = "Enables Feed Ablation";
const char kEnableFeedAblationDescription[] =
    "If Enabled the Feed will be removed from the NTP";

const char kEnableFeedContainmentName[] = "Enables feed containment";
const char kEnableFeedContainmentDescription[] =
    "If enabled, the feed is contained in a module on the Home surface.";

const char kEnableFeedCardMenuSignInPromoName[] =
    "Enable Feed card menu sign-in promotion";
const char kEnableFeedCardMenuSignInPromoDescription[] =
    "Display a sign-in promotion UI when signed out users click on "
    "personalization options within the feed card menu.";

const char kEnableFeedSyntheticCapabilitiesName[] =
    "Enable Feed synthetic capabilities.";
const char kEnableFeedSyntheticCapabilitiesDescription[] =
    "If enabled synthethic capablities will be used to inform the server of "
    "the client capabilities.";

const char kEnableFollowIPHExpParamsName[] =
    "Enable Follow IPH Experiment Parameters";
const char kEnableFollowIPHExpParamsDescription[] =
    "Enable follow IPH experiment parameters.";

const char kEnableFollowManagementInstantReloadName[] =
    "Enable Follow Management Instant Reload";
const char kEnableFollowManagementInstantReloadDescription[] =
    "Enable follow management page instant reloading when being opened.";

const char kEnableFollowUIUpdateName[] = "Enable the Follow UI Update";
const char kEnableFollowUIUpdateDescription[] =
    "Enable Follow UI Update for the Feed.";

const char kEnablePreferencesAccountStorageName[] =
    "Enable the account data storage for preferences for syncing users";
const char kEnablePreferencesAccountStorageDescription[] =
    "Enables storing preferences in a second, Gaia-account-scoped storage for "
    "syncing users";

const char kEnableReadingListAccountStorageName[] =
    "Enable Reading List Account Storage";
const char kEnableReadingListAccountStorageDescription[] =
    "Enable the reading list account storage.";

const char kEnableReadingListSignInPromoName[] =
    "Enable Reading List Sign-in promo";
const char kEnableReadingListSignInPromoDescription[] =
    "Enable the sign-in promo view in the reading list screen.";

const char kEnableSignedOutViewDemotionName[] =
    "Enable signed out user view demotion";
const char kEnableSignedOutViewDemotionDescription[] =
    "Enable signed out user view demotion to avoid repeated content for signed "
    "out users.";

const char kEnableSuggestionsScrollingOnIPadName[] =
    "Enable omnibox suggestions scrolling on iPad";
const char kEnableSuggestionsScrollingOnIPadDescription[] =
    "Enable omnibox suggestions scrolling on iPad and disable suggestions "
    "hiding on keyboard dismissal.";

const char kEnableUIButtonConfigurationName[] =
    "Enable UIButtonConfiguration Usage";
const char kEnableUIButtonConfigurationDescription[] =
    "Enable UIButtonConfiguration usage for UIButtons.";

const char kEnableUserPolicyForSigninAndNoSyncConsentLevelName[] =
    "Enable user policies when signed-in only without sync";
const char kEnableUserPolicyForSigninAndNoSyncConsentLevelDescription[] =
    "Enable the fetch and application of user policies when signed-in only with"
    "a managed account, excluding signed-in+sync.";

const char kEnableUserPolicyForSigninOrSyncConsentLevelName[] =
    "Enable user policies when signed-in only or signed-in+sync";
const char kEnableUserPolicyForSigninOrSyncConsentLevelDescription[] =
    "Enable the fetch and application of user policies when signed-in only or "
    "signed-in+sync with a managed account.";

const char kEnableWebChannelsName[] = "Enable WebFeed";
const char kEnableWebChannelsDescription[] =
    "Enable folowing content from web and display Following feed on NTP based "
    "on sites that users followed.";

const char kFeedBackgroundRefreshName[] = "Enable feed background refresh";
const char kFeedBackgroundRefreshDescription[] =
    "Schedules a feed background refresh after some minimum period of time has "
    "passed after the last refresh.";

const char kFeedDisableHotStartRefreshName[] = "Disable hot start feed refresh";
const char kFeedDisableHotStartRefreshDescription[] =
    "Disables all Discover-controlled foregrounding refreshes.";

const char kFeedExperimentTaggingName[] = "Enable Feed experiment tagging";
const char kFeedExperimentTaggingDescription[] =
    "Makes server experiments visible as client-side experiments.";

const char kFeedInvisibleForegroundRefreshName[] =
    "Enable feed invisible foreground refresh";
const char kFeedInvisibleForegroundRefreshDescription[] =
    "Invisible foreground refresh has two variations. The first is when the "
    "Feed is refreshed after the user ends a Feed session, but the app is "
    "still in the foreground (e.g., user switches tabs, user navigates away "
    "from Feed in current tab). The second is when the Feed is refreshed at "
    "the moment the app is backgrounding (e.g., during extended execution "
    "time).";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kFullScreenPromoOnOmniboxCopyPasteName[] =
    "Trigger Default Browser full-screen promo on omnibox copy-paste";
const char kFullScreenPromoOnOmniboxCopyPasteDescription[] =
    "When enabled full-screen promo will be displayed on omnibox copy-paste "
    "event";

const char kFullscreenPromosManagerSkipInternalLimitsName[] =
    "Fullscreen Promos Manager (Skip internal Impression Limits)";
const char kFullscreenPromosManagerSkipInternalLimitsDescription[] =
    "When enabled, the internal Impression Limits of the Promos Manager will "
    "be ignored; this is useful for local development.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

const char kIncognitoNtpRevampName[] = "Revamped Incognito New Tab Page";
const char kIncognitoNtpRevampDescription[] =
    "When enabled, Incognito new tab page will have an updated UI.";

const char kIndicateIdentityErrorInOverflowMenuName[] =
    "Indicate Identity Error in Overflow Menu";
const char kIndicateIdentityErrorInOverflowMenuDescription[] =
    "When enabled, the Overflow Menu indicates the identity error with an "
    "error badge on the Settings destination";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kIOSBrowserEditMenuMetricsName[] = "Browser edit menu metrics";
const char kIOSBrowserEditMenuMetricsDescription[] =
    "Collect metrics for edit menu usage.";

const char kIOSEditMenuPartialTranslateName[] =
    "Enable partial translate in edit menu";
const char kIOSEditMenuPartialTranslateDescription[] =
    "Replace the Apple translate entry in the web edit menu to use Google "
    "Translate instead.";

extern const char kIOSEditMenuSearchWithName[] =
    "Enable Search with in edit menu";
extern const char kIOSEditMenuSearchWithDescription[] =
    "Add an entry to search the web selection with your default search engine.";

extern const char kIOSEditMenuHideSearchWebName[] =
    "Hides Search Web in edit menu";
extern const char kIOSEditMenuHideSearchWebDescription[] =
    "Hides the Search Web entry in edit menu.";

const char kIOSForceTranslateEnabledName[] = "Allow force translate on iOS.";
const char kIOSForceTranslateEnabledDescription[] =
    "Enable the translate feature when language detection failed.";

const char kIOSIncognitoDownloadsWarningName[] =
    "Enable Incognito downloads warning on iOS";
const char kIOSIncognitoDownloadsWarningDescription[] =
    "When enabled, users will be warned that downloaded files are saved on the "
    "device and might be seen by other users even if they are in Incognito.";

const char kIOSNewPostRestoreExperienceName[] = "New Post Restore Experience";
const char kIOSNewPostRestoreExperienceDescription[] =
    "When enabled, a prompt will be presented after a device restore to "
    "allow the user to sign in again.";

const char kIOSParcelTrackingName[] = "Parcel Tracking";
const char kIOSParcelTrackingDescription[] =
    "When enabled, the user will be able to track their packages.";

const char kIOSPasswordAuthOnEntryName[] = "Password Manager Auth on Entry";
const char kIOSPasswordAuthOnEntryDescription[] =
    "Requires Local Authentication before showing saved credentials in "
    "the Password Manager Main Page. Ignored if 'Password Manager Auth on "
    "Entry V2' is enabled.";

const char kIOSPasswordAuthOnEntryV2Name[] =
    "Password Manager Auth on Entry V2";
const char kIOSPasswordAuthOnEntryV2Description[] =
    "Requires Local Authentication before showing saved credentials in "
    "Password Manager subpages. Supersedes `Password Manager Auth on Entry` if "
    "enabled.";

const char kIOSPasswordCheckupName[] = "Password Checkup";
const char kIOSPasswordCheckupDescription[] =
    "Enables displaying and managing compromised, weak and reused credentials "
    "in the Password Manager.";

const char kIOSPasswordUISplitName[] = "Password Manager UI Split";
const char kIOSPasswordUISplitDescription[] =
    "Splits Password Settings and "
    "Password Manager into two separate UIs.";

const char kIOSSaveToDriveName[] = "IOS Save to Drive";
const char kIOSSaveToDriveDescription[] =
    "Enables the Save to Drive feature on iOS.";

const char kIOSSaveToPhotosName[] = "IOS Save to Photos";
const char kIOSSaveToPhotosDescription[] =
    "Enables the Save to Photos feature on iOS.";

const char kIOSSetUpListName[] = "IOS Set Up List";
const char kIOSSetUpListDescription[] =
    "Displays an unobtrusive list of set up tasks on Home for a new user.";

const char kIOSPasswordBottomSheetName[] = "IOS Password Manager Bottom Sheet";
const char kIOSPasswordBottomSheetDescription[] =
    "Enables the display of the password bottom sheet on IOS.";

const char kIOSPasswordSettingsBulkUploadLocalPasswordsName[] =
    "iOS Bulk Upload Local Passwords";
const char kIOSPasswordSettingsBulkUploadLocalPasswordsDescription[] =
    "Enables bulk uploading local passwords for eligible users in the iOS "
    "password settings.";

const char kIOSPaymentsBottomSheetName[] = "IOS Payments Manager Bottom Sheet";
const char kIOSPaymentsBottomSheetDescription[] =
    "Enables the display of the payments bottom sheet on IOS.";

const char kNewTabPageFieldTrialName[] =
    "New tab page features that target new users";
const char kNewTabPageFieldTrialDescription[] =
    "Enables new tab page features that are available on first run for new "
    "Chrome iOS users.";

const char kIOSSharedHighlightingColorChangeName[] =
    "IOS Shared Highlighting color change";
const char kIOSSharedHighlightingColorChangeDescription[] =
    "Changes the Shared Highlighting color of the text fragment"
    "away from the default yellow in iOS. Works with #scroll-to-text-ios flag.";

const char kIOSSharedHighlightingAmpName[] = "Shared Highlighting on AMP pages";
const char kIOSSharedHighlightingAmpDescription[] =
    "Enables the Create Link option on AMP pages.";

const char kIOSSharedHighlightingV2Name[] = "Text Fragments UI improvements";
const char kIOSSharedHighlightingV2Description[] =
    "Enables improvements to text fragments UI, including a menu for removing "
    "or resharing a highlight.";

const char kIPHForSafariSwitcherName[] = "IPH for Safari Switcher";
const char kIPHForSafariSwitcherDescription[] =
    "Enables displaying IPH for users who are considered Safari Switcher";

const char kIPHiOSPromoPasswordManagerWidgetName[] =
    "Password Manager widget promo IPH";
const char kIPHiOSPromoPasswordManagerWidgetDescription[] =
    "Enables displaying the Password Manager widget promo IPH when users "
    "navigate to the Password Manager";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kMetrickitNonCrashReportName[] = "Metrickit non-crash reports";
const char kMetrickitNonCrashReportDescription[] =
    "Enables sending Metrickit reports for non crash type (hang, "
    "cpu-exception, diskwrite-exception)";

const char kMixedContentAutoupgradeName[] = "Auto-upgrade mixed content";
const char kMixedContentAutoupgradeDescription[] =
    "Enables auto-upgrading of mixed content images, audio and video";

const char kModernTabStripName[] = "Modern TabStrip";
const char kModernTabStripDescription[] =
    "When enabled, the newly implemented tabstrip can be tested.";

const char kNativeFindInPageName[] = "Native Find in Page";
const char kNativeFindInPageDescription[] =
    "When enabled, the JavaScript implementation of the Find in Page feature "
    "is replaced with a native implementation which also enables searching "
    "text in PDF files. Available for iOS 16 or later.";

const char kNewNTPOmniboxLayoutName[] = "New NTP Omnibox Layout";
const char kNewNTPOmniboxLayoutDescription[] =
    "Enables the new NTP omnibox layout with leading-edge aligned hint label "
    "and magnifying glass icon.";

const char kNewOverflowMenuName[] = "New Overflow Menu";
const char kNewOverflowMenuDescription[] = "Enables the new overflow menu";

const char kOverflowMenuCustomizationName[] = "Overflow Menu Customization";
const char kOverflowMenuCustomizationDescription[] =
    "Allow users to customize the order of the overflow menu";

const char kNTPViewHierarchyRepairName[] = "NTP View Hierarchy Repair";
const char kNTPViewHierarchyRepairDescription[] =
    "Checks if NTP view hierarchy is broken and fixes it if necessary.";

const char kOmniboxCompanyEntityIconAdjustmentName[] =
    "Omnibox Company Entity Icon Adjustment";
const char kOmniboxCompanyEntityIconAdjustmentDescription[] =
    "When enabled, company entity icons may be replaced based on the search "
    "suggestions and their corresponding order.";

const char kOmniboxGroupingFrameworkForZPSName[] =
    "Omnibox Grouping Framework for ZPS";
const char kOmniboxGroupingFrameworkForZPSDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxGroupingFrameworkForTypedSuggestionsName[] =
    "Omnibox Grouping Framework for Typed Suggestions";
const char kOmniboxGroupingFrameworkForTypedSuggestionsDescription[] =
    "Enables an alternative grouping implementation for omnibox "
    "autocompletion.";

const char kOmniboxHttpsUpgradesName[] = "Omnibox HTTPS upgrades";
const char kOmniboxHttpsUpgradesDescription[] =
    "Enables HTTPS upgrades for omnibox navigations typed without a scheme";

const char kOmniboxInspireMeName[] = "Omnibox Trending Queries";
const char kOmniboxInspireMeDescription[] =
    "When enabled, appends additional suggestions based on local trends and "
    "optionally extends the ZPS limit.";

const char kOmniboxInspireMeSignedOutName[] =
    "Omnibox Trending Queries For Signed-Out users";
const char kOmniboxInspireMeSignedOutDescription[] =
    "When enabled, appends additional suggestions based on local trends and "
    "optionally extends the ZPS limit (for signed out users).";

const char kOmniboxKeyboardPasteButtonName[] = "Omnibox keyboard paste button";
const char kOmniboxKeyboardPasteButtonDescription[] =
    "Enables paste button in the omnibox's keyboard accessory. Only available "
    "from iOS 16 onward.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxMaxZPSMatchesName[] = "Omnibox Max ZPS Matches";
const char kOmniboxMaxZPSMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "zero-prefix state in the omnibox (e.g. on NTP when tapped on OB).";

const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[] =
    "Omnibox on device head suggestions (incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for incognito";

const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[] =
    "Omnibox on device head suggestions (non-incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for non-incognito";

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "Limit the number of URL suggestions in the omnibox. The omnibox will "
    "still display more than MaxURLMatches if there are no non-URL suggestions "
    "to replace them.";

const char kOmniboxNewImplementationName[] =
    "Use experimental omnibox textfield";
const char kOmniboxNewImplementationDescription[] =
    "Uses a textfield implementation that doesn't use UILabels internally";

const char kOmniboxFocusTriggersSRPZeroSuggestName[] =
    "Allow Omnibox contextual web on-focus suggestions on the SRP";
const char kOmniboxFocusTriggersSRPZeroSuggestDescription[] =
    "Enables on-focus suggestions on the Search Results page. Requires "
    "on-focus suggestions for the contextual web to be enabled. Will only work "
    "if user is signed-in and syncing.";

const char kOmniboxLocalHistoryZeroSuggestBeyondNTPName[] =
    "Allow local history zero-prefix suggestions beyond NTP";
const char kOmniboxLocalHistoryZeroSuggestBeyondNTPDescription[] =
    "Enables local history zero-prefix suggestions in every context in which "
    "the remote zero-prefix suggestions are enabled.";

const char kOmniboxOnDeviceTailSuggestionsName[] =
    "Omnibox on device tail suggestions";
const char kOmniboxOnDeviceTailSuggestionsDescription[] =
    "Google tail non personalized search suggestions provided by a compact on "
    "device model.";

const char kOmniboxPopulateShortcutsDatabaseName[] =
    "Omnibox shortcuts database";
const char kOmniboxPopulateShortcutsDatabaseDescription[] =
    "Enables storing successful query/match in the omnibox shortcut database "
    "to provider better suggestions ranking.";

const char kOmniboxReportAssistedQueryStatsName[] =
    "Omnibox Assisted Query Stats param";
const char kOmniboxReportAssistedQueryStatsDescription[] =
    "Enables reporting the Assisted Query Stats param in search destination "
    "URLs originated from the Omnibox.";

const char kOmniboxReportSearchboxStatsName[] =
    "Omnibox Searchbox Stats proto param";
const char kOmniboxReportSearchboxStatsDescription[] =
    "Enables reporting the serialized Searchbox Stats proto param in search "
    "destination URLs originated from the Omnibox.";

extern const char kOmniboxSuggestionsRTLImprovementsName[] =
    "Omnibox Improved RTL Suggestion Layout";
extern const char kOmniboxSuggestionsRTLImprovementsDescription[] =
    "Improved layout for suggestions in right-to-left contexts";

const char kOmniboxTailSuggestName[] = "Omnibox Tail suggestions";
const char kOmniboxTailSuggestDescription[] =
    "Enables tail search suggestions. Search suggestions only matching the end "
    "of users input text.";

const char kOmniboxZeroSuggestInMemoryCachingName[] =
    "Omnibox Zero Prefix Suggestion in-memory caching";
const char kOmniboxZeroSuggestInMemoryCachingDescription[] =
    "Enables in-memory caching of zero prefix suggestions.";

const char kOmniboxZeroSuggestPrefetchingName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on NTP";
const char kOmniboxZeroSuggestPrefetchingDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the New Tab page.";

const char kOmniboxZeroSuggestPrefetchingOnSRPName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on SRP";
const char kOmniboxZeroSuggestPrefetchingOnSRPDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Search Results page.";

const char kOmniboxZeroSuggestPrefetchingOnWebName[] =
    "Omnibox Zero Prefix Suggestion Prefetching on the Web";
const char kOmniboxZeroSuggestPrefetchingOnWebDescription[] =
    "Enables prefetching of the zero prefix suggestions for eligible users "
    "on the Web (i.e. non-NTP and non-SRP URLs).";

const char kOnlyAccessClipboardAsyncName[] =
    "Only access the clipboard asynchronously";
const char kOnlyAccessClipboardAsyncDescription[] =
    "Only accesses the clipboard asynchronously.";

const char kOptimizationGuideDebugLogsName[] =
    "Enable optimization guide debug logs";
const char kOptimizationGuideDebugLogsDescription[] =
    "Enables the optimization guide to log and save debug messages that can be "
    "shown in the internals page.";

const char kOptimizationGuideInstallWideModelStoreName[] =
    "Enables the new optimization guide install-wide model store";
const char kOptimizationGuideInstallWideModelStoreDescription[] =
    "Enables the new model store that is per Chrome installation and can "
    "share models across user profiles.";

const char kOptimizationGuidePushNotificationClientName[] =
    "Enable optimization guide push notification client";
const char kOptimizationGuidePushNotificationClientDescription[] =
    "Enables the client that handles incoming push notifications on behalf of "
    "the optimization guide.";

const char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
const char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

const char kPasswordSharingName[] = "Enables password sharing";
const char kPasswordSharingDescription[] =
    "Enables password sharing between members of the same family.";

const char kEnablePolicyTestPageName[] =
    "Enable access to the policy test page";
const char kEnablePolicyTestPageDescription[] =
    "When enabled, allows the policy test page to be accessed at "
    "chrome://policy/test.";

const char kPostRestoreDefaultBrowserPromoName[] =
    "Post Restore Default Browser Promo";
const char kPostRestoreDefaultBrowserPromoDescription[] =
    "When enabled, the user will be presented a promo showing how to set "
    "Chrome as default browser after losing their default browser status from "
    "an iOS restore.";

const char kPrivacyGuideIosName[] = "Privacy Guide on iOS";
const char kPrivacyGuideIosDescription[] =
    "Shows a new subpage in Settings that helps the user to review various "
    "privacy settings.";

const char kPromosManagerUsesFETName[] = "Promos Manager using FET";
const char kPromosManagerUsesFETDescription[] =
    "Migrates the Promos Manager to use the Feature Engagement Tracker as its "
    "impression tracking system";

const char kIPHPriceNotificationsWhileBrowsingName[] =
    "Price Tracking IPH Display";
const char kIPHPriceNotificationsWhileBrowsingDescription[] =
    "Displays the Price Tracking IPH when the user navigates to a "
    "product "
    "webpage that supports price tracking.";

const char kNotificationSettingsMenuItemName[] =
    "Notification Settings Menu Item";
const char kNotificationSettingsMenuItemDescription[] =
    "Displays the menu item for the notification controls inside the chrome "
    "settings UI.";

const char kRemoveExcessNTPsExperimentName[] = "Remove extra New Tab Pages";
const char kRemoveExcessNTPsExperimentDescription[] =
    "When enabled, extra tabs with the New Tab Page open and no navigation "
    "history will be removed.";

const char kReplaceSyncPromosWithSignInPromosName[] =
    "Replace all sync-related UI with sign-in ones";
const char kReplaceSyncPromosWithSignInPromosDescription[] =
    "When enabled, all sync-related promos will be replaced by sign-in ones.";

const char kRestoreSessionFromCacheName[] =
    "Use native WKWebView sesion restoration (iOS15 only).";
const char kRestoreSessionFromCacheDescription[] =
    "Enable instant session restoration for faster web session restoration "
    "(iOS15 only).";

const char kSafeBrowsingAvailableName[] = "Make Safe Browsing available";
const char kSafeBrowsingAvailableDescription[] =
    "When enabled, navigation URLs are compared to Safe Browsing blocklists, "
    "subject to an opt-out preference.";

const char kSafeBrowsingRealTimeLookupName[] = "Enable real-time Safe Browsing";
const char kSafeBrowsingRealTimeLookupDescription[] =
    "When enabled, navigation URLs are checked using real-time queries to Safe "
    "Browsing servers, subject to an opt-in preference.";

const char kSafetyCheckMagicStackName[] = "Enable Safety Check (Magic Stack)";
const char kSafetyCheckMagicStackDescription[] =
    "When enabled, the Safety Check module will be displayed in the Magic "
    "Stack.";

const char kScreenTimeIntegrationName[] = "Enables ScreenTime Integration";
const char kScreenTimeIntegrationDescription[] =
    "Enables integration with ScreenTime in iOS 14.0 and above.";

const char kSegmentationPlatformIosModuleRankerName[] =
    "Enable Magic Stack Segmentation Ranking";
const char kSegmentationPlatformIosModuleRankerDescription[] =
    "Enables the Segmentation platform to rank Magic Stack modules";

const char kSendUmaOverAnyNetwork[] =
    "Send UMA data over any network available.";
const char kSendUmaOverAnyNetworkDescription[] =
    "When enabled, will send UMA data over either WiFi or cellular by default.";

const char kSharedHighlightingIOSName[] = "Enable Shared Highlighting features";
const char kSharedHighlightingIOSDescription[] =
    "Adds a Link to Text option in the Edit Menu which generates URLs with a "
    "text fragment.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kShowInactiveTabsCountName[] =
    "Show Inactive Tabs count in Tab Grid";
const char kShowInactiveTabsCountDescription[] =
    "When enabled, the count of Inactive Tabs is shown in the Inactive Tabs "
    "button that appears in the Tab Grid.";

const char kSkipUndecryptablePasswordsName[] =
    "Enable silent ignoring of undecryptable passwords";
const char kSkipUndecryptablePasswordsDescription[] =
    "The password store will silently skip undecryptable passwords when "
    "reading them";

const char kSpotlightOpenTabsSourceName[] = "Show Open local tabs in Spotlight";
const char kSpotlightOpenTabsSourceDescription[] =
    "Donate local open tabs items to iOS Search Engine Spotlight";

const char kSpotlightReadingListSourceName[] = "Show Reading List in Spotlight";
const char kSpotlightReadingListSourceDescription[] =
    "Donate Reading List items to iOS Search Engine Spotlight";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncSessionOnVisibilityChangedName[] =
    "Sync session when tab visibility changes";
const char kSyncSessionOnVisibilityChangedDescription[] =
    "This flag enables session syncing when the visibility of a tab changes.";

const char kSyncSegmentsDataName[] = "Use synced segments data";
const char kSyncSegmentsDataDescription[] =
    "Enables history's segments to include foreign visits from syncing "
    "devices.";

const char kStartSurfaceName[] = "Start Surface";
const char kStartSurfaceDescription[] =
    "Enable showing the Start Surface when launching Chrome via clicking the "
    "icon or the app switcher.";

const char kDownloadServiceForegroundSessionName[] =
    "Download service foreground download";
const char kDownloadServiceForegroundSessionDescription[] =
    "Enable download service to download in app foreground only";

const char kTFLiteLanguageDetectionName[] = "TFLite-based Language Detection";
const char kTFLiteLanguageDetectionDescription[] =
    "Uses TFLite for language detection in place of CLD3";

const char kTFLiteLanguageDetectionIgnoreName[] =
    "Ignore TFLite-based Language Detection";
const char kTFLiteLanguageDetectionIgnoreDescription[] =
    "Computes the TFLite language detection but ignore the result and uses the "
    "CLD3 detection instead.";

const char kThemeColorInTopToolbarName[] = "Top toolbar use page's theme color";
const char kThemeColorInTopToolbarDescription[] =
    "When enabled with bottom omnibox, the top toolbar background color is the "
    "page's theme color. Disabled when a dynamic color flag is enabled.";

const char kIOSHideFeedWithSearchChoiceName[] = "Hide Feed with Search Choice";
const char kIOSHideFeedWithSearchChoiceDescription[] =
    "When enabled, the feed and feed header are hidden depending on the "
    "Search Engine setting.";

const char kIOSLargeFakeboxName[] = "Enable Large Fakebox on Home";
const char kIOSLargeFakeboxDescription[] =
    "When enabled, the Fakebox on Home appears larger and has an updated "
    "design.";

const char kIOSLensUseDirectUploadName[] =
    "Use direct upload for Lens searches";
const char kIOSLensUseDirectUploadDescription[] =
    "When enabled, use the direct upload Lens endpoint when searching images "
    "with Lens.";

const char kEnableLensInHomeScreenWidgetName[] =
    "Enable Google Lens in the Home Screen Widget";
const char kEnableLensInHomeScreenWidgetDescription[] =
    "When enabled, use Lens to search for images from your device camera "
    "menu when Google is the selected search engine, accessible from the"
    "home screen widget.";

const char kEnableLensInKeyboardName[] =
    "Enable Google Lens in the Omnibox Keyboard";
const char kEnableLensInKeyboardDescription[] =
    "When enabled, use Lens to search for images from your device camera "
    "menu when Google is the selected search engine, accessible from the"
    "omnibox keyboard.";

const char kEnableLensInNTPName[] = "Enable Google Lens in the NTP";
const char kEnableLensInNTPDescription[] =
    "When enabled, use Lens to search for images from your device camera "
    "menu when Google is the selected search engine, accessible from the"
    "new tab page.";

const char kEnableLensInOmniboxCopiedImageName[] =
    "Enable Google Lens in the Omnibox for Copied Images";
const char kEnableLensInOmniboxCopiedImageDescription[] =
    "When enabled, use Lens to search images from your device clipboard "
    "when Google is the selected search engine, accessible from the omnibox or "
    "popup menu.";

const char kEnableSessionSerializationOptimizationsName[] =
    "Session Serialization Optimization";
const char kEnableSessionSerializationOptimizationsDescription[] =
    "Enables the use of multiple separate files to save the session state "
    "and the ability to load only the minimum amount of data when restoring "
    "the session from disk.";

const char kTabGridRefactoringName[] = "Enable tab grid refactoring";
const char kTabGridRefactoringDescription[] =
    "When enabled, the Tab Grid use the refactored version, it should not have "
    "any visual difference nor different feature with the legacy one.";

const char kTabGridNewTransitionsName[] = "Enable new TabGrid transitions";
const char kTabGridNewTransitionsDescription[] =
    "When enabled, the new Tab Grid to Browser (and vice versa) transitions"
    "are used.";

const char kTabInactivityThresholdName[] = "Change Tab inactivity threshold";
const char kTabInactivityThresholdDescription[] =
    "When enabled, the tabs older than the threshold are considered inactive "
    "and set aside in the Inactive Tabs section of the TabGrid."
    "IMPORTANT: If you ever used the in-app settings for Inactive Tabs, this "
    "flag is never read again.";

const char kTabPickupMinimumDelayName[] =
    "Add delay between tab pickup banners";
const char kTabPickupMinimumDelayDescription[] =
    "When enabled, adds a 2h delay between the presentation of two tab pickup "
    "banners.";

const char kTabPickupThresholdName[] = "Enable and change tab pickup threshold";
const char kTabPickupThresholdDescription[] =
    "When enabled, an infobar will be displayed when the latest tab used from "
    "another device is yougner than the threshold.";

const char kTabResumptionName[] = "Enable Tab Resumption";
const char kTabResumptionDescription[] =
    "When enabled, offer users with a quick shortcut to resume the last synced "
    "tab from another device.";

const char kUseLoadSimulatedRequestForOfflinePageName[] =
    "Use loadSimulatedRequest:responseHTMLString: when displaying offline "
    "pages";
const char kUseLoadSimulatedRequestForOfflinePageDescription[] =
    "When enabled, the offline pages uses the iOS 15 "
    "loadSimulatedRequest:responseHTMLString: API";

const char kWaitThresholdMillisecondsForCapabilitiesApiName[] =
    "Maximum wait time (in seconds) for a response from the Account "
    "Capabilities API";
const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[] =
    "Used for testing purposes to test waiting thresholds in dev.";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kWebFeedFeedbackRerouteName[] =
    "Send discover feed feedback to a updated destination";
const char kWebFeedFeedbackRerouteDescription[] =
    "Directs discover feed feedback to a new target for better handling of the"
    "feedback reports.";

const char kWebPageDefaultZoomFromDynamicTypeName[] =
    "Use dynamic type size for default text zoom level";
const char kWebPageDefaultZoomFromDynamicTypeDescription[] =
    "When enabled, the default text zoom level for a website comes from the "
    "current dynamic type setting.";

const char kWebPageAlternativeTextZoomName[] =
    "Use different method for zooming web pages";
const char kWebPageAlternativeTextZoomDescription[] =
    "When enabled, switches the method used to zoom web pages.";

const char kWebPageTextZoomIPadName[] = "Enable text zoom on iPad";
const char kWebPageTextZoomIPadDescription[] =
    "When enabled, text zoom works again on iPad";

const char kWhatsNewIOSM116Name[] = "Enable What's New M116.";
const char kWhatsNewIOSM116Description[] =
    "When enabled, What's New will display new features and a chrome tip.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions
