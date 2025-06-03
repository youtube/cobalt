// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/permission.h"

namespace apps {

using AppInfo = AppStorageFileHandler::AppInfo;

namespace {

constexpr char kAppServiceDirName[] = "app_service";
constexpr char kAppStorageFileName[] = "AppStorage";

constexpr char kTypeKey[] = "type";
constexpr char kReadinessKey[] = "readiness";
constexpr char kNameKey[] = "name";
constexpr char kShortNameKey[] = "short_name";
constexpr char kDescriptionKey[] = "description";
constexpr char kVersionKey[] = "version";
constexpr char kAdditionalSearchTermsKey[] = "additional_search_terms";
constexpr char kIconResourceIdKey[] = "icon_resource_id";
constexpr char kLastLaunchTimeKey[] = "last_launch_time";
constexpr char kInstallTimeKey[] = "install_time";
constexpr char kPermissionsKey[] = "permissions";
constexpr char kInstallReasonKey[] = "install_reason";
constexpr char kInstallSourceKey[] = "install_source";
constexpr char kPolicyIdsKey[] = "policy_ids";
constexpr char kIsPlatformAppKey[] = "is_platform_app";
constexpr char kRecommendableKey[] = "recommendable";
constexpr char kSearchableKey[] = "searchable";
constexpr char kShowInLauncherKey[] = "show_in_launcher";
constexpr char kShowInShelfKey[] = "show_in_shelf";
constexpr char kShowInSearchKey[] = "show_in_search";
constexpr char kShowInManagementKey[] = "show_in_management";
constexpr char kHandlesIntentsKey[] = "handles_intents";
constexpr char kAllowUninstallKey[] = "allow_uninstall";
constexpr char kIntentFiltersKey[] = "intent_filters";
constexpr char kWindowModeKey[] = "window_mode";

absl::optional<std::string> GetStringValueFromDict(
    const base::Value::Dict& dict,
    base::StringPiece key_name) {
  const std::string* value = dict.FindString(key_name);
  return value ? absl::optional<std::string>(*value) : absl::nullopt;
}

template <typename T>
bool FieldHasValue(const AppPtr& app, T App::*field) {
  return app.get()->*field != T::kUnknown;
}

template <typename T>
bool FieldHasValue(const AppPtr& app, absl::optional<T> App::*field) {
  return (app.get()->*field).has_value();
}

template <typename T>
bool FieldHasValue(const AppPtr& app, std::vector<T> App::*field) {
  return !(app.get()->*field).empty();
}

template <typename T>
base::Value GetValue(const AppPtr& app, T App::*field) {
  return base::Value(static_cast<int>(app.get()->*field));
}

template <typename T>
base::Value GetValue(const AppPtr& app, absl::optional<T> App::*field) {
  return base::Value((app.get()->*field).value());
}

template <>
base::Value GetValue(const AppPtr& app,
                     absl::optional<base::Time> App::*field) {
  return base::TimeToValue((app.get()->*field).value());
}

template <>
base::Value GetValue(const AppPtr& app, std::vector<std::string> App::*field) {
  base::Value::List items;
  for (const auto& item : app.get()->*field) {
    items.Append(item);
  }
  return base::Value(std::move(items));
}

template <>
base::Value GetValue(const AppPtr& app,
                     std::vector<IntentFilterPtr> App::*field) {
  base::Value::List intent_filters;
  for (const auto& intent_filter : app.get()->*field) {
    intent_filters.Append(apps_util::ConvertIntentFilterToDict(intent_filter));
  }
  return base::Value(std::move(intent_filters));
}

template <>
base::Value GetValue(const AppPtr& app,
                     std::vector<PermissionPtr> App::*field) {
  return base::Value(ConvertPermissionsToList(app.get()->*field));
}

template <typename T>
void SetKey(const AppPtr& app,
            T App::*field,
            const std::string& key,
            base::Value::Dict& app_details_dict) {
  if (FieldHasValue(app, field)) {
    app_details_dict.Set(key, GetValue(app, field));
  }
}

template <typename T>
void GetEnumFromKey(base::Value::Dict* value,
                    T App::*field,
                    const std::string& key,
                    AppPtr& app) {
  auto result = value->FindInt(key);
  if (result.has_value() && result.value() >= static_cast<int>(T::kUnknown) &&
      result.value() <= static_cast<int>(T::kMaxValue)) {
    app.get()->*field = static_cast<T>(result.value());
  }
}

}  // namespace

AppStorageFileHandler::AppInfo::AppInfo() = default;
AppStorageFileHandler::AppInfo::~AppInfo() = default;

AppStorageFileHandler::AppStorageFileHandler(const base::FilePath& base_path)
    : RefCountedDeleteOnSequence(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      file_path_(base_path.AppendASCII(kAppServiceDirName)
                     .AppendASCII(kAppStorageFileName)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void AppStorageFileHandler::WriteToFile(std::vector<AppPtr> apps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::CreateDirectory(file_path_.DirName())) {
    LOG(ERROR) << "Fail to create the directory for " << file_path_;
    return;
  }

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(ConvertAppsToValue(std::move(apps)));

  if (!base::ImportantFileWriter::WriteFileAtomically(file_path_,
                                                      json_string)) {
    LOG(ERROR) << "Fail to write the app info to " << file_path_;
  }
}

std::unique_ptr<AppInfo> AppStorageFileHandler::ReadFromFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(file_path_)) {
    return nullptr;
  }

  std::string app_info_data;
  if (!base::ReadFileToString(file_path_, &app_info_data) ||
      app_info_data.empty()) {
    return nullptr;
  }

  base::JSONReader::Result app_info_value =
      base::JSONReader::ReadAndReturnValueWithError(app_info_data);
  if (!app_info_value.has_value()) {
    LOG(ERROR)
        << "Fail to deserialize json value from string with error message: "
        << app_info_value.error().message << ", in line "
        << app_info_value.error().line << ", column "
        << app_info_value.error().column;
    return nullptr;
  }

  return ConvertValueToApps(std::move(*app_info_value));
}

AppStorageFileHandler::~AppStorageFileHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::Value AppStorageFileHandler::ConvertAppsToValue(
    std::vector<AppPtr> apps) {
  base::Value::Dict app_info_dict;
  for (const auto& app : apps) {
    base::Value::Dict dict;

    dict.Set(kTypeKey, static_cast<int>(app->app_type));

    SetKey(app, &App::readiness, kReadinessKey, dict);
    SetKey(app, &App::name, kNameKey, dict);
    SetKey(app, &App::short_name, kShortNameKey, dict);
    SetKey(app, &App::description, kDescriptionKey, dict);
    SetKey(app, &App::version, kVersionKey, dict);
    SetKey(app, &App::additional_search_terms, kAdditionalSearchTermsKey, dict);

    if (app->icon_key.has_value() &&
        app->icon_key->resource_id != IconKey::kInvalidResourceId) {
      dict.Set(kIconResourceIdKey, app->icon_key->resource_id);
    }

    SetKey(app, &App::last_launch_time, kLastLaunchTimeKey, dict);
    SetKey(app, &App::install_time, kInstallTimeKey, dict);
    SetKey(app, &App::permissions, kPermissionsKey, dict);
    SetKey(app, &App::install_reason, kInstallReasonKey, dict);
    SetKey(app, &App::install_source, kInstallSourceKey, dict);
    SetKey(app, &App::policy_ids, kPolicyIdsKey, dict);
    SetKey(app, &App::is_platform_app, kIsPlatformAppKey, dict);
    SetKey(app, &App::recommendable, kRecommendableKey, dict);
    SetKey(app, &App::searchable, kSearchableKey, dict);
    SetKey(app, &App::show_in_launcher, kShowInLauncherKey, dict);
    SetKey(app, &App::show_in_shelf, kShowInShelfKey, dict);
    SetKey(app, &App::show_in_search, kShowInSearchKey, dict);
    SetKey(app, &App::show_in_management, kShowInManagementKey, dict);
    SetKey(app, &App::handles_intents, kHandlesIntentsKey, dict);
    SetKey(app, &App::allow_uninstall, kAllowUninstallKey, dict);
    SetKey(app, &App::intent_filters, kIntentFiltersKey, dict);
    SetKey(app, &App::window_mode, kWindowModeKey, dict);

    // TODO(crbug.com/1385932): Add other files in the App structure.
    app_info_dict.Set(app->app_id, std::move(dict));
  }

  return base::Value(std::move(app_info_dict));
}

std::unique_ptr<AppInfo> AppStorageFileHandler::ConvertValueToApps(
    base::Value app_info_value) {
  std::unique_ptr<AppInfo> app_info = std::make_unique<AppInfo>();

  base::Value::Dict* dict = app_info_value.GetIfDict();
  if (!dict) {
    LOG(ERROR) << "Fail to parse the app info value. "
               << "Cannot find the app info dict.";
    return nullptr;
  }

  for (auto [app_id, app_value] : *dict) {
    base::Value::Dict* value = app_value.GetIfDict();

    if (!value) {
      LOG(ERROR) << "Fail to parse the app info value. "
                 << "Cannot find the value for the app:" << app_id;
      continue;
    }

    auto app_type = value->FindInt(kTypeKey);
    if (!app_type.has_value() ||
        app_type.value() < static_cast<int>(AppType::kUnknown) ||
        app_type.value() > static_cast<int>(AppType::kMaxValue)) {
      LOG(ERROR) << "Fail to parse the app info value. "
                 << "Cannot find the app type for the app:" << app_id;
      continue;
    }

    auto app =
        std::make_unique<App>(static_cast<AppType>(app_type.value()), app_id);

    GetEnumFromKey(value, &App::readiness, kReadinessKey, app);

    app->name = GetStringValueFromDict(*value, kNameKey);
    app->short_name = GetStringValueFromDict(*value, kShortNameKey);
    app->description = GetStringValueFromDict(*value, kDescriptionKey);
    app->version = GetStringValueFromDict(*value, kVersionKey);

    auto* additional_search_terms = value->FindList(kAdditionalSearchTermsKey);
    if (additional_search_terms) {
      for (auto& additional_search_term : *additional_search_terms) {
        app->additional_search_terms.push_back(
            additional_search_term.GetString());
      }
    }

    auto icon_resource_id = value->FindInt(kIconResourceIdKey);
    if (icon_resource_id.has_value()) {
      app->icon_key =
          apps::IconKey(apps::IconKey::kDoesNotChangeOverTime,
                        icon_resource_id.value(), apps::IconEffects::kNone);
    }

    app->last_launch_time = base::ValueToTime(value->Find(kLastLaunchTimeKey));
    app->install_time = base::ValueToTime(value->Find(kInstallTimeKey));

    app->permissions =
        ConvertListToPermissions(value->FindList(kPermissionsKey));

    GetEnumFromKey(value, &App::install_reason, kInstallReasonKey, app);
    GetEnumFromKey(value, &App::install_source, kInstallSourceKey, app);

    auto* policy_ids = value->FindList(kPolicyIdsKey);
    if (policy_ids) {
      for (auto& policy_id : *policy_ids) {
        app->policy_ids.push_back(policy_id.GetString());
      }
    }

    app->is_platform_app = value->FindBool(kIsPlatformAppKey);
    app->recommendable = value->FindBool(kRecommendableKey);
    app->searchable = value->FindBool(kSearchableKey);
    app->show_in_launcher = value->FindBool(kShowInLauncherKey);
    app->show_in_shelf = value->FindBool(kShowInShelfKey);
    app->show_in_search = value->FindBool(kShowInSearchKey);
    app->show_in_management = value->FindBool(kShowInManagementKey);
    app->handles_intents = value->FindBool(kHandlesIntentsKey);
    app->allow_uninstall = value->FindBool(kAllowUninstallKey);

    // Set has_badge as false for the default init value, and wait for the
    // publishers to update the has_badge status.
    app->has_badge = false;

    // Set paused as false for the default init value to keep the consistent
    // implementation as AppPublisher::MakeApp, and wait for the family link to
    // update the pasued status.
    app->paused = false;

    auto* intent_filters = value->FindList(kIntentFiltersKey);
    if (intent_filters) {
      for (const auto& intent_filter : *intent_filters) {
        app->intent_filters.push_back(
            apps_util::ConvertDictToIntentFilter(intent_filter.GetIfDict()));
      }
    }

    GetEnumFromKey(value, &App::window_mode, kWindowModeKey, app);

    // TODO(crbug.com/1385932): Add other files in the App structure.
    app_info->apps.push_back(std::move(app));
    app_info->app_types.insert(static_cast<AppType>(app_type.value()));
  }
  return app_info;
}

}  // namespace apps
