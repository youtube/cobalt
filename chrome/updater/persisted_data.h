// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PERSISTED_DATA_H_
#define CHROME_UPDATER_PERSISTED_DATA_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

class PrefService;
class PrefRegistrySimple;

namespace base {
class FilePath;
class Time;
class Version;
}  // namespace base

namespace updater {

struct RegistrationRequest;

// PersistedData uses the PrefService to persist updater data that outlives
// the updater processes.
//
// This class has sequence affinity.
//
// A mechanism to remove apps or app versions from prefs is needed.
// TODO(sorin): crbug.com/1056450
class PersistedData : public base::RefCountedThreadSafe<PersistedData> {
 public:
  // Constructs a provider using the specified |pref_service|.
  // The associated preferences are assumed to already be registered.
  // The |pref_service| must outlive the instance of this class.
  PersistedData(UpdaterScope scope, PrefService* pref_service);
  PersistedData(const PersistedData&) = delete;
  PersistedData& operator=(const PersistedData&) = delete;

  // These functions access |pv| data for the specified |id|. Returns an empty
  // version, if the version is not found.
  base::Version GetProductVersion(const std::string& id) const;
  void SetProductVersion(const std::string& id, const base::Version& pv);

  // These functions access |fingerprint| data for the specified |id|.
  std::string GetFingerprint(const std::string& id) const;
  void SetFingerprint(const std::string& id, const std::string& fp);

  // These functions access the existence checker path for the specified id.
  base::FilePath GetExistenceCheckerPath(const std::string& id) const;
  void SetExistenceCheckerPath(const std::string& id,
                               const base::FilePath& ecp);

  // These functions access the brand code for the specified id.
  std::string GetBrandCode(const std::string& id) const;
  void SetBrandCode(const std::string& id, const std::string& bc);

  // These functions access the brand path for the specified id.
  base::FilePath GetBrandPath(const std::string& id) const;
  void SetBrandPath(const std::string& id, const base::FilePath& bp);

  // These functions access the AP for the specified id.
  std::string GetAP(const std::string& id) const;
  void SetAP(const std::string& id, const std::string& ap);

  // These functions get/set the client-regulated-counting data for the
  // specified id. The functions are for app migration only.
  // The getters return nullopt when the persisted data does not have the
  // corresponding value, or any node subtype is not expected along the
  // path to the target value.
  absl::optional<int> GetDateLastActive(const std::string& id) const;
  void SetDateLastActive(const std::string& id, int dla);
  absl::optional<int> GetDateLastRollcall(const std::string& id) const;
  void SetDateLastRollcall(const std::string& id, int dlrc);

  // This function sets any non-empty field in the registration request object
  // into the persistent data store.
  void RegisterApp(const RegistrationRequest& rq);

  // This function removes a registered application from the persistent store.
  bool RemoveApp(const std::string& id);

  // Returns the app ids of the applications registered in prefs, if the
  // application has a valid version.
  std::vector<std::string> GetAppIds() const;

  // HadApps is set when the updater processes a registration for an app other
  // than itself, and is never unset, even if the app is uninstalled.
  bool GetHadApps() const;
  void SetHadApps();

  // UsageStatsEnabled reflects whether the updater as a whole is allowed to
  // send usage stats, and is set or reset periodically based on the usage
  // stats opt-in state of each product.
  bool GetUsageStatsEnabled() const;
  void SetUsageStatsEnabled(bool usage_stats_enabled);

  // LastChecked is set when the updater completed successfully a call to
  // `UpdateService::UpdateAll` as indicated by the `UpdateService::Result`
  // argument of the completion callback. This means that the execution path
  // for updating all applications works end to end, including communicating
  // with the backend.
  base::Time GetLastChecked() const;
  void SetLastChecked(const base::Time& time);

  // LastStarted is set when `UpdateService::RunPeriodicTasks` is called. This
  // indicates that the mechanism to initiate automated update checks is
  // working.
  base::Time GetLastStarted() const;
  void SetLastStarted(const base::Time& time);

#if BUILDFLAG(IS_WIN)
  // Retrieves the previously stored OS version.
  absl::optional<OSVERSIONINFOEX> GetLastOSVersion() const;

  // Stores the current os version.
  void SetLastOSVersion();
#endif

 private:
  friend class base::RefCountedThreadSafe<PersistedData>;
  ~PersistedData();

  // Returns nullptr if the app key does not exist.
  const base::Value::Dict* GetAppKey(const std::string& id) const;

  // Returns an existing or newly created app key under a root pref.
  base::Value::Dict* GetOrCreateAppKey(const std::string& id,
                                       base::Value::Dict& root);

  absl::optional<int> GetInteger(const std::string& id,
                                 const std::string& key) const;
  void SetInteger(const std::string& id, const std::string& key, int value);
  std::string GetString(const std::string& id, const std::string& key) const;
  void SetString(const std::string& id,
                 const std::string& key,
                 const std::string& value);

  SEQUENCE_CHECKER(sequence_checker_);

  const UpdaterScope scope_;
  raw_ptr<PrefService> pref_service_ = nullptr;  // Not owned by this class.
};

void RegisterPersistedDataPrefs(scoped_refptr<PrefRegistrySimple> registry);

}  // namespace updater

#endif  // CHROME_UPDATER_PERSISTED_DATA_H_
