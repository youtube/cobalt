// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"

#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/webui/web_ui_util.h"

void RegisterNearbySharedStrings(content::WebUIDataSource* data_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"nearbyShareAccountRowLabel", IDS_NEARBY_ACCOUNT_ROW_LABEL},
      {"nearbyShareActionsAccept", IDS_NEARBY_ACTIONS_ACCEPT},
      {"nearbyShareActionsCancel", IDS_NEARBY_ACTIONS_CANCEL},
      {"nearbyShareActionsClose", IDS_NEARBY_ACTIONS_CLOSE},
      {"nearbyShareActionsConfirm", IDS_NEARBY_ACTIONS_CONFIRM},
      {"nearbyShareActionsDecline", IDS_NEARBY_ACTIONS_DECLINE},
      {"nearbyShareActionsNext", IDS_NEARBY_ACTIONS_NEXT},
      {"nearbyShareActionsReject", IDS_NEARBY_ACTIONS_REJECT},
      {"nearbyShareConfirmationPageAddContactSubtitle",
       IDS_NEARBY_CONFIRMATION_PAGE_ADD_CONTACT_SUBTITLE},
      {"nearbyShareConfirmationPageAddContactTitle",
       IDS_NEARBY_CONFIRMATION_PAGE_ADD_CONTACT_TITLE},
      {"nearbyShareConfirmationPageTitle", IDS_NEARBY_CONFIRMATION_PAGE_TITLE},
      {"nearbyShareContactVisibilityAll", IDS_NEARBY_VISIBLITY_ALL_CONTACTS},
      {"nearbyShareContactVisibilityAllDescription",
       IDS_NEARBY_VISIBLITY_ALL_CONTACTS_DESCRIPTION},
      {"nearbyShareContactVisibilityDownloadFailed",
       IDS_NEARBY_CONTACT_VISIBILITY_DOWNLOAD_FAILED},
      {"nearbyShareContactVisibilityDownloading",
       IDS_NEARBY_CONTACT_VISIBILITY_DOWNLOADING},
      {"nearbyShareContactVisibilityNoContactsSubtitle",
       IDS_NEARBY_CONTACT_VISIBILITY_NO_CONTACTS_SUBTITLE},
      {"nearbyShareContactVisibilityNoContactsTitle",
       IDS_NEARBY_CONTACT_VISIBILITY_NO_CONTACTS_TITLE},
      {"nearbyShareContactVisibilityNone", IDS_NEARBY_VISIBLITY_HIDDEN},
      {"nearbyShareContactVisibilityNoneDescription",
       IDS_NEARBY_VISIBLITY_HIDDEN_DESCRIPTION},
      {"nearbyShareContactVisibilityOwnAll",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_ALL},
      {"nearbyShareContactVisibilityOwnNone",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_NONE},
      {"nearbyShareContactVisibilityOwnSome",
       IDS_NEARBY_CONTACT_VISIBILITY_OWN_SOME},
      {"nearbyShareContactVisibilitySome", IDS_NEARBY_VISIBLITY_SOME_CONTACTS},
      {"nearbyShareContactVisibilitySomeDescription",
       IDS_NEARBY_VISIBLITY_SOME_CONTACTS_DESCRIPTION},
      {"nearbyShareContactVisibilityUnknown", IDS_NEARBY_VISIBLITY_UNKNOWN},
      {"nearbyShareContactVisibilityUnknownDescription",
       IDS_NEARBY_VISIBLITY_UNKNOWN_DESCRIPTION},
      {"nearbyShareContactVisibilityZeroStateText",
       IDS_NEARBY_CONTACT_VISIBILITY_ZERO_STATE_TEXT},
      {"nearbyShareDeviceNameEmptyError", IDS_NEARBY_DEVICE_NAME_EMPTY_ERROR},
      {"nearbyShareDeviceNameTooLongError",
       IDS_NEARBY_DEVICE_NAME_TOO_LONG_ERROR},
      {"nearbyShareDeviceNameInvalidCharactersError",
       IDS_NEARBY_DEVICE_NAME_INVALID_CHARACTERS_ERROR},
      {"nearbyShareDiscoveryPageInfo", IDS_NEARBY_DISCOVERY_PAGE_INFO},
      {"nearbyShareDiscoveryPagePlaceholder",
       IDS_NEARBY_DISCOVERY_PAGE_PLACEHOLDER},
      {"nearbyShareDiscoveryPageSubtitle", IDS_NEARBY_DISCOVERY_PAGE_SUBTITLE},
      {"nearbyShareDiscoveryPageTitle", IDS_NEARBY_DISCOVERY_PAGE_TITLE},
      {"nearbyShareErrorCancelled", IDS_NEARBY_ERROR_CANCELLED},
      {"nearbyShareErrorCantReceive", IDS_NEARBY_ERROR_CANT_RECEIVE},
      {"nearbyShareErrorCantShare", IDS_NEARBY_ERROR_CANT_SHARE},
      {"nearbyShareErrorNoResponse", IDS_NEARBY_ERROR_NO_RESPONSE},
      {"nearbyShareErrorNotEnoughSpace", IDS_NEARBY_ERROR_NOT_ENOUGH_SPACE},
      {"nearbyShareErrorTransferInProgress",
       IDS_NEARBY_ERROR_TRANSFER_IN_PROGRESS},
      {"nearbyShareErrorRejected", IDS_NEARBY_ERROR_REJECTED},
      {"nearbyShareErrorSomethingWrong", IDS_NEARBY_ERROR_SOMETHING_WRONG},
      {"nearbyShareErrorTimeOut", IDS_NEARBY_ERROR_TIME_OUT},
      {"nearbyShareErrorTryAgain", IDS_NEARBY_ERROR_TRY_AGAIN},
      {"nearbyShareErrorUnsupportedFileType",
       IDS_NEARBY_ERROR_UNSUPPORTED_FILE_TYPE},
      {"nearbyShareFeatureName", IDS_NEARBY_SHARE_FEATURE_NAME},
      {"nearbyShareOnboardingPageDeviceName",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_NAME},
      {"nearbyShareOnboardingPageDeviceNameHelp",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_NAME_HELP},
      {"nearbyShareOnboardingPageDeviceVisibility",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_VISIBILITY},
      {"nearbyShareOnboardingPageDeviceVisibilityHelpAllContacts",
       IDS_NEARBY_ONBOARDING_PAGE_DEVICE_VISIBILITY_HELP_ALL_CONTACTS},
      {"nearbyShareOnboardingPageSubtitle",
       IDS_NEARBY_ONBOARDING_PAGE_SUBTITLE},
      {"nearbyShareOnboardingPageTitle", IDS_NEARBY_ONBOARDING_PAGE_TITLE},
      {"nearbySharePreviewMultipleFileTitle",
       IDS_NEARBY_PREVIEW_TITLE_MULTIPLE_FILE},
      {"nearbyShareSecureConnectionId", IDS_NEARBY_SECURE_CONNECTION_ID},
      {"nearbyShareSettingsHelpCaption", IDS_NEARBY_SETTINGS_HELP_CAPTION},
      {"nearbyShareVisibilityPageManageContacts",
       IDS_NEARBY_VISIBILITY_PAGE_MANAGE_CONTACTS},
      {"nearbyShareVisibilityPageSubtitle",
       IDS_NEARBY_VISIBILITY_PAGE_SUBTITLE},
      {"nearbyShareVisibilityPageTitle", IDS_NEARBY_VISIBILITY_PAGE_TITLE},
      {"nearbyShareHighVisibilitySubTitle",
       IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE},
      {"nearbyShareHighVisibilitySubTitleMinutes",
       IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE_MINUTES},
      {"nearbyShareHighVisibilitySubTitleSeconds",
       IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE_SECONDS},
      {"nearbyShareHighVisibilityHelpText",
       IDS_NEARBY_HIGH_VISIBILITY_HELP_TEXT},
      {"nearbyShareHighVisibilityTimeoutText",
       IDS_NEARBY_HIGH_VISIBILITY_TIMEOUT_TEXT},
      {"nearbyShareReceiveConfirmPageTitle",
       IDS_NEARBY_RECEIVE_CONFIRM_PAGE_TITLE},
      {"nearbyShareReceiveConfirmPageConnectionId",
       IDS_NEARBY_RECEIVE_CONFIRM_PAGE_CONNECTION_ID},
      {"nearbyShareErrorNoConnectionMedium",
       IDS_NEARBY_HIGH_VISIBILITY_CONNECTION_MEDIUM_ERROR},
      {"nearbyShareErrorNoConnectionMediumDescription",
       IDS_NEARBY_HIGH_VISIBILITY_CONNECTION_MEDIUM_DESCRIPTION},
      {"nearbyShareErrorTransferInProgressTitle",
       IDS_NEARBY_HIGH_VISIBILITY_TRANSFER_IN_PROGRESS_ERROR},
      {"nearbyShareErrorTransferInProgressDescription",
       IDS_NEARBY_HIGH_VISIBILITY_TRANSFER_IN_PROGRESS_DESCRIPTION}};
  data_source->AddLocalizedStrings(kLocalizedStrings);

  data_source->AddString("nearbyShareLearnMoreLink",
                         base::ASCIIToUTF16(chrome::kNearbyShareLearnMoreURL));

  data_source->AddString(
      "nearbyShareManageContactsUrl",
      base::ASCIIToUTF16(chrome::kNearbyShareManageContactsURL));
}
