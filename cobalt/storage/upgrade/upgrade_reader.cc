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

#include "cobalt/storage/upgrade/upgrade_reader.h"

#include <memory>
#include <string>

#include "base/basictypes.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "url/gurl.h"

namespace cobalt {
namespace storage {
namespace upgrade {

namespace {

// The header at the start of upgrade data.
const int kHeaderSize = 4;
const char kHeader[] = "UPG0";

// Used as a sanity check.
const int kMaxUpgradeDataSize = 10 * 1024 * 1024;

// Deseralize a time value encoded as a possibly empty ASCII decimal string.
base::Time StringToTime(const std::string& time_as_string,
                        const base::Time& default_time) {
  if (!time_as_string.empty()) {
    int64 time_as_int64;
    if (base::StringToInt64(time_as_string, &time_as_int64)) {
      return base::Time::FromInternalValue(time_as_int64);
    }
  }

  return default_time;
}

base::Time GetSerializedTime(const base::DictionaryValue* dictionary,
                             const std::string& field_name,
                             const base::Time& default_time) {
  std::string time_string;
  dictionary->GetString(field_name, &time_string);
  return StringToTime(time_string, default_time);
}

// Get a list contained in a dictionary.
const base::ListValue* GetList(const base::DictionaryValue* dictionary,
                               const std::string& list_name) {
  if (!dictionary) {
    return NULL;
  }

  const base::ListValue* list = NULL;
  if (!dictionary->GetList(list_name, &list)) {
    return NULL;
  }

  return list;
}

// Get the size of a list contained in a dictionary.
int GetListSize(const base::DictionaryValue* dictionary,
                const std::string& list_name) {
  const base::ListValue* list = GetList(dictionary, list_name);
  if (!list) {
    return 0;
  }

  return static_cast<int>(list->GetSize());
}

// Get a dictionary in a list contained in a dictionary.
const base::DictionaryValue* GetListItem(
    const base::DictionaryValue* dictionary, const std::string& list_name,
    int index) {
  const base::ListValue* list = GetList(dictionary, list_name);
  if (!list) {
    return NULL;
  }

  if (index < 0 || index >= static_cast<int>(list->GetSize())) {
    return NULL;
  }

  const base::DictionaryValue* list_item = NULL;
  if (!list->GetDictionary(static_cast<size_t>(index), &list_item)) {
    return NULL;
  }

  return list_item;
}

}  // namespace

void UpgradeReader::Parse(const char* data, int size) {
  DCHECK(data);
  DCHECK_GE(size, kHeaderSize);
  DCHECK_LE(size, kMaxUpgradeDataSize);

  // Check the header and offset the data.
  DCHECK(IsUpgradeData(data, size));
  data += kHeaderSize;
  size -= kHeaderSize;

  base::JSONReader json_reader;
  std::unique_ptr<base::Value> parsed(
      json_reader.ReadToValue(std::string(data, static_cast<size_t>(size))));
  base::DictionaryValue* valid_dictionary = NULL;
  if (parsed) {
    parsed->GetAsDictionary(&valid_dictionary);
  }

  if (valid_dictionary) {
    ProcessValues(valid_dictionary);
  } else {
    DLOG(ERROR) << "Cannot parse upgrade save data: "
                << base::JSONReader::ErrorCodeToString(
                       json_reader.error_code());
  }
}

int UpgradeReader::GetNumCookies() const {
  return static_cast<int>(cookies_.size());
}

int UpgradeReader::GetNumLocalStorageEntries() const {
  return static_cast<int>(local_storage_entries_.size());
}

const net::CanonicalCookie* UpgradeReader::GetCookie(int index) const {
  if (index >= 0 && index < static_cast<int>(cookies_.size())) {
    return &cookies_[static_cast<size_t>(index)];
  } else {
    return static_cast<const net::CanonicalCookie*>(NULL);
  }
}

void UpgradeReader::AddCookieIfValid(const base::DictionaryValue* cookie) {
  DCHECK(cookie);

  // Required attributes.
  std::string url;
  std::string name;
  std::string value;
  cookie->GetString("url", &url);
  cookie->GetString("name", &name);
  cookie->GetString("value", &value);
  if (url.empty() || name.empty() || value.empty()) {
    return;
  }

  // Optional attributes with default values.
  const GURL gurl(url);
  const std::string host = gurl.host();
  std::string domain = host.empty() ? "" : host.substr(host.find("."));
  cookie->GetString("domain", &domain);
  std::string path = "/";
  cookie->GetString("path", &path);
  base::Time creation =
      GetSerializedTime(cookie, "creation", base::Time::Now());
  base::Time expiration = GetSerializedTime(cookie, "expiration", base::Time());
  base::Time last_access =
      GetSerializedTime(cookie, "last_access", base::Time::Now());
  bool http_only = false;
  cookie->GetBoolean("http_only", &http_only);

  // Attributes not defined in upgrade data.
  const std::string mac_key;
  const std::string mac_algorithm;
  const bool secure = true;

  if (expiration.is_null()) {
    LOG(ERROR) << "\"" << name
               << "\" cookie can not be upgraded due to null expiration.";
    OnNullExpiration();
  }
  LOG_IF(ERROR, expiration < base::Time::UnixEpoch())
      << "\"" << name.c_str() << "\" upgrade cookie expiration is "
      << base::TimeFormatFriendlyDateAndTime(expiration).c_str();

  cookies_.push_back(net::CanonicalCookie(
      name, value, domain, path, creation, expiration, last_access, secure,
      http_only, net::CookieSameSite(), net::COOKIE_PRIORITY_DEFAULT));
}

const UpgradeReader::LocalStorageEntry* UpgradeReader::GetLocalStorageEntry(
    int index) const {
  if (index >= 0 && index < static_cast<int>(local_storage_entries_.size())) {
    return &local_storage_entries_[static_cast<size_t>(index)];
  } else {
    return static_cast<const LocalStorageEntry*>(NULL);
  }
}

void UpgradeReader::AddLocalStorageEntryIfValid(
    const base::DictionaryValue* local_storage_entry) {
  DCHECK(local_storage_entry);
  std::string key;
  std::string value;
  if (!local_storage_entry->GetString("key", &key) ||
      !local_storage_entry->GetString("value", &value) || key.empty() ||
      value.empty()) {
    return;
  }

  local_storage_entries_.push_back(LocalStorageEntry(key, value));
}

// static
bool UpgradeReader::IsUpgradeData(const char* data, int size) {
  return size >= kHeaderSize && memcmp(data, kHeader, kHeaderSize) == 0;
}

void UpgradeReader::ProcessValues(const base::DictionaryValue* dictionary) {
  DCHECK(dictionary);

  const int num_cookies = GetListSize(dictionary, "cookies");
  for (int n = 0; n < num_cookies; n++) {
    const base::DictionaryValue* cookie = GetListItem(dictionary, "cookies", n);
    if (cookie) {
      AddCookieIfValid(cookie);
    }
  }

  const int num_local_storage_entries =
      GetListSize(dictionary, "local_storage_entries");
  for (int n = 0; n < num_local_storage_entries; n++) {
    const base::DictionaryValue* local_storage_entry =
        GetListItem(dictionary, "local_storage_entries", n);
    if (local_storage_entry) {
      AddLocalStorageEntryIfValid(local_storage_entry);
    }
  }
}

}  // namespace upgrade
}  // namespace storage
}  // namespace cobalt
