// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/store_upgrade/upgrade.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "cobalt/storage/storage_constants.h"
#include "cobalt/storage/store/storage.pb.h"
#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace storage {
namespace store_upgrade {
namespace {

std::vector<uint8> ReadFile(const char* pathname) {
  base::FilePath file_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEST_DATA, &file_path));
  file_path = file_path.Append(pathname);
  std::string string_out;
  EXPECT_TRUE(base::ReadFileToString(file_path, &string_out));
  std::vector<uint8> buffer(string_out.begin(), string_out.end());
  return buffer;
}

void ValidateCookie(const cobalt::storage::Cookie& cookie,
                    const std::string& name, const std::string& value,
                    const std::string domain, const std::string& path,
                    int64 creation_time_us, const int64 expiration_time_us,
                    int64 last_access_time_us, bool http_only) {
  EXPECT_EQ(cookie.name(), name);
  EXPECT_EQ(cookie.value(), value);
  EXPECT_EQ(cookie.domain(), domain);
  EXPECT_EQ(cookie.path(), path);
  EXPECT_EQ(cookie.creation_time_us(), creation_time_us);
  EXPECT_EQ(cookie.expiration_time_us(), expiration_time_us);
  EXPECT_EQ(cookie.last_access_time_us(), last_access_time_us);
  EXPECT_EQ(cookie.secure(), false);
  EXPECT_EQ(cookie.http_only(), http_only);
}

void ValidateLocalStorageEntry(
    const cobalt::storage::LocalStorageEntry& local_storage_entry,
    const std::string& key, const std::string& value) {
  EXPECT_EQ(local_storage_entry.key(), key);
  EXPECT_EQ(local_storage_entry.value(), value);
}

TEST(StorageUpgradeTest, Upgrade) {
  std::vector<uint8> buffer =
      ReadFile("cobalt/storage/store_upgrade/testdata/test.storage");
  EXPECT_TRUE(IsUpgradeRequired(buffer));
  EXPECT_TRUE(UpgradeStore(&buffer));

  Storage storage;
  EXPECT_TRUE(storage.ParseFromArray(buffer.data() + kStorageHeaderSize,
                                     buffer.size() - kStorageHeaderSize));

  if (storage.cookies(0).name() == "foo") {
    ValidateCookie(storage.cookies(0), "foo", "bar", "localhost", "/",
                   13172275723286776, 13191233280286776, 13191233280286776,
                   false);
  } else {
    ValidateCookie(storage.cookies(1), "sessionid", "4242", "localhost", "/",
                   13172278221139558, 13191319680139558, 13172278221139558,
                   true);
  }

  EXPECT_EQ(storage.local_storages(0).serialized_origin(),
            "http://localhost:8000");
  if (storage.local_storages(0).local_storage_entries(0).key() == "foo") {
    ValidateLocalStorageEntry(
        storage.local_storages(0).local_storage_entries(0), "foo", "bar");
  } else {
    ValidateLocalStorageEntry(
        storage.local_storages(0).local_storage_entries(1), "data",
        "{\"mydata\": \"123\"}");
  }
}

}  // namespace
}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt
