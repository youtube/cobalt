// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/files_section.h"

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_shares_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kFilesSectionPath;
using ::chromeos::settings::mojom::kGoogleDriveSubpagePath;
using ::chromeos::settings::mojom::kNetworkFileSharesSubpagePath;
using ::chromeos::settings::mojom::kOfficeFilesSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetFilesSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_FILES,
       mojom::kFilesSectionPath,
       mojom::SearchResultIcon::kFolder,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kFiles}},
      {IDS_OS_SETTINGS_TAG_FILES_DISCONNECT_GOOGLE_DRIVE,
       mojom::kFilesSectionPath,
       mojom::SearchResultIcon::kDrive,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kGoogleDriveConnection}},
      {IDS_OS_SETTINGS_TAG_FILES_NETWORK_FILE_SHARES,
       mojom::kNetworkFileSharesSubpagePath,
       mojom::SearchResultIcon::kFolder,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kNetworkFileShares},
       {IDS_OS_SETTINGS_TAG_FILES_NETWORK_FILE_SHARES_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetFilesOfficeSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_FILES_OFFICE,
        mojom::kOfficeFilesSubpagePath,
        mojom::SearchResultIcon::kFolder,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kOfficeFiles}}});
  return *tags;
}

const std::vector<SearchConcept>& GetFilesGoogleDriveSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_FILES_GOOGLE_DRIVE,
        mojom::kGoogleDriveSubpagePath,
        mojom::SearchResultIcon::kFolder,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kGoogleDrive}}});
  return *tags;
}

}  // namespace

FilesSection::FilesSection(Profile* profile,
                           SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetFilesSearchConcepts());
  if (cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud()) {
    updater.AddSearchTags(GetFilesOfficeSearchConcepts());
  }
  if (ash::features::IsDriveFsBulkPinningEnabled()) {
    updater.AddSearchTags(GetFilesGoogleDriveSearchConcepts());
  }
}

FilesSection::~FilesSection() = default;

void FilesSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"disconnectGoogleDriveAccount", IDS_SETTINGS_DISCONNECT_GOOGLE_DRIVE},
      {"googleDriveLabel", IDS_SETTINGS_GOOGLE_DRIVE},
      {"googleDriveEnabledLabel", IDS_SETTINGS_GOOGLE_DRIVE_ENABLED},
      {"googleDriveDisabledLabel", IDS_SETTINGS_GOOGLE_DRIVE_DISABLED},
      {"googleDriveDisconnectLabel", IDS_SETTINGS_GOOGLE_DRIVE_DISCONNECT},
      {"googleDriveConnectLabel", IDS_SETTINGS_GOOGLE_DRIVE_CONNECT},
      {"googleDriveOfflineTitle", IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_TITLE},
      {"googleDriveOfflineSubtitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_SUBTITLE},
      {"googleDriveOfflineStorageTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_STORAGE_TITLE},
      {"googleDriveOfflineSpaceSubtitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_STORAGE_REQUIRED_SUBTITLE},
      {"googleDriveOfflineClearCalculatingSubtitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAR_CALCULATING_SUBTITLE},
      {"googleDriveOfflineClearErrorSubtitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAR_ERROR_SUBTITLE},
      {"googleDriveOfflineClearAction",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAR_ACTION},
      {"googleDriveOfflineClearDialogTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAR_DIALOG_TITLE},
      {"googleDriveOfflineClearDialogBody",
       IDS_SETTINGS_GOOGLE_DRIVE_OFFLINE_CLEAR_DIALOG_BODY},
      {"googleDriveTurnOffLabel",
       IDS_SETTINGS_GOOGLE_DRIVE_TURN_OFF_BUTTON_LABEL},
      {"googleDriveTurnOffTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_TURN_OFF_TITLE_TEXT},
      {"googleDriveNotEnoughSpaceTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_BULK_PINNING_NOT_ENOUGH_SPACE_TITLE_TEXT},
      {"googleDriveNotEnoughSpaceBody",
       IDS_SETTINGS_GOOGLE_DRIVE_BULK_PINNING_NOT_ENOUGH_SPACE_BODY_TEXT},
      {"googleDriveUnexpectedErrorTitle",
       IDS_SETTINGS_GOOGLE_DRIVE_BULK_PINNING_UNEXPECTED_ERROR_TITLE_TEXT},
      {"googleDriveUnexpectedErrorBody",
       IDS_SETTINGS_GOOGLE_DRIVE_BULK_PINNING_UNEXPECTED_ERROR_BODY_TEXT},
      {"googleDriveTurnOffBody", IDS_SETTINGS_GOOGLE_DRIVE_TURN_OFF_BODY_TEXT},
      {"filesPageTitle", IDS_OS_SETTINGS_FILES},
      {"smbSharesTitle", IDS_SETTINGS_DOWNLOADS_SMB_SHARES},
      {"smbSharesLearnMoreLabel",
       IDS_SETTINGS_DOWNLOADS_SMB_SHARES_LEARN_MORE_LABEL},
      {"addSmbShare", IDS_SETTINGS_DOWNLOADS_SMB_SHARES_ADD_SHARE},
      {"smbShareAddedSuccessfulMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_SUCCESS_MESSAGE},
      {"smbShareAddedErrorMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_ERROR_MESSAGE},
      {"smbShareAddedAuthFailedMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_AUTH_FAILED_MESSAGE},
      {"smbShareAddedNotFoundMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_NOT_FOUND_MESSAGE},
      {"smbShareAddedUnsupportedDeviceMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_UNSUPPORTED_DEVICE_MESSAGE},
      {"smbShareAddedMountExistsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_EXISTS_MESSAGE},
      {"smbShareAddedTooManyMountsMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_TOO_MANY_MOUNTS_MESSAGE},
      {"smbShareAddedInvalidURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_URL_MESSAGE},
      {"smbShareAddedInvalidSSOURLMessage",
       IDS_SETTINGS_DOWNLOADS_SHARE_ADDED_MOUNT_INVALID_SSO_URL_MESSAGE},
      {"officeLabel", IDS_SETTINGS_OFFICE_LABEL},
      {"officeSubpageTitle", IDS_SETTINGS_OFFICE_SUBPAGE_TITLE},
      {"alwaysMoveToDrivePreferenceLabel",
       IDS_SETTINGS_ALWAYS_MOVE_OFFICE_TO_DRIVE_PREFERENCE_LABEL},
      {"alwaysMoveToOneDrivePreferenceLabel",
       IDS_SETTINGS_ALWAYS_MOVE_OFFICE_TO_ONEDRIVE_PREFERENCE_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  smb_dialog::AddLocalizedStrings(html_source);

  html_source->AddString("smbSharesLearnMoreURL",
                         GetHelpUrlWithBoard(chrome::kSmbSharesLearnMoreURL));

  html_source->AddBoolean(
      "showOfficeSettings",
      cloud_upload::IsEligibleAndEnabledUploadOfficeToCloud());

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile());
  html_source->AddBoolean("isActiveDirectoryUser",
                          user && user->IsActiveDirectoryUser());

  if (user && user->GetAccountId().is_valid()) {
    html_source->AddString(
        "googleDriveSignedInAs",
        l10n_util::GetStringFUTF16(
            IDS_SETTINGS_GOOGLE_DRIVE_SIGNED_IN_AS,
            base::ASCIIToUTF16(user->GetAccountId().GetUserEmail())));
    html_source->AddString(
        "googleDriveDisconnectedFrom",
        l10n_util::GetStringFUTF16(
            IDS_SETTINGS_GOOGLE_DRIVE_DISCONNECTED_FROM,
            base::ASCIIToUTF16(user->GetAccountId().GetUserEmail())));
  }

  html_source->AddBoolean("enableDriveFsBulkPinning",
                          features::IsDriveFsBulkPinningEnabled());
}

void FilesSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<smb_dialog::SmbHandler>(profile(), base::DoNothing()));
}

int FilesSection::GetSectionNameMessageId() const {
  return IDS_OS_SETTINGS_FILES;
}

mojom::Section FilesSection::GetSection() const {
  return mojom::Section::kFiles;
}

mojom::SearchResultIcon FilesSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kFolder;
}

std::string FilesSection::GetSectionPath() const {
  return mojom::kFilesSectionPath;
}

bool FilesSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  // Unimplemented.
  return false;
}

void FilesSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kGoogleDriveConnection);

  // Network file shares.
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_DOWNLOADS_SMB_SHARES, mojom::Subpage::kNetworkFileShares,
      mojom::SearchResultIcon::kFolder, mojom::SearchResultDefaultRank::kMedium,
      mojom::kNetworkFileSharesSubpagePath);

  // Office.
  // TODO(b:264314789): Correct string (not smb).
  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_DOWNLOADS_SMB_SHARES, mojom::Subpage::kOfficeFiles,
      mojom::SearchResultIcon::kFolder, mojom::SearchResultDefaultRank::kMedium,
      mojom::kNetworkFileSharesSubpagePath);

  generator->RegisterTopLevelSubpage(
      IDS_SETTINGS_GOOGLE_DRIVE, mojom::Subpage::kGoogleDrive,
      mojom::SearchResultIcon::kFolder, mojom::SearchResultDefaultRank::kMedium,
      mojom::kGoogleDriveSubpagePath);
}

}  // namespace ash::settings
