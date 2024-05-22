// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/persisted_data.h"

#include <string>
#include <vector>

#include "base/guid.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/update_client/activity_data_service.h"

const char kPersistedDataPreference[] = "updateclientdata";

namespace update_client {

PersistedData::PersistedData(PrefService* pref_service,
                             ActivityDataService* activity_data_service)
    : pref_service_(pref_service),
      activity_data_service_(activity_data_service) {}

PersistedData::~PersistedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int PersistedData::GetInt(const std::string& id,
                          const std::string& key,
                          int fallback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We assume ids do not contain '.' characters.
  DCHECK_EQ(std::string::npos, id.find('.'));
  if (!pref_service_)
    return fallback;
  const base::Value::Dict* dict =
      pref_service_->GetDictionary(kPersistedDataPreference);
  if (!dict)
    return fallback;
  absl::optional<int> result = dict->FindInt(
             base::StringPrintf("apps.%s.%s", id.c_str(), key.c_str()));
  return result.value_or(fallback);
}

std::string PersistedData::GetString(const std::string& id,
                                     const std::string& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We assume ids do not contain '.' characters.
  DCHECK_EQ(std::string::npos, id.find('.'));
  if (!pref_service_)
    return std::string();
  const base::Value::Dict* dict =
      pref_service_->GetDictionary(kPersistedDataPreference);
  if (!dict)
    return std::string();
  const std::string* result = dict->FindString(
             base::StringPrintf("apps.%s.%s", id.c_str(), key.c_str()));
  return result != nullptr ? *result : std::string();
}

int PersistedData::GetDateLastRollCall(const std::string& id) const {
  return GetInt(id, "dlrc", kDateUnknown);
}

int PersistedData::GetDateLastActive(const std::string& id) const {
  return GetInt(id, "dla", kDateUnknown);
}

std::string PersistedData::GetPingFreshness(const std::string& id) const {
  std::string result = GetString(id, "pf");
  return !result.empty() ? base::StringPrintf("{%s}", result.c_str()) : result;
}

#if defined(STARBOARD)
std::string PersistedData::GetLastInstalledVersion(const std::string& id) const {
  return GetString(id, "version");
}
std::string PersistedData::GetUpdaterChannel(const std::string& id) const {
  return GetString(id, "updaterchannel");
}
std::string PersistedData::GetLatestChannel() const {
  const base::Value::Dict* dict =
      pref_service_->GetDictionary(kPersistedDataPreference);
  if (!dict)
    return std::string();
  const std::string* result = dict->FindString("latestchannel");
  return result != nullptr ? *result : std::string();
}
#endif

std::string PersistedData::GetCohort(const std::string& id) const {
  return GetString(id, "cohort");
}

std::string PersistedData::GetCohortName(const std::string& id) const {
  return GetString(id, "cohortname");
}

std::string PersistedData::GetCohortHint(const std::string& id) const {
  return GetString(id, "cohorthint");
}

void PersistedData::SetDateLastRollCall(const std::vector<std::string>& ids,
                                        int datenum) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_ || datenum < 0)
    return;
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  for (const auto& id : ids) {
    // We assume ids do not contain '.' characters.
    DCHECK_EQ(std::string::npos, id.find('.'));
    update->Set(base::StringPrintf("apps.%s.dlrc", id.c_str()), datenum);
    update->Set(base::StringPrintf("apps.%s.pf", id.c_str()),
                      base::GenerateGUID());
  }
}

void PersistedData::SetDateLastActive(const std::vector<std::string>& ids,
                                      int datenum) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_ || datenum < 0)
    return;
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  for (const auto& id : ids) {
    if (GetActiveBit(id)) {
      // We assume ids do not contain '.' characters.
      DCHECK_EQ(std::string::npos, id.find('.'));
      update->Set(base::StringPrintf("apps.%s.dla", id.c_str()),
                         datenum);
      activity_data_service_->ClearActiveBit(id);
    }
  }
}

void PersistedData::SetString(const std::string& id,
                              const std::string& key,
                              const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_)
    return;
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  update->Set(base::StringPrintf("apps.%s.%s", id.c_str(), key.c_str()),
                    value);
}

#if defined(STARBOARD)
void PersistedData::SetLastInstalledVersion(const std::string& id,
                                           const std::string& version) {
  SetString(id, "version", version);
}
void PersistedData::SetUpdaterChannel(const std::string& id,
                                      const std::string& channel) {
  SetString(id, "updaterchannel", channel);
}
void PersistedData::SetLatestChannel(const std::string& channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_)
    return;
  ScopedDictPrefUpdate update(pref_service_, kPersistedDataPreference);
  update->Set("latestchannel", channel);
}
#endif

void PersistedData::SetCohort(const std::string& id,
                              const std::string& cohort) {
  SetString(id, "cohort", cohort);
}

void PersistedData::SetCohortName(const std::string& id,
                                  const std::string& cohort_name) {
  SetString(id, "cohortname", cohort_name);
}

void PersistedData::SetCohortHint(const std::string& id,
                                  const std::string& cohort_hint) {
  SetString(id, "cohorthint", cohort_hint);
}

bool PersistedData::GetActiveBit(const std::string& id) const {
  return activity_data_service_ && activity_data_service_->GetActiveBit(id);
}

int PersistedData::GetDaysSinceLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return activity_data_service_
             ? activity_data_service_->GetDaysSinceLastRollCall(id)
             : kDaysUnknown;
}

int PersistedData::GetDaysSinceLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return activity_data_service_
             ? activity_data_service_->GetDaysSinceLastActive(id)
             : kDaysUnknown;
}

void PersistedData::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kPersistedDataPreference);
}

}  // namespace update_client
