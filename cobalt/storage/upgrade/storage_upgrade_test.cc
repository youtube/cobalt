// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <cstring>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/time.h"
#include "cobalt/storage/upgrade/upgrade_reader.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace storage {
namespace upgrade {

namespace {

using ::testing::StrictMock;

class MockUpgradeReader : public UpgradeReader {
 public:
  MOCK_METHOD0(OnNullExpiration, void());
};

void ReadFileToString(const char* pathname, std::string* string_out) {
  EXPECT_TRUE(pathname);
  EXPECT_TRUE(string_out);
  FilePath file_path;
  EXPECT_TRUE(PathService::Get(base::DIR_TEST_DATA, &file_path));
  file_path = file_path.Append(pathname);
  EXPECT_TRUE(file_util::ReadFileToString(file_path, string_out));
  const char* data = string_out->c_str();
  const int size = static_cast<int>(string_out->length());
  EXPECT_GT(size, 0);
  EXPECT_LE(size, 10 * 1024 * 1024);
  EXPECT_TRUE(UpgradeReader::IsUpgradeData(data, size));
}

void ValidateCookie(const net::CanonicalCookie* cookie, const std::string& url,
                    const std::string& name, const std::string& value,
                    const std::string domain, const std::string& path,
                    const base::Time& creation, const base::Time expiration,
                    bool http_only) {
  EXPECT_TRUE(cookie);
  EXPECT_EQ(cookie->Source(),
            net::CanonicalCookie::GetCookieSourceFromURL(GURL(url)));
  EXPECT_EQ(cookie->Name(), name);
  EXPECT_EQ(cookie->Value(), value);
  EXPECT_EQ(cookie->Domain(), domain);
  EXPECT_EQ(cookie->Path(), path);
  EXPECT_EQ((cookie->CreationDate() - creation).InSeconds(), 0);
  EXPECT_EQ((cookie->ExpiryDate() - expiration).InSeconds(), 0);
  EXPECT_EQ((cookie->LastAccessDate() - base::Time::Now()).InSeconds(), 0);
  EXPECT_EQ(cookie->IsSecure(), true);
  EXPECT_EQ(cookie->IsHttpOnly(), http_only);
}

void ValidateCookie(const net::CanonicalCookie* cookie, const std::string& url,
                    const std::string& name, const std::string& value) {
  const std::string host = GURL(url).host();
  const std::string domain = host.empty() ? "" : host.substr(host.find("."));
  const std::string path = "/";
  const base::Time creation = base::Time::Now();
  const base::Time expiration = base::Time();
  const bool http_only = false;
  ValidateCookie(cookie, url, name, value, domain, path, creation, expiration,
                 http_only);
}

void ValidateLocalStorageEntry(
    const UpgradeReader::LocalStorageEntry* local_storage_entry,
    const std::string& key, const std::string& value) {
  EXPECT_TRUE(local_storage_entry);
  EXPECT_EQ(local_storage_entry->key, key);
  EXPECT_EQ(local_storage_entry->value, value);
}

}  // namespace

TEST(StorageUpgradeTest, UpgradeMinimalCookie) {
  std::string file_contents;
  ReadFileToString("cobalt/storage/upgrade/testdata/minimal_cookie_v1.json",
                   &file_contents);
  StrictMock<MockUpgradeReader> upgrade_reader;

  // No expiration
  EXPECT_CALL(upgrade_reader, OnNullExpiration());

  upgrade_reader.Parse(file_contents.c_str(),
                       static_cast<int>(file_contents.length()));

  // 1 cookie.
  EXPECT_EQ(upgrade_reader.GetNumCookies(), 1);
  const net::CanonicalCookie* cookie = upgrade_reader.GetCookie(0);
  ValidateCookie(cookie, "https://www.example.com/", "cookie_name",
                 "cookie_value");
  EXPECT_FALSE(upgrade_reader.GetCookie(1));

  // 0 local storage entries.
  EXPECT_EQ(upgrade_reader.GetNumLocalStorageEntries(), 0);
  EXPECT_FALSE(upgrade_reader.GetLocalStorageEntry(0));
}

TEST(StorageUpgradeTest, UpgradeMinimalLocalStorageEntry) {
  std::string file_contents;
  ReadFileToString(
      "cobalt/storage/upgrade/testdata/minimal_local_storage_entry_v1.json",
      &file_contents);
  UpgradeReader upgrade_reader;
  upgrade_reader.Parse(file_contents.c_str(),
                       static_cast<int>(file_contents.length()));

  // 0 cookies.
  EXPECT_EQ(upgrade_reader.GetNumCookies(), 0);
  EXPECT_FALSE(upgrade_reader.GetCookie(0));

  // 1 local storage entry.
  EXPECT_EQ(upgrade_reader.GetNumLocalStorageEntries(), 1);
  const UpgradeReader::LocalStorageEntry* local_storage_entry =
      upgrade_reader.GetLocalStorageEntry(0);
  ValidateLocalStorageEntry(local_storage_entry, "key-1", "value-1");
  EXPECT_FALSE(upgrade_reader.GetLocalStorageEntry(1));
}

TEST(StorageUpgradeTest, UpgradeFullData) {
  std::string file_contents;
  ReadFileToString("cobalt/storage/upgrade/testdata/full_data_v1.json",
                   &file_contents);
  UpgradeReader upgrade_reader;
  upgrade_reader.Parse(file_contents.c_str(),
                       static_cast<int>(file_contents.length()));

  // 2 cookies.
  EXPECT_EQ(upgrade_reader.GetNumCookies(), 2);
  const net::CanonicalCookie* cookie = upgrade_reader.GetCookie(0);
  base::Time creation = base::Time::FromInternalValue(13119668760000000L);
  base::Time expiration = base::Time::FromInternalValue(13120000000000000L);
  ValidateCookie(cookie, "https://www.example.com/1", "cookie_name",
                 "cookie_value", ".example.com", "/1", creation, expiration,
                 true);
  cookie = upgrade_reader.GetCookie(1);
  creation = base::Time::FromInternalValue(13109668760000000L);
  expiration = base::Time::FromInternalValue(13110000000000000L);
  ValidateCookie(cookie, "https://www.somewhere.com/2", "cookie_name_2",
                 "cookie_value_2", ".somewhere.com", "/2", creation, expiration,
                 true);
  EXPECT_FALSE(upgrade_reader.GetCookie(2));

  // 2 local storage entries.
  EXPECT_EQ(upgrade_reader.GetNumLocalStorageEntries(), 2);
  const UpgradeReader::LocalStorageEntry* local_storage_entry =
      upgrade_reader.GetLocalStorageEntry(0);
  ValidateLocalStorageEntry(local_storage_entry, "key-1", "value-1");
  local_storage_entry = upgrade_reader.GetLocalStorageEntry(1);
  ValidateLocalStorageEntry(local_storage_entry, "key-2", "value-2");
  EXPECT_FALSE(upgrade_reader.GetLocalStorageEntry(2));
}

TEST(StorageUpgradeTest, UpgradeMissingFields) {
  std::string file_contents;
  ReadFileToString("cobalt/storage/upgrade/testdata/missing_fields_v1.json",
                   &file_contents);
  UpgradeReader upgrade_reader;
  upgrade_reader.Parse(file_contents.c_str(),
                       static_cast<int>(file_contents.length()));

  // 1 cookie with missing fields, 2 local storage entries with missing fields,
  // 1 valid local storage entry.
  EXPECT_EQ(upgrade_reader.GetNumCookies(), 0);
  EXPECT_FALSE(upgrade_reader.GetCookie(0));
  EXPECT_EQ(upgrade_reader.GetNumLocalStorageEntries(), 1);
  const UpgradeReader::LocalStorageEntry* local_storage_entry =
      upgrade_reader.GetLocalStorageEntry(0);
  ValidateLocalStorageEntry(local_storage_entry, "key-3", "value-3");
  EXPECT_FALSE(upgrade_reader.GetLocalStorageEntry(1));
}

TEST(StorageUpgradeTest, UpgradeMalformed) {
  std::string file_contents;
  ReadFileToString("cobalt/storage/upgrade/testdata/malformed_v1.json",
                   &file_contents);
  UpgradeReader upgrade_reader;
  upgrade_reader.Parse(file_contents.c_str(),
                       static_cast<int>(file_contents.length()));

  // No cookies or local storage entries available in malformed data.
  EXPECT_EQ(upgrade_reader.GetNumCookies(), 0);
  EXPECT_FALSE(upgrade_reader.GetCookie(0));
  EXPECT_EQ(upgrade_reader.GetNumLocalStorageEntries(), 0);
  EXPECT_FALSE(upgrade_reader.GetLocalStorageEntry(0));
}

TEST(StorageUpgradeTest, UpgradeExtraFields) {
  std::string file_contents;
  ReadFileToString("cobalt/storage/upgrade/testdata/extra_fields_v1.json",
                   &file_contents);
  StrictMock<MockUpgradeReader> upgrade_reader;

  // No expiration
  EXPECT_CALL(upgrade_reader, OnNullExpiration());

  upgrade_reader.Parse(file_contents.c_str(),
                       static_cast<int>(file_contents.length()));

  // 1 cookie, extra fields should be ignored.
  EXPECT_EQ(upgrade_reader.GetNumCookies(), 1);
  const net::CanonicalCookie* cookie = upgrade_reader.GetCookie(0);
  ValidateCookie(cookie, "https://www.example.com/", "cookie_name",
                 "cookie_value");
  EXPECT_FALSE(upgrade_reader.GetCookie(1));

  // 2 local storage entries, extra fields should be ignored.
  EXPECT_EQ(upgrade_reader.GetNumLocalStorageEntries(), 2);
  const UpgradeReader::LocalStorageEntry* local_storage_entry =
      upgrade_reader.GetLocalStorageEntry(0);
  ValidateLocalStorageEntry(local_storage_entry, "key-1", "value-1");
  local_storage_entry = upgrade_reader.GetLocalStorageEntry(1);
  ValidateLocalStorageEntry(local_storage_entry, "key-2", "value-2");
  EXPECT_FALSE(upgrade_reader.GetLocalStorageEntry(2));
}

}  // namespace upgrade
}  // namespace storage
}  // namespace cobalt
