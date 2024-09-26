// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
#define CHROME_BROWSER_PREFS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_

#include <string>

#include "chrome/browser/sync/prefs/chrome_syncable_prefs_database.h"
#include "components/sync_preferences/pref_model_associator_client.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class ChromePrefModelAssociatorClient
    : public sync_preferences::PrefModelAssociatorClient {
 public:
  // Returns the global instance.
  static ChromePrefModelAssociatorClient* GetInstance();

  ChromePrefModelAssociatorClient(const ChromePrefModelAssociatorClient&) =
      delete;
  ChromePrefModelAssociatorClient& operator=(
      const ChromePrefModelAssociatorClient&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<ChromePrefModelAssociatorClient>;

  ChromePrefModelAssociatorClient();
  ~ChromePrefModelAssociatorClient() override;

  // sync_preferences::PrefModelAssociatorClient implementation.
  bool IsMergeableListPreference(const std::string& pref_name) const override;
  bool IsMergeableDictionaryPreference(
      const std::string& pref_name) const override;
  base::Value MaybeMergePreferenceValues(
      const std::string& pref_name,
      const base::Value& local_value,
      const base::Value& server_value) const override;
  const sync_preferences::SyncablePrefsDatabase& GetSyncablePrefsDatabase()
      const override;

  // This defines the list of preferences that can be synced.
  browser_sync::ChromeSyncablePrefsDatabase chrome_syncable_prefs_database_;
};

#endif  // CHROME_BROWSER_PREFS_CHROME_PREF_MODEL_ASSOCIATOR_CLIENT_H_
