// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// Sections used in Settings page.
typedef NS_ENUM(NSInteger, SettingsSectionIdentifier) {
  SettingsSectionIdentifierSignIn = kSectionIdentifierEnumZero,
  SettingsSectionIdentifierAccount,
  SettingsSectionIdentifierBasics,
  SettingsSectionIdentifierAdvanced,
  SettingsSectionIdentifierInfo,
  SettingsSectionIdentifierDebug,
  SettingsSectionIdentifierDefaults,
};

// Item types used per Setting section.
typedef NS_ENUM(NSInteger, SettingsItemType) {
  SettingsItemTypeSignInButton = kItemTypeEnumZero,
  SettingsItemTypeSigninPromo,
  SettingsItemTypeAccount,
  SettingsItemTypeGoogleSync,
  SettingsItemTypeGoogleServices,
  SettingsItemTypeHeader,
  SettingsItemTypeSearchEngine,
  SettingsItemTypeManagedDefaultSearchEngine,
  SettingsItemTypePasswords,
  SettingsItemTypeAutofillCreditCard,
  SettingsItemTypeAutofillProfile,
  SettingsItemTypeVoiceSearch,
  SettingsItemTypeBottomOmnibox,
  SettingsItemTypeNotifications,
  SettingsItemTypePrivacy,
  SettingsItemTypeLanguageSettings,
  SettingsItemTypeContentSettings,
  SettingsItemTypeBandwidth,
  SettingsItemTypeAboutChrome,
  SettingsItemTypeMemoryDebugging,
  SettingsItemTypeViewSource,
  SettingsItemTypeTableCellCatalog,
  SettingsItemTypeArticlesForYou,
  SettingsItemTypeManagedArticlesForYou,
  SettingsItemTypeSafetyCheck,
  SettingsItemTypeDefaultBrowser,
  SettingsItemTypeSigninDisabled,
  SettingsItemTypeTabs,
};

// The accessibility identifier of the settings TableView.
extern NSString* const kSettingsTableViewId;

// The accessibility identifier of the sign in cell.
extern NSString* const kSettingsSignInCellId;

// The accessibility identifier of the sign in cell when sign-in is disabled by
// policy.
extern NSString* const kSettingsSignInDisabledByPolicyCellId;

// The accessibility identifier of the sign in cell when sign-in is disabled.
extern NSString* const kSettingsSignInDisabledCellId;

// The accessibility identifier of the account cell.
extern NSString* const kSettingsAccountCellId;

// The accessibility identifier of the Search Engine cell.
extern NSString* const kSettingsSearchEngineCellId;

// The accessibility identifier of the Managed Search Engine cell.
extern NSString* const kSettingsManagedSearchEngineCellId;

// The accessibility identifier of the Voice Search cell.
extern NSString* const kSettingsVoiceSearchCellId;

// The accessibility identifier of the Bottom Omnibox cell.
extern NSString* const kSettingsBottomOmniboxCellId;

// The accessibility identifier of the Sync and Google services cell.
extern NSString* const kSettingsGoogleSyncAndServicesCellId;

// The accessibility identifier of the Google services cell.
extern NSString* const kSettingsGoogleServicesCellId;

// The accessibility identifier of the Passwords cell.
extern NSString* const kSettingsPasswordsCellId;

// The accessibility identifier of the Payment Methods cell.
extern NSString* const kSettingsPaymentMethodsCellId;

// The accessibility identifier of the Addresses and More cell.
extern NSString* const kSettingsAddressesAndMoreCellId;

// The accessibility identifier of the Privacy cell.
extern NSString* const kSettingsPrivacyCellId;

// The accessibility identifier of the Article Suggestions cell.
extern NSString* const kSettingsArticleSuggestionsCellId;

// The accessibility identifier of the Languages cell.
extern NSString* const kSettingsLanguagesCellId;

// The accessibility identifier of the Content Settings cell.
extern NSString* const kSettingsContentSettingsCellId;

// The accessibility identifier of the Bandwidth cell.
extern NSString* const kSettingsBandwidthCellId;

// The accessibility identifier of the About cell.
extern NSString* const kSettingsAboutCellId;

// The accessibility identifier of the Open Source Licences cell.
extern NSString* const kSettingsOpenSourceLicencesCellId;

// The accessibility identifier of the TOS cell.
extern NSString* const kSettingsTOSCellId;

// The accessibility identifier of the Privacy Notice cell.
extern NSString* const kSettingsPrivacyNoticeCellId;

// The accessibility identifier of the Preload cell.
extern NSString* const kSettingsPreloadCellId;

// The accessibility identifier of the Block Popups cell.
extern NSString* const kSettingsBlockPopupsCellId;

// The accessibility identifier of the Show Link Preview cell.
extern NSString* const kSettingsShowLinkPreviewCellId;

// The accessibility identifier of the Default Apps cell.
extern NSString* const kSettingsDefaultAppsCellId;

// The accessibility identifier of the Add Language cell.
extern NSString* const kSettingsAddLanguageCellId;

// The accessibility identifier of the Clear Browsing Data cell.
extern NSString* const kSettingsClearBrowsingDataCellId;

// The accessibility identifier of the Handoff cell.
extern NSString* const kSettingsHandoffCellId;

// The accessibility identifier of the Cookies cell.
extern NSString* const kSettingsCookiesCellId;

// The accessibility identifier of the Default Site Mode cell.
extern NSString* const kSettingsDefaultSiteModeCellId;

// The accessibility identifier of the default browser settings TableView.
extern NSString* const kDefaultBrowserSettingsTableViewId;

extern NSString* const kSettingsHttpsOnlyModeCellId;

// The accessibility identifier of the Incognito interstitial setting.
extern NSString* const kSettingsIncognitoInterstitialId;

// The accessibility identifier of the Incognito interstitial setting
// when the setting is disabled because of Enterprise policy.
extern NSString* const kSettingsIncognitoInterstitialDisabledId;

// The accessibility identifier of the Notifications setting.
extern NSString* const kSettingsNotificationsId;

// The accessibility identifier of the tabs cell.
extern NSString* const kSettingsTabsCellId;

// The accessibility identifier of the move inactive tabs settings cell.
extern NSString* const kSettingsMoveInactiveTabsCellId;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_CONSTANTS_H_
