// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace file_manager::file_tasks {

extensions::api::file_manager_private::TaskResult
ConvertLaunchResultToTaskResult(const apps::LaunchResult& result,
                                TaskType task_type) {
  // TODO(benwells): return the correct code here, depending
  // on how the app will be opened in multiprofile.
  namespace fmp = extensions::api::file_manager_private;
  switch (result.state) {
    case apps::State::SUCCESS:
      if (task_type == TASK_TYPE_WEB_APP) {
        return fmp::TASK_RESULT_OPENED;
      } else {
        return fmp::TASK_RESULT_MESSAGE_SENT;
      }
    case apps::State::FAILED_DIRECTORY_NOT_SHARED:
      DCHECK(task_type == TASK_TYPE_PLUGIN_VM_APP);
      return fmp::TASK_RESULT_FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED;
    case apps::State::FAILED:
      return fmp::TASK_RESULT_FAILED;
  }
}

namespace {

TaskType GetTaskType(apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kArc:
      return TASK_TYPE_ARC_APP;
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      return TASK_TYPE_WEB_APP;
    case apps::AppType::kChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kStandaloneBrowserExtension:
      // Chrome apps and Extensions both get called file_handler, even though
      // extensions really have file_browser_handler. It doesn't matter anymore
      // because both are executed through App Service, which can tell the
      // difference itself.
      return TASK_TYPE_FILE_HANDLER;
    case apps::AppType::kBruschetta:
      return TASK_TYPE_BRUSCHETTA_APP;
    case apps::AppType::kCrostini:
      return TASK_TYPE_CROSTINI_APP;
    case apps::AppType::kPluginVm:
      return TASK_TYPE_PLUGIN_VM_APP;
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kMacOs:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kRemote:
    case apps::AppType::kBorealis:
      return TASK_TYPE_UNKNOWN;
  }
}

const char kImportCrostiniImageHandlerId[] =
    "chrome://file-manager/?import-crostini-image";
const char kInstallLinuxPackageHandlerId[] =
    "chrome://file-manager/?install-linux-package";

}  // namespace

bool FileHandlerIsEnabled(Profile* profile,
                          const std::string& app_id,
                          const std::string& file_handler_id) {
  if (app_id != kFileManagerSwaAppId) {
    return true;
  }
  // Crostini deb files and backup files can be disabled by policy.
  if (file_handler_id == kInstallLinuxPackageHandlerId) {
    return crostini::CrostiniFeatures::Get()->IsRootAccessAllowed(profile);
  }
  if (file_handler_id == kImportCrostiniImageHandlerId) {
    return crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile);
  }
  return true;
}

// Check if the file URLs can be mapped to a path inside VMs for
// GuestOS apps to access.
bool FilesCanBeSharedToVm(Profile* profile, std::vector<GURL> file_urls) {
  storage::FileSystemContext* file_system_context =
      util::GetFileManagerFileSystemContext(profile);
  base::FilePath placeholder_vm_mount("/");
  base::FilePath not_used;
  for (const GURL& file_url : file_urls) {
    if (!file_manager::util::ConvertFileSystemURLToPathInsideVM(
            profile, file_system_context->CrackURLInFirstPartyContext(file_url),
            placeholder_vm_mount,
            /*map_crostini_home=*/false, &not_used)) {
      return false;
    }
  }
  return true;
}

Profile* GetProfileWithAppService(Profile* profile) {
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return profile;
  } else {
    if (profile->IsOffTheRecord()) {
      return profile->GetOriginalProfile();
    } else {
      LOG(WARNING) << "Unexpected profile type";
      return nullptr;
    }
  }
}

// TODO(petermarshall): This can be removed along with ParseFilesAppActionId()
// in file_tasks.cc as the legacy files app has been removed.
std::string ToSwaActionId(const std::string& action_id) {
  return std::string(ash::file_manager::kChromeUIFileManagerURL) + "?" +
         action_id;
}

// True if |launch_entry| represents a task which opens the file by getting the
// URL for a file rather than by opening the local contents of the file.
bool IsFilesAppUrlOpener(const apps::IntentLaunchInfo& launch_entry) {
  if (launch_entry.app_id != kFileManagerSwaAppId) {
    return false;
  }
  return launch_entry.activity_name == ToSwaActionId(kActionIdOpenInOffice) ||
         launch_entry.activity_name ==
             ToSwaActionId(kActionIdWebDriveOfficeWord) ||
         launch_entry.activity_name ==
             ToSwaActionId(kActionIdWebDriveOfficeExcel) ||
         launch_entry.activity_name ==
             ToSwaActionId(kActionIdWebDriveOfficePowerPoint);
}

bool IsSystemAppIdWithFileHandlers(base::StringPiece id) {
  return id == web_app::kMediaAppId;
}

void FindAppServiceTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         const std::vector<std::string>& dlp_source_urls,
                         std::vector<FullTaskDescriptor>* result_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(entries.size(), file_urls.size());
  // App Service uses the file extension in the URL for file_handlers for Web
  // Apps.
#if DCHECK_IS_ON()
  for (const GURL& url : file_urls) {
    DCHECK(url.is_valid());
  }
#endif  // DCHECK_IS_ON()

  // WebApps only have full support for files backed by inodes, so tasks
  // provided by most Web Apps will be skipped if any non-native files are
  // present. "System" Web Apps are an exception: we have more control over what
  // they can do, so tasks provided by System Web Apps are the only ones
  // permitted at present. See https://crbug.com/1079065.
  const bool has_non_native_file =
      base::ranges::any_of(entries, [profile](const auto& entry) {
        return util::IsUnderNonNativeLocalPath(profile, entry.path);
      });

  // App Service doesn't exist in Incognito mode but we still want to find
  // handlers to open a download from its notification from Incognito mode. Use
  // the base profile in these cases (see crbug.com/1111695).
  Profile* profile_with_app_service = GetProfileWithAppService(profile);
  if (!profile_with_app_service) {
    LOG(WARNING) << "Unexpected profile type";
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_with_app_service);

  bool files_shareable_to_vm =
      FilesCanBeSharedToVm(profile_with_app_service, file_urls);

  std::vector<apps::IntentFilePtr> intent_files;
  intent_files.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    auto file = std::make_unique<apps::IntentFile>(file_urls.at(i));
    file->mime_type = entries[i].mime_type;
    file->is_directory = entries[i].is_directory;
    file->dlp_source_url = dlp_source_urls[i];
    intent_files.push_back(std::move(file));
  }
  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      proxy->GetAppsForFiles(std::move(intent_files));

  std::vector<apps::AppType> supported_app_types = {
      apps::AppType::kWeb,
      apps::AppType::kSystemWeb,
      apps::AppType::kChromeApp,
      apps::AppType::kExtension,
      apps::AppType::kStandaloneBrowserChromeApp,
      apps::AppType::kStandaloneBrowserExtension,
      apps::AppType::kBruschetta,
      apps::AppType::kCrostini,
      apps::AppType::kPluginVm,
  };
  if (ash::features::ShouldArcFileTasksUseAppService()) {
    supported_app_types.push_back(apps::AppType::kArc);
  }
  for (auto& launch_entry : intent_launch_info) {
    auto app_type = proxy->AppRegistryCache().GetAppType(launch_entry.app_id);
    if (!base::Contains(supported_app_types, app_type)) {
      continue;
    }

    if (app_type == apps::AppType::kWeb ||
        app_type == apps::AppType::kSystemWeb) {
      // Media app and other SWAs can handle "non-native" files, as can special
      // tasks which only access the file via URL.
      if (has_non_native_file &&
          !(IsSystemAppIdWithFileHandlers(launch_entry.app_id) ||
            IsFilesAppUrlOpener(launch_entry))) {
        continue;
      }

      // Check the origin trial and feature flag for file handling in web apps.
      // TODO(1240018): Remove when this feature is fully launched. This check
      // will not work for lacros web apps.
      web_app::WebAppProvider* provider =
          web_app::WebAppProvider::GetDeprecated(profile_with_app_service);
      web_app::OsIntegrationManager& os_integration_manager =
          provider->os_integration_manager();
      if (!os_integration_manager.IsFileHandlingAPIAvailable(
              launch_entry.app_id)) {
        continue;
      }
    }

    if (app_type == apps::AppType::kChromeApp ||
        app_type == apps::AppType::kExtension) {
      if (profile->IsOffTheRecord() &&
          !extensions::util::IsIncognitoEnabled(launch_entry.app_id, profile)) {
        continue;
      }
    }

    if ((app_type == apps::AppType::kBruschetta ||
         app_type == apps::AppType::kCrostini ||
         app_type == apps::AppType::kPluginVm) &&
        !files_shareable_to_vm) {
      continue;
    }

    if (!FileHandlerIsEnabled(profile_with_app_service, launch_entry.app_id,
                              launch_entry.activity_name)) {
      continue;
    }

    constexpr int kIconSize = 32;
    GURL icon_url =
        apps::AppIconSource::GetIconURL(launch_entry.app_id, kIconSize);
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(launch_entry.app_id, GetTaskType(app_type),
                       launch_entry.activity_name),
        launch_entry.activity_label, icon_url,
        /* is_default=*/false,
        // TODO(petermarshall): Handle the rest of the logic from FindWebTasks()
        // e.g. prioritise non-generic handlers.
        /* is_generic_file_handler=*/launch_entry.is_generic_file_handler,
        /* is_file_extension_match=*/launch_entry.is_file_extension_match,
        /* is_dlp_blocked=*/launch_entry.is_dlp_blocked));
  }
}

void ExecuteAppServiceTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    const std::vector<std::string>& mime_types,
    FileTaskFinishedCallback done) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(file_system_urls.size(), mime_types.size());

  // App Service doesn't exist in Incognito mode but apps can be
  // launched (ie. default handler to open a download from its
  // notification) from Incognito mode. Use the base profile in these
  // cases (see crbug.com/1111695).
  Profile* profile_with_app_service = GetProfileWithAppService(profile);
  if (!profile_with_app_service) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Unexpected profile type");
    return;
  }

  std::vector<GURL> file_urls;
  std::vector<apps::IntentFilePtr> intent_files;
  file_urls.reserve(file_system_urls.size());
  intent_files.reserve(file_system_urls.size());
  for (size_t i = 0; i < file_system_urls.size(); i++) {
    file_urls.push_back(file_system_urls[i].ToGURL());

    auto file =
        std::make_unique<apps::IntentFile>(file_system_urls[i].ToGURL());
    file->mime_type = mime_types.at(i);
    intent_files.push_back(std::move(file));
  }

  DCHECK(task.task_type == TASK_TYPE_WEB_APP ||
         task.task_type == TASK_TYPE_FILE_HANDLER ||
         task.task_type == TASK_TYPE_BRUSCHETTA_APP ||
         task.task_type == TASK_TYPE_CROSTINI_APP ||
         task.task_type == TASK_TYPE_PLUGIN_VM_APP ||
         (ash::features::ShouldArcFileTasksUseAppService() &&
          task.task_type == TASK_TYPE_ARC_APP));

  apps::IntentPtr intent = std::make_unique<apps::Intent>(
      apps_util::kIntentActionView, std::move(intent_files));
  intent->activity_name = task.action_id;

  apps::AppServiceProxyFactory::GetForProfile(profile_with_app_service)
      ->LaunchAppWithIntent(
          task.app_id, ui::EF_NONE, std::move(intent),
          apps::LaunchSource::kFromFileManager,
          std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId),
          base::BindOnce(
              [](FileTaskFinishedCallback done, TaskType task_type,
                 apps::LaunchResult&& result) {
                std::move(done).Run(
                    ConvertLaunchResultToTaskResult(result, task_type), "");
              },
              std::move(done), task.task_type));
}

absl::optional<std::string> GetPolicyDefaultHandlerForFileExtension(
    Profile* profile,
    const std::string& file_extension) {
  const auto& policy_default_handlers =
      profile->GetPrefs()->GetDict(prefs::kDefaultHandlersForFileExtensions);
  if (auto* policy_default_handler =
          policy_default_handlers.FindString(file_extension)) {
    return *policy_default_handler;
  }
  return {};
}

bool ChooseAndSetDefaultTaskFromPolicyPrefs(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    ResultingTasks* resulting_tasks) {
  // Check that there are no conflicting assignments for the given set of
  // entries.
  base::flat_set<std::string> default_handlers_for_entries;
  for (const auto& entry : entries) {
    if (auto policy_default_handler = GetPolicyDefaultHandlerForFileExtension(
            profile, entry.path.Extension())) {
      default_handlers_for_entries.insert(*policy_default_handler);
    }
  }

  // If there are no policy-set handlers, we fallback to the regular flow.
  if (default_handlers_for_entries.empty()) {
    return false;
  }

  // Conflicting assignment! No default should be set.
  if (default_handlers_for_entries.size() > 1) {
    resulting_tasks->policy_default_handler_status =
        PolicyDefaultHandlerStatus::kIncorrectAssignment;
    return true;
  }

  DCHECK_EQ(default_handlers_for_entries.size(), 1U);
  const auto& policy_id = *default_handlers_for_entries.begin();

  if (auto app_id = apps_util::GetAppIdFromPolicyId(profile, policy_id)) {
    auto task_it = base::ranges::find_if(
        resulting_tasks->tasks, [&app_id](const FullTaskDescriptor& task) {
          return task.task_descriptor.app_id == *app_id;
        });
    if (task_it != resulting_tasks->tasks.end()) {
      task_it->is_default = true;
      resulting_tasks->policy_default_handler_status =
          PolicyDefaultHandlerStatus::kDefaultHandlerAssignedByPolicy;
      return true;
    }
  }

  // The corresponding task was not found -- no default.
  resulting_tasks->policy_default_handler_status =
      PolicyDefaultHandlerStatus::kIncorrectAssignment;
  return true;
}

}  // namespace file_manager::file_tasks
