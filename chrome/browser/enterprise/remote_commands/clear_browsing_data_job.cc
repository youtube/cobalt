// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/clear_browsing_data_job.h"

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_commands {

namespace {

const char kFailedTypesPath[] = "failed_data_types";

const char kProfilePathField[] = "profile_path";
const char kClearCacheField[] = "clear_cache";
const char kClearCookiesField[] = "clear_cookies";

// Define the possibly failed data types here for 2 reasons:
//
// 1. This will be easier to keep in sync with the server, as the latter
// doesn't care about *all* the types in BrowsingDataRemover.
//
// 2. Centralize handling the underlying type of the values here.
// BrowsingDataRemover represents failed types as uint64_t, which isn't
// natively supported by base::Value, so this class needs to convert to a
// type that's supported. This will also allow us to use a list instead of a
// bit mask, which will be easier to parse gracefully on the server in case
// more types are added.
enum class DataTypes {
  kCache = 0,
  kCookies = 1,
};

std::string CreatePayload(uint64_t failed_data_types) {
  base::Value::Dict root;
  base::Value::List failed_types_list;

  if (failed_data_types & content::BrowsingDataRemover::DATA_TYPE_CACHE)
    failed_types_list.Append(static_cast<int>(DataTypes::kCache));

  if (failed_data_types & content::BrowsingDataRemover::DATA_TYPE_COOKIES)
    failed_types_list.Append(static_cast<int>(DataTypes::kCookies));

  root.Set(kFailedTypesPath, std::move(failed_types_list));

  std::string payload;
  base::JSONWriter::Write(root, &payload);
  return payload;
}

}  // namespace

ClearBrowsingDataJob::ClearBrowsingDataJob(ProfileManager* profile_manager)
    : profile_manager_(profile_manager) {}

ClearBrowsingDataJob::~ClearBrowsingDataJob() = default;

enterprise_management::RemoteCommand_Type ClearBrowsingDataJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA;
}

bool ClearBrowsingDataJob::ParseCommandPayload(
    const std::string& command_payload) {
  absl::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root)
    return false;

  if (!root->is_dict())
    return false;
  const base::Value::Dict& dict = root->GetDict();

  const std::string* path = dict.FindString(kProfilePathField);
  if (!path)
    return false;

  // On Windows, file paths are wstring as opposed to string on other platforms.
  // On POSIX platforms other than MacOS and ChromeOS, the encoding is unknown.
  //
  // This path is sent from the server, which obtained it from Chrome in a
  // previous report, and Chrome casts the path as UTF8 using UTF8Unsafe before
  // sending it (see BrowserReportGeneratorDesktop::GenerateProfileInfo).
  // Because of that, the best thing we can do everywhere is try to get the
  // path from UTF8, and ending up with an invalid path will fail later in
  // RunImpl when we attempt to get the profile from the path.
  profile_path_ = base::FilePath::FromUTF8Unsafe(*path);
#if BUILDFLAG(IS_WIN)
  // For Windows machines, the path that Chrome reports for the profile is
  // "Normalized" to all lower-case on the reporting server. This means that
  // when the server sends the command, the path will be all lower case and
  // the profile manager won't be able to use it as a key. To avoid this issue,
  // This code will iterate over all profile paths and find the one that matches
  // in a case-insensitive comparison. If this doesn't find one, RunImpl will
  // fail in the same manner as if the profile didn't exist, which is the
  // expected behavior.
  ProfileAttributesStorage& storage =
      profile_manager_->GetProfileAttributesStorage();
  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    base::FilePath entry_path = entry->GetPath();

    if (base::FilePath::CompareEqualIgnoreCase(profile_path_.value(),
                                               entry_path.value())) {
      profile_path_ = entry_path;
      break;
    }
  }
#endif

  // Not specifying these fields is equivalent to setting them to false.
  clear_cache_ = dict.FindBool(kClearCacheField).value_or(false);
  clear_cookies_ = dict.FindBool(kClearCookiesField).value_or(false);

  return true;
}

void ClearBrowsingDataJob::RunImpl(CallbackWithResult result_callback) {
  DCHECK(profile_manager_);

  uint64_t types = 0;
  if (clear_cache_)
    types |= content::BrowsingDataRemover::DATA_TYPE_CACHE;

  if (clear_cookies_)
    types |= content::BrowsingDataRemover::DATA_TYPE_COOKIES;

  Profile* profile = profile_manager_->GetProfileByPath(profile_path_);
  if (!profile) {
    // If the payload's profile path doesn't correspond to an existing profile,
    // there's nothing to do. The most likely scenario is that the profile was
    // deleted by the time the command was received.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback), policy::ResultType::kFailure,
                       CreatePayload(types)));
    return;
  }

  result_callback_ = std::move(result_callback);

  if (types == 0) {
    // There's nothing to clear, invoke the callback with success result and be
    // done.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback_),
                                  policy::ResultType::kSuccess,
                                  CreatePayload(
                                      /*failed_data_types=*/0)));
    return;
  }

  content::BrowsingDataRemover* remover = profile->GetBrowsingDataRemover();
  remover->AddObserver(this);

  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), types,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, this);
}

void ClearBrowsingDataJob::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  Profile* profile = profile_manager_->GetProfileByPath(profile_path_);
  DCHECK(profile);

  content::BrowsingDataRemover* remover = profile->GetBrowsingDataRemover();
  remover->RemoveObserver(this);

  std::string payload = CreatePayload(failed_data_types);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback_),
                     failed_data_types != 0 ? policy::ResultType::kFailure
                                            : policy::ResultType::kSuccess,
                     std::move(payload)));
}

}  // namespace enterprise_commands
