// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

id<GREYMatcher> WindowWithNumber(int window_number) {
  return [ChromeMatchersAppInterface windowWithNumber:window_number];
}

id<GREYMatcher> BlockerWindowWithNumber(int window_number) {
  return [ChromeMatchersAppInterface blockerWindowWithNumber:window_number];
}

id<GREYMatcher> MatchInWindowWithNumber(int window_number,
                                        id<GREYMatcher> matcher) {
  return grey_allOf(matcher, grey_ancestor(WindowWithNumber(window_number)),
                    nil);
}

id<GREYMatcher> MatchInBlockerWindowWithNumber(int window_number,
                                               id<GREYMatcher> matcher) {
  return grey_allOf(matcher,
                    grey_ancestor(BlockerWindowWithNumber(window_number)), nil);
}

id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabel:label];
}

id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:message_id];
}

id<GREYMatcher> ImageViewWithImageNamed(NSString* imageName) {
  return [ChromeMatchersAppInterface imageViewWithImageNamed:imageName];
}

id<GREYMatcher> ImageViewWithImage(UIImage* image) {
  return [ChromeMatchersAppInterface imageViewWithImage:image];
}

id<GREYMatcher> ButtonWithImage(int image_id) {
  return [ChromeMatchersAppInterface buttonWithImage:image_id];
}

id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchersAppInterface
      staticTextWithAccessibilityLabelID:message_id];
}

id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchersAppInterface staticTextWithAccessibilityLabel:label];
}

id<GREYMatcher> ContainsPartialText(NSString* text) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    return [[element text] containsString:text];
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString
                       stringWithFormat:@"containsPartialText('%@')", text]];
  };
  id<GREYMatcher> matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_anyOf(grey_kindOfClassName(@"UILabel"),
                               grey_kindOfClassName(@"UITextField"),
                               grey_kindOfClassName(@"UITextView"), nil),
                    matcher, nil);
}

id<GREYMatcher> HeaderWithAccessibilityLabelId(int message_id) {
  return [ChromeMatchersAppInterface headerWithAccessibilityLabelID:message_id];
}

id<GREYMatcher> HeaderWithAccessibilityLabel(NSString* label) {
  return [ChromeMatchersAppInterface headerWithAccessibilityLabel:label];
}

id<GREYMatcher> NavigationBarTitleWithAccessibilityLabelId(int label_id) {
  return [ChromeMatchersAppInterface
      navigationBarTitleWithAccessibilityLabelID:label_id];
}

id<GREYMatcher> TextFieldForCellWithLabelId(int message_id) {
  return [ChromeMatchersAppInterface textFieldForCellWithLabelID:message_id];
}

id<GREYMatcher> IconViewForCellWithLabelId(int message_id,
                                           NSString* icon_type) {
  return [ChromeMatchersAppInterface iconViewForCellWithLabelID:message_id
                                                       iconType:icon_type];
}

id<GREYMatcher> PrimaryToolbar() {
  return [ChromeMatchersAppInterface primaryToolbar];
}

id<GREYMatcher> CancelButton() {
  return [ChromeMatchersAppInterface cancelButton];
}

id<GREYMatcher> NavigationBarCancelButton() {
  return [ChromeMatchersAppInterface navigationBarCancelButton];
}

id<GREYMatcher> CloseButton() {
  return [ChromeMatchersAppInterface closeButton];
}

id<GREYMatcher> ForwardButton() {
  return [ChromeMatchersAppInterface forwardButton];
}

id<GREYMatcher> BackButton() {
  return [ChromeMatchersAppInterface backButton];
}

id<GREYMatcher> ReloadButton() {
  return [ChromeMatchersAppInterface reloadButton];
}

id<GREYMatcher> StopButton() {
  return [ChromeMatchersAppInterface stopButton];
}

id<GREYMatcher> Omnibox() {
  return [ChromeMatchersAppInterface omnibox];
}

id<GREYMatcher> OmniboxPopupRow() {
  return [ChromeMatchersAppInterface omniboxPopupRow];
}

id<GREYMatcher> OmniboxPopupList() {
  return [ChromeMatchersAppInterface omniboxPopupList];
}

id<GREYMatcher> DefocusedLocationView() {
  return [ChromeMatchersAppInterface defocusedLocationView];
}

id<GREYMatcher> PageSecurityInfoButton() {
  return [ChromeMatchersAppInterface pageSecurityInfoButton];
}

id<GREYMatcher> PageSecurityInfoIndicator() {
  return [ChromeMatchersAppInterface pageSecurityInfoIndicator];
}

id<GREYMatcher> OmniboxText(const std::string& text) {
  return [ChromeMatchersAppInterface omniboxText:base::SysUTF8ToNSString(text)];
}

id<GREYMatcher> OmniboxContainingText(const std::string& text) {
  return [ChromeMatchersAppInterface
      omniboxContainingText:base::SysUTF8ToNSString(text)];
}

id<GREYMatcher> OmniboxAutocompleteLabel() {
  return [ChromeMatchersAppInterface omniboxAutocompleteLabel];
}

id<GREYMatcher> LocationViewContainingText(const std::string& text) {
  return [ChromeMatchersAppInterface
      locationViewContainingText:base::SysUTF8ToNSString(text)];
}

id<GREYMatcher> ToolsMenuButton() {
  return [ChromeMatchersAppInterface toolsMenuButton];
}

id<GREYMatcher> NewTabButton() {
  return [ChromeMatchersAppInterface openNewTabButton];
}

id<GREYMatcher> ShareButton() {
  return [ChromeMatchersAppInterface shareButton];
}

id<GREYMatcher> TabShareButton() {
  return [ChromeMatchersAppInterface tabShareButton];
}

id<GREYMatcher> ShowTabsButton() {
  return [ChromeMatchersAppInterface showTabsButton];
}

id<GREYMatcher> AddToReadingListButton() {
  return [ChromeMatchersAppInterface addToReadingListButton];
}

id<GREYMatcher> AddToBookmarksButton() {
  return [ChromeMatchersAppInterface addToBookmarksButton];
}

id<GREYMatcher> CloseTabMenuButton() {
  return [ChromeMatchersAppInterface closeTabMenuButton];
}

id<GREYMatcher> TableViewSwitchCell(NSString* accessibility_identifier,
                                    BOOL is_toggled_on) {
  return
      [ChromeMatchersAppInterface tableViewSwitchCell:accessibility_identifier
                                          isToggledOn:is_toggled_on];
}

id<GREYMatcher> TableViewSwitchCell(NSString* accessibility_identifier,
                                    BOOL is_toggled_on,
                                    BOOL is_enabled) {
  return
      [ChromeMatchersAppInterface tableViewSwitchCell:accessibility_identifier
                                          isToggledOn:is_toggled_on
                                            isEnabled:is_enabled];
}

id<GREYMatcher> SyncSwitchCell(NSString* accessibility_label,
                               BOOL is_toggled_on) {
  return [ChromeMatchersAppInterface syncSwitchCell:accessibility_label
                                        isToggledOn:is_toggled_on];
}

id<GREYMatcher> OpenLinkInNewTabButton() {
  return [ChromeMatchersAppInterface openLinkInNewTabButton];
}

id<GREYMatcher> OpenLinkInIncognitoButton() {
  return [ChromeMatchersAppInterface openLinkInIncognitoButton];
}

id<GREYMatcher> OpenLinkInNewWindowButton() {
  return [ChromeMatchersAppInterface openLinkInNewWindowButton];
}

id<GREYMatcher> NavigationBarDoneButton() {
  return [ChromeMatchersAppInterface navigationBarDoneButton];
}

id<GREYMatcher> BookmarksNavigationBarDoneButton() {
  return [ChromeMatchersAppInterface bookmarksNavigationBarDoneButton];
}

id<GREYMatcher> BookmarksNavigationBarBackButton() {
  return [ChromeMatchersAppInterface bookmarksNavigationBarBackButton];
}

id<GREYMatcher> AddAccountButton() {
  return [ChromeMatchersAppInterface addAccountButton];
}

id<GREYMatcher> SignOutAccountsButton() {
  return [ChromeMatchersAppInterface signOutAccountsButton];
}

id<GREYMatcher> ClearBrowsingDataCell() {
  return [ChromeMatchersAppInterface clearBrowsingDataCell];
}

id<GREYMatcher> ClearBrowsingDataButton() {
  return [ChromeMatchersAppInterface clearBrowsingDataButton];
}

id<GREYMatcher> ClearBrowsingDataView() {
  return [ChromeMatchersAppInterface clearBrowsingDataView];
}

id<GREYMatcher> ConfirmClearBrowsingDataButton() {
  return [ChromeMatchersAppInterface confirmClearBrowsingDataButton];
}

id<GREYMatcher> SettingsDoneButton() {
  return [ChromeMatchersAppInterface settingsDoneButton];
}

id<GREYMatcher> AutofillCreditCardEditTableView() {
  return [ChromeMatchersAppInterface autofillCreditCardEditTableView];
}

id<GREYMatcher> AutofillCreditCardTableView() {
  return [ChromeMatchersAppInterface autofillCreditCardTableView];
}

id<GREYMatcher> AddressesAndMoreButton() {
  return [ChromeMatchersAppInterface addressesAndMoreButton];
}

id<GREYMatcher> PaymentMethodsButton() {
  return [ChromeMatchersAppInterface paymentMethodsButton];
}

id<GREYMatcher> LanguagesButton() {
  return [ChromeMatchersAppInterface languagesButton];
}

id<GREYMatcher> AddCreditCardView() {
  return [ChromeMatchersAppInterface addCreditCardView];
}

id<GREYMatcher> AddCreditCardButton() {
  return [ChromeMatchersAppInterface addCreditCardButton];
}

id<GREYMatcher> AddCreditCardCancelButton() {
  return [ChromeMatchersAppInterface addCreditCardCancelButton];
}

id<GREYMatcher> ToolsMenuView() {
  return [ChromeMatchersAppInterface toolsMenuView];
}

id<GREYMatcher> OKButton() {
  return [ChromeMatchersAppInterface OKButton];
}

id<GREYMatcher> PrimarySignInButton() {
  return [ChromeMatchersAppInterface primarySignInButton];
}

id<GREYMatcher> SecondarySignInButton() {
  return [ChromeMatchersAppInterface secondarySignInButton];
}

id<GREYMatcher> SettingsAccountButton() {
  return [ChromeMatchersAppInterface settingsAccountButton];
}

id<GREYMatcher> SettingsAccountsCollectionView() {
  return [ChromeMatchersAppInterface settingsAccountsCollectionView];
}

id<GREYMatcher> SettingsImportDataImportButton() {
  return [ChromeMatchersAppInterface settingsImportDataImportButton];
}

id<GREYMatcher> SettingsImportDataKeepSeparateButton() {
  return [ChromeMatchersAppInterface settingsImportDataKeepSeparateButton];
}

id<GREYMatcher> SettingsImportDataContinueButton() {
  return [ChromeMatchersAppInterface settingsImportDataContinueButton];
}

id<GREYMatcher> SettingsSafetyCheckTableView() {
  return [ChromeMatchersAppInterface settingsSafetyCheckTableView];
}

id<GREYMatcher> SettingsPrivacyTableView() {
  return [ChromeMatchersAppInterface settingsPrivacyTableView];
}

id<GREYMatcher> SettingsPrivacySafeBrowsingTableView() {
  return [ChromeMatchersAppInterface settingsPrivacySafeBrowsingTableView];
}

id<GREYMatcher> SettingsNotificationsTableView() {
  return [ChromeMatchersAppInterface settingsNotificationsTableView];
}

id<GREYMatcher> SettingsTrackingPriceTableView() {
  return [ChromeMatchersAppInterface settingsTrackingPriceTableView];
}

id<GREYMatcher> ContentSettingsButton() {
  return [ChromeMatchersAppInterface contentSettingsButton];
}

id<GREYMatcher> GoogleServicesSettingsButton() {
  return [ChromeMatchersAppInterface googleServicesSettingsButton];
}

id<GREYMatcher> ManageSyncSettingsButton() {
  return [ChromeMatchersAppInterface manageSyncSettingsButton];
}

id<GREYMatcher> GoogleServicesSettingsView() {
  return [ChromeMatchersAppInterface googleServicesSettingsView];
}

id<GREYMatcher> SettingsMenuBackButton() {
  return [ChromeMatchersAppInterface settingsMenuBackButton];
}

id<GREYMatcher> SettingsMenuBackButton(int window_number) {
  return [ChromeMatchersAppInterface
      settingsMenuBackButtonInWindowWithNumber:window_number];
}

id<GREYMatcher> SettingsMenuPrivacyButton() {
  return [ChromeMatchersAppInterface settingsMenuPrivacyButton];
}

id<GREYMatcher> SettingsMenuNotificationsButton() {
  return [ChromeMatchersAppInterface settingsMenuNotificationsButton];
}

id<GREYMatcher> SettingsMenuPasswordsButton() {
  return [ChromeMatchersAppInterface settingsMenuPasswordsButton];
}

id<GREYMatcher> PaymentRequestView() {
  return [ChromeMatchersAppInterface paymentRequestView];
}

id<GREYMatcher> PaymentRequestErrorView() {
  return [ChromeMatchersAppInterface paymentRequestErrorView];
}

id<GREYMatcher> VoiceSearchButton() {
  return [ChromeMatchersAppInterface voiceSearchButton];
}

id<GREYMatcher> VoiceSearchInputAccessoryButton() {
  return [ChromeMatchersAppInterface voiceSearchInputAccessoryButton];
}

id<GREYMatcher> SettingsCollectionView() {
  return [ChromeMatchersAppInterface settingsCollectionView];
}

id<GREYMatcher> ClearBrowsingHistoryButton() {
  return [ChromeMatchersAppInterface clearBrowsingHistoryButton];
}

id<GREYMatcher> HistoryTableView() {
  return [ChromeMatchersAppInterface historyTableView];
}

id<GREYMatcher> ClearCookiesButton() {
  return [ChromeMatchersAppInterface clearCookiesButton];
}

id<GREYMatcher> ClearCacheButton() {
  return [ChromeMatchersAppInterface clearCacheButton];
}

id<GREYMatcher> ClearSavedPasswordsButton() {
  return [ChromeMatchersAppInterface clearSavedPasswordsButton];
}

id<GREYMatcher> ClearAutofillButton() {
  return [ChromeMatchersAppInterface clearAutofillButton];
}

id<GREYMatcher> ContentSuggestionsCollectionView() {
  return [ChromeMatchersAppInterface contentSuggestionCollectionView];
}

id<GREYMatcher> NTPCollectionView() {
  return [ChromeMatchersAppInterface ntpCollectionView];
}

id<GREYMatcher> NTPIncognitoView() {
  return [ChromeMatchersAppInterface ntpIncognitoView];
}

id<GREYMatcher> NTPFeedMenuEnableButton() {
  return [ChromeMatchersAppInterface ntpFeedMenuEnableButton];
}

id<GREYMatcher> NTPFeedMenuDisableButton() {
  return [ChromeMatchersAppInterface ntpFeedMenuDisableButton];
}

id<GREYMatcher> WarningMessageView() {
  return [ChromeMatchersAppInterface warningMessageView];
}

id<GREYMatcher> PaymentRequestPickerRow() {
  return [ChromeMatchersAppInterface paymentRequestPickerRow];
}

id<GREYMatcher> PaymentRequestPickerSearchBar() {
  return [ChromeMatchersAppInterface paymentRequestPickerSearchBar];
}

id<GREYMatcher> OpenNewWindowMenuButton() {
  return [ChromeMatchersAppInterface openNewWindowMenuButton];
}

id<GREYMatcher> SystemSelectionCallout() {
  return [EditMenuAppInterface editMenuButtonMatcher];
}

id<GREYMatcher> SystemSelectionCalloutLinkToTextButton() {
  return [EditMenuAppInterface editMenuLinkToTextButtonMatcher];
}

id<GREYMatcher> SystemSelectionCalloutCopyButton() {
  return [EditMenuAppInterface editMenuCopyButtonMatcher];
}

id<GREYMatcher> SystemSelectionCalloutCutButton() {
  return [EditMenuAppInterface editMenuCutButtonMatcher];
}

id<GREYMatcher> SystemSelectionCalloutPasteButton() {
  return [EditMenuAppInterface editMenuPasteButtonMatcher];
}

id<GREYMatcher> SystemSelectionCalloutOverflowButton() {
  return [EditMenuAppInterface editMenuNextButtonMatcher];
}

id<GREYMatcher> CopyActivityButton() {
  return [ChromeMatchersAppInterface copyActivityButton];
}

id<GREYMatcher> CopyLinkButton() {
  return [ChromeMatchersAppInterface copyLinkButton];
}

id<GREYMatcher> EditButton() {
  return [ChromeMatchersAppInterface editButton];
}

id<GREYMatcher> MoveButton() {
  return [ChromeMatchersAppInterface moveButton];
}

id<GREYMatcher> ReadingListMarkAsReadButton() {
  return [ChromeMatchersAppInterface readingListMarkAsReadButton];
}

id<GREYMatcher> ReadingListMarkAsUnreadButton() {
  return [ChromeMatchersAppInterface readingListMarkAsUnreadButton];
}

id<GREYMatcher> DeleteButton() {
  return [ChromeMatchersAppInterface deleteButton];
}

id<GREYMatcher> ContextMenuCopyButton() {
  return [ChromeMatchersAppInterface contextMenuCopyButton];
}

id<GREYMatcher> NewTabPageOmnibox() {
  return [ChromeMatchersAppInterface NTPOmnibox];
}

id<GREYMatcher> FakeOmnibox() {
  return [ChromeMatchersAppInterface fakeOmnibox];
}

id<GREYMatcher> DiscoverHeaderLabel() {
  return [ChromeMatchersAppInterface discoverHeaderLabel];
}

id<GREYMatcher> NTPLogo() {
  return [ChromeMatchersAppInterface ntpLogo];
}

id<GREYMatcher> WebViewMatcher() {
  return [ChromeMatchersAppInterface webViewMatcher];
}

id<GREYMatcher> WebStateScrollViewMatcher() {
  return [ChromeMatchersAppInterface webStateScrollViewMatcher];
}

id<GREYMatcher> WebStateScrollViewMatcherInWindowWithNumber(int windowNumber) {
  return [ChromeMatchersAppInterface
      webStateScrollViewMatcherInWindowWithNumber:windowNumber];
}

id<GREYMatcher> HistoryClearBrowsingDataButton() {
  return [ChromeMatchersAppInterface historyClearBrowsingDataButton];
}

id<GREYMatcher> OpenInButton() {
  return [ChromeMatchersAppInterface openInButton];
}

id<GREYMatcher> TabGridCellAtIndex(unsigned int index) {
  return [ChromeMatchersAppInterface tabGridCellAtIndex:index];
}

id<GREYMatcher> TabGridDoneButton() {
  return [ChromeMatchersAppInterface tabGridDoneButton];
}

id<GREYMatcher> TabGridCloseAllButton() {
  return [ChromeMatchersAppInterface tabGridCloseAllButton];
}

id<GREYMatcher> TabGridUndoCloseAllButton() {
  return [ChromeMatchersAppInterface tabGridUndoCloseAllButton];
}

id<GREYMatcher> TabGridSelectShowHistoryCell() {
  return [ChromeMatchersAppInterface tabGridSelectShowHistoryCell];
}

id<GREYMatcher> TabGridRegularTabsEmptyStateView() {
  return [ChromeMatchersAppInterface tabGridRegularTabsEmptyStateView];
}

id<GREYMatcher> TabGridNewTabButton() {
  return [ChromeMatchersAppInterface tabGridNewTabButton];
}

id<GREYMatcher> TabGridNewIncognitoTabButton() {
  return [ChromeMatchersAppInterface tabGridNewIncognitoTabButton];
}

id<GREYMatcher> TabGridOpenTabsPanelButton() {
  return [ChromeMatchersAppInterface tabGridOpenTabsPanelButton];
}

id<GREYMatcher> TabGridIncognitoTabsPanelButton() {
  return [ChromeMatchersAppInterface tabGridIncognitoTabsPanelButton];
}

id<GREYMatcher> TabGridOtherDevicesPanelButton() {
  return [ChromeMatchersAppInterface tabGridOtherDevicesPanelButton];
}

id<GREYMatcher> TabGridNormalModePageControl() {
  return [ChromeMatchersAppInterface tabGridNormalModePageControl];
}

id<GREYMatcher> TabGridBackground() {
  return [ChromeMatchersAppInterface tabGridBackground];
}

id<GREYMatcher> RegularTabGrid() {
  return [ChromeMatchersAppInterface regularTabGrid];
}

id<GREYMatcher> IncognitoTabGrid() {
  return [ChromeMatchersAppInterface incognitoTabGrid];
}

id<GREYMatcher> TabGridCloseButtonForCellAtIndex(unsigned int index) {
  return [ChromeMatchersAppInterface tabGridCloseButtonForCellAtIndex:index];
}

id<GREYMatcher> SettingsPasswordMatcher() {
  return [ChromeMatchersAppInterface settingsPasswordMatcher];
}

id<GREYMatcher> SettingsPasswordSearchMatcher() {
  return [ChromeMatchersAppInterface settingsPasswordSearchMatcher];
}

id<GREYMatcher> SettingsProfileMatcher() {
  return [ChromeMatchersAppInterface settingsProfileMatcher];
}

id<GREYMatcher> SettingsCreditCardMatcher() {
  return [ChromeMatchersAppInterface settingsCreditCardMatcher];
}

id<GREYMatcher> SettingsBottomToolbarDeleteButton() {
  return [ChromeMatchersAppInterface settingsBottomToolbarDeleteButton];
}

id<GREYMatcher> SettingsSearchEngineButton() {
  return [ChromeMatchersAppInterface settingsSearchEngineButton];
}

id<GREYMatcher> AutofillSuggestionViewMatcher() {
  return [ChromeMatchersAppInterface autofillSuggestionViewMatcher];
}

id<GREYMatcher> ContentViewSmallerThanScrollView() {
  return [ChromeMatchersAppInterface contentViewSmallerThanScrollView];
}

id<GREYMatcher> HistoryEntry(const std::string& url, const std::string& title) {
  return [ChromeMatchersAppInterface
      historyEntryForURL:base::SysUTF8ToNSString(url)
                   title:base::SysUTF8ToNSString(title)];
}

id<GREYMatcher> SettingsToolbarAddButton() {
  return [ChromeMatchersAppInterface settingsToolbarAddButton];
}

id<GREYMatcher> CellCanBeSwipedToDismissed() {
  return [ChromeMatchersAppInterface cellCanBeSwipedToDismissed];
}

#pragma mark - Overflow Menu Destinations

id<GREYMatcher> BookmarksDestinationButton() {
  return [ChromeMatchersAppInterface bookmarksDestinationButton];
}

id<GREYMatcher> HistoryDestinationButton() {
  return [ChromeMatchersAppInterface historyDestinationButton];
}

id<GREYMatcher> ReadingListDestinationButton() {
  return [ChromeMatchersAppInterface readingListDestinationButton];
}

id<GREYMatcher> PasswordsDestinationButton() {
  return [ChromeMatchersAppInterface passwordsDestinationButton];
}

id<GREYMatcher> PriceNotificationsDestinationButton() {
  return [ChromeMatchersAppInterface priceNotificationsDestinationButton];
}

id<GREYMatcher> DownloadsDestinationButton() {
  return [ChromeMatchersAppInterface downloadsDestinationButton];
}

id<GREYMatcher> RecentTabsDestinationButton() {
  return [ChromeMatchersAppInterface recentTabsDestinationButton];
}

id<GREYMatcher> SiteInfoDestinationButton() {
  return [ChromeMatchersAppInterface siteInfoDestinationButton];
}

id<GREYMatcher> SettingsDestinationButton() {
  return [ChromeMatchersAppInterface settingsDestinationButton];
}

id<GREYMatcher> WhatsNewDestinationButton() {
  return [ChromeMatchersAppInterface whatsNewDestinationButton];
}

#pragma mark - Overflow Menu Actions

id<GREYMatcher> SettingsActionButton() {
  return [ChromeMatchersAppInterface settingsActionButton];
}

#pragma mark - Promo style view controller

id<GREYMatcher> PromoStylePrimaryActionButtonMatcher() {
  return [ChromeMatchersAppInterface promoStylePrimaryActionButtonMatcher];
}

id<GREYMatcher> PromoStyleSecondaryActionButtonMatcher() {
  return [ChromeMatchersAppInterface promoStyleSecondaryActionButtonMatcher];
}

#pragma mark - Incognito Interstitial

id<GREYMatcher> IncognitoInterstitialMatcher() {
  return [ChromeMatchersAppInterface incognitoInterstitial];
}

id<GREYMatcher> IncognitoInterstitialLabelForURL(const std::string& url) {
  return [ChromeMatchersAppInterface
      incognitoInterstitialLabelForURL:base::SysUTF8ToNSString(url)];
}

id<GREYMatcher> IncognitoInterstitialOpenInChromeIncognitoButton() {
  return [ChromeMatchersAppInterface
      incognitoInterstitialOpenInChromeIncognitoButton];
}

id<GREYMatcher> IncognitoInterstitialOpenInChromeButton() {
  return [ChromeMatchersAppInterface incognitoInterstitialOpenInChromeButton];
}

id<GREYMatcher> IncognitoInterstitialCancelButton() {
  return [ChromeMatchersAppInterface incognitoInterstitialCancelButton];
}

#pragma mark - Manual Fallback

id<GREYMatcher> ManualFallbackFormSuggestionViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackFormSuggestionViewMatcher];
}

id<GREYMatcher> ManualFallbackKeyboardIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackKeyboardIconMatcher];
}

id<GREYMatcher> ManualFallbackPasswordIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordIconMatcher];
}

id<GREYMatcher> ManualFallbackPasswordTableViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordTableViewMatcher];
}

id<GREYMatcher> ManualFallbackPasswordSearchBarMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordSearchBarMatcher];
}

id<GREYMatcher> ManualFallbackManagePasswordsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackManagePasswordsMatcher];
}

id<GREYMatcher> ManualFallbackManageSettingsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackManageSettingsMatcher];
}

id<GREYMatcher> ManualFallbackOtherPasswordsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackOtherPasswordsMatcher];
}

id<GREYMatcher> ManualFallbackOtherPasswordsDismissMatcher() {
  return
      [ChromeMatchersAppInterface manualFallbackOtherPasswordsDismissMatcher];
}

id<GREYMatcher> ManualFallbackPasswordButtonMatcher() {
  return [ChromeMatchersAppInterface manualFallbackPasswordButtonMatcher];
}

id<GREYMatcher> ManualFallbackPasswordTableViewWindowMatcher() {
  return
      [ChromeMatchersAppInterface manualFallbackPasswordTableViewWindowMatcher];
}

id<GREYMatcher> ManualFallbackProfilesIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackProfilesIconMatcher];
}

id<GREYMatcher> ManualFallbackProfilesTableViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackProfilesTableViewMatcher];
}

id<GREYMatcher> ManualFallbackManageProfilesMatcher() {
  return [ChromeMatchersAppInterface manualFallbackManageProfilesMatcher];
}

id<GREYMatcher> ManualFallbackProfileTableViewWindowMatcher() {
  return
      [ChromeMatchersAppInterface manualFallbackProfileTableViewWindowMatcher];
}

id<GREYMatcher> ManualFallbackCreditCardIconMatcher() {
  return [ChromeMatchersAppInterface manualFallbackCreditCardIconMatcher];
}

id<GREYMatcher> ManualFallbackCreditCardTableViewMatcher() {
  return [ChromeMatchersAppInterface manualFallbackCreditCardTableViewMatcher];
}

// Returns a matcher for the button to open password settings in manual
id<GREYMatcher> ManualFallbackManageCreditCardsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackManageCreditCardsMatcher];
}

// Returns a matcher for the button to add credit cards settings in manual
id<GREYMatcher> ManualFallbackAddCreditCardsMatcher() {
  return [ChromeMatchersAppInterface manualFallbackAddCreditCardsMatcher];
}

id<GREYMatcher> ManualFallbackCreditCardTableViewWindowMatcher() {
  return [ChromeMatchersAppInterface
      manualFallbackCreditCardTableViewWindowMatcher];
}

id<GREYMatcher> ActivityViewHeader(NSString* url_host, NSString* page_title) {
  return [ChromeMatchersAppInterface activityViewHeaderWithURLHost:url_host
                                                             title:page_title];
}

id<GREYMatcher> ManualFallbackSuggestPasswordMatcher() {
  return [ChromeMatchersAppInterface manualFallbackSuggestPasswordMatcher];
}

id<GREYMatcher> UseSuggestedPasswordMatcher() {
  return [ChromeMatchersAppInterface useSuggestedPasswordMatcher];
}

#pragma mark - Tab Grid Edit Mode

id<GREYMatcher> TabGridEditButton() {
  return [ChromeMatchersAppInterface tabGridEditButton];
}

id<GREYMatcher> TabGridEditMenuCloseAllButton() {
  return [ChromeMatchersAppInterface tabGridEditMenuCloseAllButton];
}

id<GREYMatcher> TabGridSelectTabsMenuButton() {
  return [ChromeMatchersAppInterface tabGridSelectTabsMenuButton];
}

id<GREYMatcher> TabGridEditAddToButton() {
  return [ChromeMatchersAppInterface tabGridEditAddToButton];
}

id<GREYMatcher> TabGridEditCloseTabsButton() {
  return [ChromeMatchersAppInterface tabGridEditCloseTabsButton];
}

id<GREYMatcher> TabGridEditSelectAllButton() {
  return [ChromeMatchersAppInterface tabGridEditSelectAllButton];
}

id<GREYMatcher> TabGridEditShareButton() {
  return [ChromeMatchersAppInterface tabGridEditShareButton];
}

#pragma mark - Tab Grid Search Mode

id<GREYMatcher> TabGridSearchTabsButton() {
  return [ChromeMatchersAppInterface tabGridSearchTabsButton];
}

id<GREYMatcher> TabGridSearchBar() {
  return [ChromeMatchersAppInterface tabGridSearchBar];
}

id<GREYMatcher> TabGridSearchCancelButton() {
  return [ChromeMatchersAppInterface tabGridSearchCancelButton];
}

id<GREYMatcher> TabGridSearchModeToolbar() {
  return [ChromeMatchersAppInterface tabGridSearchModeToolbar];
}

}  // namespace chrome_test_util
