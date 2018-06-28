// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_STORAGE_UPGRADE_UPGRADE_READER_H_
#define COBALT_STORAGE_UPGRADE_UPGRADE_READER_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "net/cookies/canonical_cookie.h"

namespace cobalt {
namespace storage {
namespace upgrade {

// Class description goes here.
class UpgradeReader {
 public:
  struct LocalStorageEntry {
    LocalStorageEntry(const std::string& key, const std::string& value)
        : key(key), value(value) {}
    std::string key;
    std::string value;
  };

  UpgradeReader() {}
  virtual ~UpgradeReader() {}

  // Parse |data| in JSON-encoded legacy save data upgrade format.
  void Parse(const char* data, int size);

  // The number of valid cookies found in the parsed data. May be zero.
  int GetNumCookies() const;

  // The number of valid local storage entries found in the parsed  data. May be
  // zero.
  int GetNumLocalStorageEntries() const;

  // Get one of the cookies found in the parsed data, specified by |index|
  // between 0 and |GetNumCookies| - 1. If the cookie doesn't exist, return
  // NULL.
  const net::CanonicalCookie* GetCookie(int index) const;

  // Get one of the local storage entries found in the parsed data, specified
  // by |index| between 0 and |GetNumLocalStorageEntries| - 1. If the local
  // storage entry doesn't exist, return NULL.
  const LocalStorageEntry* GetLocalStorageEntry(int index) const;

  static bool IsUpgradeData(const char* data, int size);

 private:
  // Process the parsed values and populate |cookies_| and
  // |local_storage_entries_|.
  void ProcessValues(const base::DictionaryValue* dictionary);

  // Add an entry to |cookies_| if |cookie| encodes a valid cookie.
  void AddCookieIfValid(const base::DictionaryValue* cookie);

  // Add an entry to |local_storage_entries_| if |local_storage_entry| encodes a
  // valid local storage entry.
  void AddLocalStorageEntryIfValid(
      const base::DictionaryValue* local_storage_entry);

  // Alerts porters that expiration is required.
  virtual void OnNullExpiration() {
    NOTREACHED() << "Cookies must have a specified expiration, "
                    "as there is no reasonable default.";
  }

  std::vector<net::CanonicalCookie> cookies_;
  std::vector<LocalStorageEntry> local_storage_entries_;
};

}  // namespace upgrade
}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_UPGRADE_UPGRADE_READER_H_
