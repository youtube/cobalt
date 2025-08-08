// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_misc.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/webui/settings/public/constants/routes_util.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_model.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/zoom/page_zoom.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace extensions {
namespace {

using api::file_manager_private::ProfileInfo;

// Thresholds for mountCrostini() API.
constexpr base::TimeDelta kMountCrostiniSlowOperationThreshold =
    base::Seconds(10);
constexpr base::TimeDelta kMountCrostiniVerySlowOperationThreshold =
    base::Seconds(30);

// Obtains the current app window.
AppWindow* GetCurrentAppWindow(ExtensionFunction* function) {
  content::WebContents* const contents = function->GetSenderWebContents();
  return contents ? AppWindowRegistry::Get(function->browser_context())
                        ->GetAppWindowForWebContents(contents)
                  : nullptr;
}

std::vector<ProfileInfo> GetLoggedInProfileInfoList() {
  DCHECK(user_manager::UserManager::IsInitialized());
  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::set<Profile*> original_profiles;
  std::vector<ProfileInfo> result_profiles;

  for (Profile* profile : profiles) {
    // Filter the profile.
    profile = profile->GetOriginalProfile();
    if (original_profiles.count(profile)) {
      continue;
    }
    original_profiles.insert(profile);
    const user_manager::User* const user =
        ash::ProfileHelper::Get()->GetUserByProfile(profile);
    if (!user || !user->is_logged_in()) {
      continue;
    }

    // Make a ProfileInfo.
    ProfileInfo profile_info;
    profile_info.profile_id =
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
    profile_info.display_name = base::UTF16ToUTF8(user->GetDisplayName());
    // TODO(hirono): Remove the property from the profile_info.
    profile_info.is_current_profile = true;

    result_profiles.push_back(std::move(profile_info));
  }

  return result_profiles;
}

// Converts a list of file system urls (as strings) to a pair of a provided file
// system object and a list of unique paths on the file system. In case of an
// error, false is returned and the error message set.
bool ConvertURLsToProvidedInfo(
    const scoped_refptr<storage::FileSystemContext>& file_system_context,
    const std::vector<std::string>& urls,
    ash::file_system_provider::ProvidedFileSystemInterface** file_system,
    std::vector<base::FilePath>* paths,
    std::string* error) {
  DCHECK(file_system);
  DCHECK(error);

  if (urls.empty()) {
    *error = "At least one file must be specified.";
    return false;
  }

  *file_system = nullptr;
  for (const auto& url : urls) {
    storage::FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(GURL(url)));

    // Convert fusebox URL to its backing (FSP) file system provider URL.
    if (file_system_url.type() == storage::kFileSystemTypeFuseBox) {
      std::string fsp_url(url);
      base::ReplaceFirstSubstringAfterOffset(&fsp_url, 0, "/external/fusebox",
                                             "/external/");
      file_system_url =
          file_system_context->CrackURLInFirstPartyContext(GURL(fsp_url));
    }

    ash::file_system_provider::util::FileSystemURLParser parser(
        file_system_url);
    if (!parser.Parse()) {
      *error = "Related provided file system not found.";
      return false;
    }

    if (*file_system != nullptr) {
      if (*file_system != parser.file_system()) {
        *error = "All entries must be on the same file system.";
        return false;
      }
    } else {
      *file_system = parser.file_system();
    }
    paths->push_back(parser.file_path());
  }

  // Erase duplicates.
  std::sort(paths->begin(), paths->end());
  paths->erase(std::unique(paths->begin(), paths->end()), paths->end());

  return true;
}

bool IsAllowedSource(storage::FileSystemType type,
                     api::file_manager_private::SourceRestriction restriction) {
  switch (restriction) {
    case api::file_manager_private::SOURCE_RESTRICTION_NONE:
      NOTREACHED();
      return false;

    case api::file_manager_private::SOURCE_RESTRICTION_ANY_SOURCE:
      return true;

    case api::file_manager_private::SOURCE_RESTRICTION_NATIVE_SOURCE:
      return type == storage::kFileSystemTypeLocal;
  }
}

std::string Redact(const std::string& s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

std::string Redact(const base::FilePath& path) {
  return Redact(path.value());
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateGetPreferencesFunction::Run() {
  api::file_manager_private::Preferences result;
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const service = profile->GetPrefs();
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);

  result.drive_enabled = drive::util::IsDriveEnabledForProfile(profile) &&
                         drive_integration_service &&
                         !drive_integration_service->mount_failed();
  result.drive_sync_enabled_on_metered_network =
      !service->GetBoolean(drive::prefs::kDisableDriveOverCellular);
  if (drive::util::IsDriveFsBulkPinningAvailable(profile)) {
    result.drive_fs_bulk_pinning_enabled =
        service->GetBoolean(drive::prefs::kDriveFsBulkPinningEnabled);
  }
  result.search_suggest_enabled =
      service->GetBoolean(prefs::kSearchSuggestEnabled);
  result.use24hour_clock = service->GetBoolean(prefs::kUse24HourClock);
  result.timezone = base::UTF16ToUTF8(
      ash::system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID());
  result.arc_enabled = service->GetBoolean(arc::prefs::kArcEnabled);
  result.arc_removable_media_access_enabled =
      service->GetBoolean(arc::prefs::kArcHasAccessToRemovableMedia);
  result.trash_enabled = service->GetBoolean(ash::prefs::kFilesAppTrashEnabled);
  std::vector<std::string> folder_shortcuts;
  const auto& value_list =
      service->GetList(ash::prefs::kFilesAppFolderShortcuts);
  for (const base::Value& value : value_list) {
    folder_shortcuts.push_back(value.is_string() ? value.GetString() : "");
  }
  result.folder_shortcuts = folder_shortcuts;
  result.office_file_moved_one_drive =
      service->GetTime(prefs::kOfficeFileMovedToOneDrive)
          .InMillisecondsFSinceUnixEpoch();
  result.office_file_moved_google_drive =
      service->GetTime(prefs::kOfficeFileMovedToGoogleDrive)
          .InMillisecondsFSinceUnixEpoch();

  return RespondNow(WithArguments(result.ToValue()));
}

ExtensionFunction::ResponseAction
FileManagerPrivateSetPreferencesFunction::Run() {
  using extensions::api::file_manager_private::SetPreferences::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  PrefService* const service = profile->GetPrefs();

  if (params->change_info.drive_sync_enabled_on_metered_network) {
    const bool drive_sync_enabled_on_metered_network =
        *params->change_info.drive_sync_enabled_on_metered_network;
    service->SetBoolean(drive::prefs::kDisableDriveOverCellular,
                        !drive_sync_enabled_on_metered_network);
  }
  if (drive::util::IsDriveFsBulkPinningAvailable(profile) &&
      params->change_info.drive_fs_bulk_pinning_enabled) {
    service->SetBoolean(drive::prefs::kDriveFsBulkPinningEnabled,
                        *params->change_info.drive_fs_bulk_pinning_enabled);
    drivefs::pinning::RecordBulkPinningEnabledSource(
        drivefs::pinning::BulkPinningEnabledSource::kBanner);
  }
  if (params->change_info.arc_enabled) {
    service->SetBoolean(arc::prefs::kArcEnabled,
                        *params->change_info.arc_enabled);
  }
  if (params->change_info.arc_removable_media_access_enabled) {
    service->SetBoolean(
        arc::prefs::kArcHasAccessToRemovableMedia,
        *params->change_info.arc_removable_media_access_enabled);
  }
  if (params->change_info.folder_shortcuts) {
    base::Value::List folder_shortcuts;
    for (auto& shortcut : *params->change_info.folder_shortcuts) {
      folder_shortcuts.Append(shortcut);
    }
    service->SetList(ash::prefs::kFilesAppFolderShortcuts,
                     std::move(folder_shortcuts));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateZoomFunction::Run() {
  using extensions::api::file_manager_private::Zoom::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::PageZoom zoom_type;
  switch (params->operation) {
    case api::file_manager_private::ZOOM_OPERATION_TYPE_IN:
      zoom_type = content::PAGE_ZOOM_IN;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_OUT:
      zoom_type = content::PAGE_ZOOM_OUT;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_RESET:
      zoom_type = content::PAGE_ZOOM_RESET;
      break;
    default:
      NOTREACHED();
      return RespondNow(Error(kUnknownErrorDoNotUse));
  }
  zoom::PageZoom::Zoom(GetSenderWebContents(), zoom_type);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateGetProfilesFunction::Run() {
  const std::vector<ProfileInfo>& profiles = GetLoggedInProfileInfoList();

  // Obtains the display profile ID.
  AppWindow* const app_window = GetCurrentAppWindow(this);
  ash::MultiUserWindowManager* const window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  const AccountId current_profile_id = multi_user_util::GetAccountIdFromProfile(
      Profile::FromBrowserContext(browser_context()));
  const AccountId display_profile_id =
      window_manager && app_window ? window_manager->GetUserPresentingWindow(
                                         app_window->GetNativeWindow())
                                   : EmptyAccountId();

  return RespondNow(
      ArgumentList(api::file_manager_private::GetProfiles::Results::Create(
          profiles, current_profile_id.GetUserEmail(),
          display_profile_id.is_valid() ? display_profile_id.GetUserEmail()
                                        : current_profile_id.GetUserEmail())));
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenInspectorFunction::Run() {
  using extensions::api::file_manager_private::OpenInspector::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  switch (params->type) {
    case extensions::api::file_manager_private::INSPECTION_TYPE_NORMAL:
      // Open inspector for foreground page.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_CONSOLE:
      // Open inspector for foreground page and bring focus to the console.
      DevToolsWindow::OpenDevToolsWindow(
          GetSenderWebContents(), DevToolsToggleAction::ShowConsolePanel());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_ELEMENT:
      // Open inspector for foreground page in inspect element mode.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents(),
                                         DevToolsToggleAction::Inspect());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_BACKGROUND:
      // Open inspector for background page if extension pointer is not null.
      // Files app SWA is not an extension and thus has no associated background
      // page.
      if (extension()) {
        extensions::devtools_util::InspectBackgroundPage(
            extension(), Profile::FromBrowserContext(browser_context()));
      } else {
        return RespondNow(
            Error(base::StringPrintf("Inspection type(%d) not supported.",
                                     static_cast<int>(params->type))));
      }
      break;
    default:
      NOTREACHED();
      return RespondNow(Error(
          base::StringPrintf("Unexpected inspection type(%d) is specified.",
                             static_cast<int>(params->type))));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenSettingsSubpageFunction::Run() {
  using extensions::api::file_manager_private::OpenSettingsSubpage::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (chromeos::settings::IsOSSettingsSubPage(params->sub_page)) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, params->sub_page);
  } else {
    chrome::ShowSettingsSubPageForProfile(profile, params->sub_page);
  }
  return RespondNow(NoArguments());
}

FileManagerPrivateInternalGetMimeTypeFunction::
    FileManagerPrivateInternalGetMimeTypeFunction() = default;

FileManagerPrivateInternalGetMimeTypeFunction::
    ~FileManagerPrivateInternalGetMimeTypeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetMimeTypeFunction::Run() {
  using extensions::api::file_manager_private_internal::GetMimeType::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert file url to local path.
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  storage::FileSystemURL file_system_url(
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url)));

  app_file_handler_util::GetMimeTypeForLocalPath(
      profile, file_system_url.path(),
      base::BindOnce(
          &FileManagerPrivateInternalGetMimeTypeFunction::OnGetMimeType, this));

  return RespondLater();
}

void FileManagerPrivateInternalGetMimeTypeFunction::OnGetMimeType(
    const std::string& mimeType) {
  Respond(WithArguments(mimeType));
}

FileManagerPrivateGetProvidersFunction::
    FileManagerPrivateGetProvidersFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetProvidersFunction::Run() {
  using ash::file_system_provider::Capabilities;
  using ash::file_system_provider::IconSet;
  using ash::file_system_provider::ProviderId;
  using ash::file_system_provider::ProviderInterface;
  using ash::file_system_provider::Service;
  const Service* const service = Service::Get(browser_context());

  using api::file_manager_private::Provider;
  std::vector<Provider> result;
  for (const auto& pair : service->GetProviders()) {
    const ProviderInterface* const provider = pair.second.get();
    const ProviderId provider_id = provider->GetId();

    Provider result_item;
    result_item.provider_id = provider->GetId().ToString();
    const IconSet& icon_set = provider->GetIconSet();
    file_manager::util::FillIconSet(&result_item.icon_set, icon_set);
    result_item.name = provider->GetName();

    const Capabilities capabilities = provider->GetCapabilities();
    result_item.configurable = capabilities.configurable;
    result_item.watchable = capabilities.watchable;
    result_item.multiple_mounts = capabilities.multiple_mounts;
    switch (capabilities.source) {
      case SOURCE_FILE:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_FILE;
        break;
      case SOURCE_DEVICE:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_DEVICE;
        break;
      case SOURCE_NETWORK:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_NETWORK;
        break;
    }
    result.push_back(std::move(result_item));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetProviders::Results::Create(result)));
}

FileManagerPrivateAddProvidedFileSystemFunction::
    FileManagerPrivateAddProvidedFileSystemFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateAddProvidedFileSystemFunction::Run() {
  using ash::file_system_provider::ProviderId;
  using ash::file_system_provider::Service;
  using extensions::api::file_manager_private::AddProvidedFileSystem::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  Service* const service = Service::Get(browser_context());
  ProviderId provider_id = ProviderId::FromString(params->provider_id);

  auto file_systems = service->GetProvidedFileSystemInfoList(provider_id);
  bool first_file_system = file_systems.empty();
  // Show Connect To OneDrive dialog only when mounting ODFS for the first time.
  // There will already a ODFS mount if the user is requesting a new mount to
  // replace the unauthenticated one.
  if (chromeos::cloud_upload::IsMicrosoftOfficeCloudUploadAllowed(profile) &&
      params->provider_id == extension_misc::kODFSExtensionId &&
      first_file_system) {
    // Get Files App window, if it exists.
    Browser* browser =
        FindSystemWebAppBrowser(profile, ash::SystemWebAppType::FILE_MANAGER);
    gfx::NativeWindow modal_parent =
        browser ? browser->window()->GetNativeWindow() : nullptr;

    // This will call into service->RequestMount() if necessary. This is 'fire
    // and forget' as Files app doesn't do anything if this succeeds or fails.
    bool started = ash::cloud_upload::ShowConnectOneDriveDialog(modal_parent);
    return RespondNow(started ? NoArguments()
                              : Error("Failed to request a new mount."));
  }

  if (!service->RequestMount(provider_id, base::DoNothing())) {
    return RespondNow(Error("Failed to request a new mount."));
  }

  return RespondNow(NoArguments());
}

FileManagerPrivateConfigureVolumeFunction::
    FileManagerPrivateConfigureVolumeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateConfigureVolumeFunction::Run() {
  using extensions::api::file_manager_private::ConfigureVolume::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::Volume;
  using file_manager::VolumeManager;
  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  DCHECK(volume_manager);

  std::string volume_id = params->volume_id;
  volume_manager->ConvertFuseBoxFSPVolumeIdToFSPIfNeeded(&volume_id);

  const base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(volume_id);
  if (!volume) {
    return RespondNow(
        Error("ConfigureVolume: volume with ID * not found.", volume_id));
  }

  if (!volume->configurable()) {
    return RespondNow(Error("Volume not configurable."));
  }

  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_PROVIDED: {
      using ash::file_system_provider::Service;
      Service* const service = Service::Get(browser_context());
      DCHECK(service);

      using ash::file_system_provider::ProvidedFileSystemInterface;
      ProvidedFileSystemInterface* const file_system =
          service->GetProvidedFileSystem(volume->provider_id(),
                                         volume->file_system_id());
      if (file_system) {
        file_system->Configure(base::BindOnce(
            &FileManagerPrivateConfigureVolumeFunction::OnCompleted, this));
      }
      break;
    }
    default:
      NOTIMPLEMENTED();
  }

  return RespondLater();
}

void FileManagerPrivateConfigureVolumeFunction::OnCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to complete configuration."));
    return;
  }

  Respond(NoArguments());
}

FileManagerPrivateMountCrostiniFunction::
    FileManagerPrivateMountCrostiniFunction() {
  // Mounting crostini shares may require the crostini VM to be started.
  SetWarningThresholds(kMountCrostiniSlowOperationThreshold,
                       kMountCrostiniVerySlowOperationThreshold);
}

FileManagerPrivateMountCrostiniFunction::
    ~FileManagerPrivateMountCrostiniFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateMountCrostiniFunction::Run() {
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  DCHECK(crostini::CrostiniFeatures::Get()->IsEnabled(profile));
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostini(
      crostini::DefaultContainerId(),
      base::BindOnce(&FileManagerPrivateMountCrostiniFunction::RestartCallback,
                     this));
  return RespondLater();
}

void FileManagerPrivateMountCrostiniFunction::RestartCallback(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    Respond(Error(base::StringPrintf("Error mounting crostini container: %d",
                                     static_cast<int>(result))));
    return;
  }
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  DCHECK(crostini::CrostiniFeatures::Get()->IsEnabled(profile));
  crostini::CrostiniManager::GetForProfile(profile)->MountCrostiniFiles(
      crostini::DefaultContainerId(),
      base::BindOnce(&FileManagerPrivateMountCrostiniFunction::MountCallback,
                     this),
      false);
}

void FileManagerPrivateMountCrostiniFunction::MountCallback(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    Respond(Error(base::StringPrintf("Error mounting crostini container: %d",
                                     static_cast<int>(result))));
    return;
  }
  Respond(NoArguments());
}

FileManagerPrivateInternalImportCrostiniImageFunction::
    FileManagerPrivateInternalImportCrostiniImageFunction() = default;

FileManagerPrivateInternalImportCrostiniImageFunction::
    ~FileManagerPrivateInternalImportCrostiniImageFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalImportCrostiniImageFunction::Run() {
  using extensions::api::file_manager_private_internal::ImportCrostiniImage::
      Params;

  const auto params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  base::FilePath path =
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url))
          .path();

  crostini::CrostiniExportImport::GetForProfile(profile)->ImportContainer(
      crostini::DefaultContainerId(), path,
      base::BindOnce(
          [](base::FilePath path, crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              LOG(ERROR) << "Error importing crostini image " << Redact(path)
                         << ": " << (int)result;
            }
          },
          path));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSharePathsWithCrostiniFunction::Run() {
  using extensions::api::file_manager_private_internal::SharePathsWithCrostini::
      Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  std::vector<base::FilePath> paths;
  for (const auto& url : params->urls) {
    storage::FileSystemURL cracked =
        file_system_context->CrackURLInFirstPartyContext(GURL(url));
    paths.emplace_back(cracked.path());
  }

  auto vm_info =
      guest_os::GuestOsSessionTracker::GetForProfile(profile)->GetVmInfo(
          params->vm_name);
  auto* share_service = guest_os::GuestOsSharePath::GetForProfile(profile);

  share_service->RegisterPersistedPaths(params->vm_name, paths);
  if (vm_info) {
    // The share service will mount persistent shares at VM boot, but if the VM
    // is already running we need to trigger the first mount ourselves.
    share_service->SharePaths(
        params->vm_name, vm_info->seneschal_server_handle(), std::move(paths),
        base::BindOnce(
            &FileManagerPrivateInternalSharePathsWithCrostiniFunction::
                SharePathsCallback,
            this));
    return RespondLater();
  } else {
    return RespondNow(NoArguments());
  }
}

void FileManagerPrivateInternalSharePathsWithCrostiniFunction::
    SharePathsCallback(bool success, const std::string& failure_reason) {
  Respond(success ? NoArguments() : Error(failure_reason));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalUnsharePathWithCrostiniFunction::Run() {
  using extensions::api::file_manager_private_internal::
      UnsharePathWithCrostini::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  storage::FileSystemURL cracked =
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url));
  guest_os::GuestOsSharePath::GetForProfile(profile)->UnsharePath(
      params->vm_name, cracked.path(), /*unpersist=*/true,
      base::BindOnce(
          &FileManagerPrivateInternalUnsharePathWithCrostiniFunction::
              UnsharePathCallback,
          this));

  return RespondLater();
}

void FileManagerPrivateInternalUnsharePathWithCrostiniFunction::
    UnsharePathCallback(bool success, const std::string& failure_reason) {
  Respond(success ? NoArguments() : Error(failure_reason));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetCrostiniSharedPathsFunction::Run() {
  using extensions::api::file_manager_private_internal::GetCrostiniSharedPaths::
      Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile);
  bool first_for_session =
      params->observe_first_for_session &&
      guest_os_share_path->GetAndSetFirstForSession(params->vm_name);
  auto shared_paths =
      guest_os_share_path->GetPersistedSharedPaths(params->vm_name);
  base::Value::List entries;
  for (const base::FilePath& path : shared_paths) {
    std::string mount_name;
    std::string file_system_name;
    std::string full_path;
    if (!file_manager::util::ExtractMountNameFileSystemNameFullPath(
            path, &mount_name, &file_system_name, &full_path)) {
      LOG(ERROR) << "Error extracting mount name and path from "
                 << Redact(path);
      continue;
    }
    base::Value::Dict entry;
    entry.Set("fileSystemRoot", storage::GetExternalFileSystemRootURIString(
                                    source_url(), mount_name));
    entry.Set("fileSystemName", file_system_name);
    entry.Set("fileFullPath", full_path);
    // All shared paths should be directories.  Even if this is not true,
    // it is fine for foreground/js/crostini.js class to think so. We
    // verify that the paths are in fact valid directories before calling
    // seneschal/9p in GuestOsSharePath::CallSeneschalSharePath().
    entry.Set("fileIsDirectory", true);
    entries.Append(std::move(entry));
  }
  return RespondNow(WithArguments(std::move(entries), first_for_session));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetLinuxPackageInfoFunction::Run() {
  using api::file_manager_private_internal::GetLinuxPackageInfo::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageService::GetForProfile(profile)->GetLinuxPackageInfo(
      crostini::DefaultContainerId(),
      file_system_context->CrackURLInFirstPartyContext(GURL(params->url)),
      base::BindOnce(&FileManagerPrivateInternalGetLinuxPackageInfoFunction::
                         OnGetLinuxPackageInfo,
                     this));
  return RespondLater();
}

void FileManagerPrivateInternalGetLinuxPackageInfoFunction::
    OnGetLinuxPackageInfo(
        const crostini::LinuxPackageInfo& linux_package_info) {
  api::file_manager_private::LinuxPackageInfo result;
  if (!linux_package_info.success) {
    Respond(Error(linux_package_info.failure_reason));
    return;
  }

  result.name = linux_package_info.name;
  result.version = linux_package_info.version;
  result.summary = linux_package_info.summary;
  result.description = linux_package_info.description;

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           GetLinuxPackageInfo::Results::Create(result)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalInstallLinuxPackageFunction::Run() {
  using extensions::api::file_manager_private_internal::InstallLinuxPackage::
      Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageService::GetForProfile(profile)
      ->QueueInstallLinuxPackage(
          crostini::DefaultContainerId(),
          file_system_context->CrackURLInFirstPartyContext(GURL(params->url)),
          base::BindOnce(
              &FileManagerPrivateInternalInstallLinuxPackageFunction::
                  OnInstallLinuxPackage,
              this));
  return RespondLater();
}

void FileManagerPrivateInternalInstallLinuxPackageFunction::
    OnInstallLinuxPackage(crostini::CrostiniResult result) {
  extensions::api::file_manager_private::InstallLinuxPackageResponse response;
  switch (result) {
    case crostini::CrostiniResult::SUCCESS:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_STARTED;
      break;
    case crostini::CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_FAILED;
      break;
    case crostini::CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_INSTALL_ALREADY_ACTIVE;
      break;
    default:
      NOTREACHED();
  }
  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           InstallLinuxPackage::Results::Create(response)));
}

FileManagerPrivateInternalGetCustomActionsFunction::
    FileManagerPrivateInternalGetCustomActionsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetCustomActionsFunction::Run() {
  using extensions::api::file_manager_private_internal::GetCustomActions::
      Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  std::vector<base::FilePath> paths;
  ash::file_system_provider::ProvidedFileSystemInterface* file_system = nullptr;
  std::string error;

  if (!ConvertURLsToProvidedInfo(file_system_context, params->urls,
                                 &file_system, &paths, &error)) {
    return RespondNow(Error(error));
  }

  DCHECK(file_system);
  file_system->GetActions(
      paths,
      base::BindOnce(
          &FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted(
    const ash::file_system_provider::Actions& actions,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to fetch actions."));
    return;
  }

  using api::file_system_provider::Action;
  std::vector<Action> items;
  for (const auto& action : actions) {
    Action item;
    item.id = action.id;
    item.title = action.title;
    items.push_back(std::move(item));
  }

  Respond(ArgumentList(
      api::file_manager_private_internal::GetCustomActions::Results::Create(
          items)));
}

FileManagerPrivateInternalExecuteCustomActionFunction::
    FileManagerPrivateInternalExecuteCustomActionFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalExecuteCustomActionFunction::Run() {
  using extensions::api::file_manager_private_internal::ExecuteCustomAction::
      Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  std::vector<base::FilePath> paths;
  ash::file_system_provider::ProvidedFileSystemInterface* file_system = nullptr;
  std::string error;

  if (!ConvertURLsToProvidedInfo(file_system_context, params->urls,
                                 &file_system, &paths, &error)) {
    return RespondNow(Error(error));
  }

  DCHECK(file_system);
  file_system->ExecuteAction(
      paths, params->action_id,
      base::BindOnce(
          &FileManagerPrivateInternalExecuteCustomActionFunction::OnCompleted,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalExecuteCustomActionFunction::OnCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to execute the action."));
    return;
  }

  Respond(NoArguments());
}

FileManagerPrivateInternalGetRecentFilesFunction::
    FileManagerPrivateInternalGetRecentFilesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetRecentFilesFunction::Run() {
  using extensions::api::file_manager_private_internal::GetRecentFiles::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  ash::RecentModel* model = ash::RecentModel::GetForProfile(profile);

  ash::RecentModel::FileType file_type;
  if (!file_manager::util::ToRecentSourceFileType(params->file_category,
                                                  &file_type)) {
    return RespondNow(Error("Cannot convert category to file type"));
  }

  if (base::FeatureList::IsEnabled(ash::features::kFSPsInRecents)) {
    // If File System Provider is enabled, we set the maximum latency to be 3s.
    // This is based on "User Preference and Search Engine Latency" paper, which
    // stated that "[...] once latency exceeds 3 seconds for the slower engine,
    // users are 1.5 times as likely to choose the faster engine."
    model->SetScanTimeout(base::Milliseconds(3000));
  }
  model->GetRecentFiles(
      file_system_context.get(), source_url(), params->query, file_type,
      params->invalidate_cache,
      base::BindOnce(
          &FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles,
          this, params->restriction));
  return RespondLater();
}

void FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles(
    api::file_manager_private::SourceRestriction restriction,
    const std::vector<ash::RecentFile>& files) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& file : files) {
    // Filter out files from non-allowed sources.
    // We do this filtering here rather than in RecentModel so that the set of
    // files returned with some restriction is a subset of what would be
    // returned without restriction. Anyway, the maximum number of files
    // returned from RecentModel is large enough.
    if (!IsAllowedSource(file.url().type(), restriction)) {
      continue;
    }

    file_manager::util::FileDefinition file_definition;
    // Recent file system only lists regular files, not directories.
    file_definition.is_directory = false;
    if (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile, source_url(), file.url().path(),
            &file_definition.virtual_path)) {
      file_definition_list.emplace_back(std::move(file_definition));
    }
  }

  // During this conversion a GET_METADATA_FIELD_IS_DIRECTORY will be triggered
  // in file_system_context.cc DidOpenFileSystemForResolveURL() which will
  // might not use file_definition.is_directory in some scenarios, which will
  // make entry_definition's is_directory become true, so in the callback we
  // need to filter out is_directory=true.
  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                           source_url()),
      url::Origin::Create(source_url().DeprecatedGetOriginAsURL()),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(&FileManagerPrivateInternalGetRecentFilesFunction::
                         OnConvertFileDefinitionListToEntryDefinitionList,
                     this));
}

void FileManagerPrivateInternalGetRecentFilesFunction::
    OnConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  DCHECK(entry_definition_list);

  // Remove all directories entries.
  entry_definition_list->erase(
      std::remove_if(entry_definition_list->begin(),
                     entry_definition_list->end(),
                     [](const file_manager::util::EntryDefinition& e) {
                       return e.is_directory == true;
                     }),
      entry_definition_list->end());

  Respond(
      WithArguments(file_manager::util::ConvertEntryDefinitionListToListValue(
          *entry_definition_list)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateIsTabletModeEnabledFunction::Run() {
  ash::TabletMode* tablet_mode = ash::TabletMode::Get();
  return RespondNow(
      WithArguments(tablet_mode ? tablet_mode->InTabletMode() : false));
}

ExtensionFunction::ResponseAction FileManagerPrivateOpenURLFunction::Run() {
  using extensions::api::file_manager_private::OpenURL::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const GURL url(params->url);

  if (!ash::NewWindowDelegate::GetPrimary()) {
    return RespondNow(
        Error("Could not get NewWindowDelegate's primary browser"));
  }
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateOpenWindowFunction::Run() {
  using extensions::api::file_manager_private::OpenWindow::Params;
  const absl::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const GURL destination_folder(params->params.current_directory_url
                                    ? (*params->params.current_directory_url)
                                    : "");
  const GURL selection_url(
      params->params.selection_url ? (*params->params.selection_url) : "");

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  GURL files_swa_url =
      ::file_manager::util::GetFileManagerMainPageUrlWithParams(
          ui::SelectFileDialog::SELECT_NONE, /*title=*/{}, destination_folder,
          selection_url,
          /*target_name=*/{}, &file_type_info,
          /*file_type_index=*/0,
          /*search_query=*/{},
          /*show_android_picker_apps=*/false,
          /*volume_filter=*/{});

  ash::SystemAppLaunchParams launch_params;
  launch_params.url = files_swa_url;

  Profile* profile = Profile::FromBrowserContext(browser_context());

  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                               launch_params);

  return RespondNow(WithArguments(true));
}

ExtensionFunction::ResponseAction
FileManagerPrivateSendFeedbackFunction::Run() {
  GURL url;
  if (GetSenderWebContents()) {
    url = GetSenderWebContents()->GetVisibleURL();
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  chrome::ShowFeedbackPage(url, profile, chrome::kFeedbackSourceFilesApp,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/std::string(),
                           /*category_tag=*/"chromeos-files-app",
                           /*extra_diagnostics=*/std::string());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDeviceConnectionStateFunction::Run() {
  api::file_manager_private::DeviceConnectionState result =
      content::GetNetworkConnectionTracker()->IsOffline()
          ? api::file_manager_private::DEVICE_CONNECTION_STATE_OFFLINE
          : api::file_manager_private::DEVICE_CONNECTION_STATE_ONLINE;

  return RespondNow(ArgumentList(
      api::file_manager_private::GetDeviceConnectionState::Results::Create(
          result)));
}

}  // namespace extensions
