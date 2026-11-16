// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/c27_storage_cleanup.h"

#include <unistd.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace storage {

namespace {

// GetC27AppKey() of https://www.youtube.com/tv and of another app.
const char kAppKey[] = "aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHY=";
const char kOtherAppKey[] = "aHR0cHM6Ly9tdXNpYy55b3V0dWJlLmNvbS8=";

class C27StorageCleanupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(home_.CreateUniqueTempDir());
    // The Linux reference layout: $HOME/.cobalt_storage, $HOME/.cache/cobalt
    // and (Cobalt 27 only) $HOME/.cobalt_files.
    storage_dir_ = home_.GetPath().Append(".cobalt_storage");
    cache_dir_ = home_.GetPath().Append(".cache").Append("cobalt");
    files_dir_ = home_.GetPath().Append(kC27FilesDirectoryName);
    ASSERT_TRUE(base::CreateDirectory(storage_dir_));
    ASSERT_TRUE(base::CreateDirectory(cache_dir_));
  }

  void WriteFile(const base::FilePath& path, const std::string& content) {
    ASSERT_TRUE(base::CreateDirectory(path.DirName()));
    ASSERT_EQ(static_cast<int>(content.size()),
              base::WriteFile(path, content.data(),
                              static_cast<int>(content.size())));
  }

  int64_t Cleanup(const std::string& app_key = kAppKey) {
    return DeleteOrphanedC27Storage(storage_dir_, cache_dir_, app_key);
  }

  base::ScopedTempDir home_;
  base::FilePath storage_dir_;
  base::FilePath cache_dir_;
  base::FilePath files_dir_;
};

TEST(C27AppKeyTest, MatchesCobalt27) {
  // The directory names a Cobalt 27 linux-x64x11 build created for these
  // --url values.
  EXPECT_EQ(kAppKey, GetC27AppKey(""));
  EXPECT_EQ(kAppKey, GetC27AppKey("https://www.youtube.com/tv"));
  EXPECT_EQ("aHR0cHM6Ly8xMjcuMC4wLjE6ODQ2MS90di8=",
            GetC27AppKey("https://127.0.0.1:8461/tv/?mode=check&tag=a"));
  // Vectors from Cobalt 27's starboard/common/app_key_test.cc.
  EXPECT_EQ("aHR0cDovL2ZvbzoyMS9iYXI7cGFy",
            GetC27AppKey("http://user:pass@foo:21/bar;par?b#c"));
  EXPECT_EQ("aHR0cDpmb28uY29t", GetC27AppKey("http:foo.com"));
  EXPECT_EQ("ZmlsZTovLy9mb28uaHRtbA==",
            GetC27AppKey("file:///foo.html?q=y#ref"));
  // The text is not canonicalized the way GURL would.
  EXPECT_EQ("aHR0cHM6Ly9leGFtcGxlLmNvbQ==",
            GetC27AppKey("https://example.com"));
  // '+' and '/' of plain base64 become '-' and '_'.
  EXPECT_EQ("aHR0cDovL2Evfn5-", GetC27AppKey("http://a/~~~"));
  EXPECT_EQ("-_A=", GetC27AppKey("\xFB\xF0"));
}

TEST(C27AppKeyTest, KeepsOnlyTheEndOfLongKeys) {
  const std::string key =
      GetC27AppKey("https://www.youtube.com/tv/some/much/longer/path");
  EXPECT_EQ(static_cast<size_t>(kSbFileMaxName) - 24, key.size());
  if (kSbFileMaxName == 64) {
    EXPECT_EQ("ZS5jb20vdHYvc29tZS9tdWNoL2xvbmdlci9wYXRo", key);
  }
}

TEST_F(C27StorageCleanupTest, NothingToDelete) {
  WriteFile(cache_dir_.Append("settings.json"), "{}");
  EXPECT_EQ(0, Cleanup());
  EXPECT_TRUE(base::PathExists(cache_dir_.Append("settings.json")));
  EXPECT_TRUE(base::DirectoryExists(storage_dir_));
  EXPECT_FALSE(base::PathExists(files_dir_));
}

TEST_F(C27StorageCleanupTest, DeletesProfileAndEmptyFilesDirectory) {
  const base::FilePath profile = files_dir_.Append(kAppKey);
  WriteFile(profile.Append("Cookies"), std::string(100, 'c'));
  WriteFile(profile.Append("Local Storage").Append("leveldb").Append("1.log"),
            std::string(50, 'l'));
  ASSERT_TRUE(base::CreateDirectory(profile.Append("blob_storage")));

  EXPECT_EQ(150, Cleanup());
  EXPECT_FALSE(base::PathExists(profile));
  EXPECT_FALSE(base::PathExists(files_dir_));
  EXPECT_TRUE(base::DirectoryExists(storage_dir_));
  // A second run finds nothing.
  EXPECT_EQ(0, Cleanup());
}

TEST_F(C27StorageCleanupTest, DeletesProfileOfLongUrl) {
  const std::string key =
      GetC27AppKey("https://www.youtube.com/tv/some/much/longer/path");
  WriteFile(files_dir_.Append(key).Append("Cookies"), "long");
  EXPECT_EQ(4, Cleanup(key));
  EXPECT_FALSE(base::PathExists(files_dir_));
}

TEST_F(C27StorageCleanupTest, KeepsProfilesOfOtherApps) {
  WriteFile(files_dir_.Append(kAppKey).Append("Cookies"), "mine");
  const base::FilePath other =
      files_dir_.Append(kOtherAppKey).Append("Cookies");
  WriteFile(other, "theirs");

  EXPECT_EQ(4, Cleanup());
  EXPECT_FALSE(base::PathExists(files_dir_.Append(kAppKey)));
  EXPECT_TRUE(base::PathExists(other));
}

TEST_F(C27StorageCleanupTest, DeletesDeviceCacheDirectoryWithMarker) {
  // Starboard device builds of Cobalt 27: <cache>/<app_key>/...
  const base::FilePath c27_cache = cache_dir_.Append(kAppKey);
  WriteFile(c27_cache.Append(kC27MigrationMarkerFileName), "0-0-0");
  WriteFile(c27_cache.Append(kC27LegacyRecordDigestFileName), "abcd");
  WriteFile(c27_cache.Append("Cache").Append("Cache_Data").Append("f_1"),
            std::string(1000, 'x'));
  // Cobalt 25's own cache entries, including its per-app splash screen.
  WriteFile(cache_dir_.Append("settings.json"), "{}");
  WriteFile(cache_dir_.Append("compiled_js").Append("1"), "js");
  const base::FilePath splash =
      cache_dir_.Append("splash_screen").Append(kAppKey).Append("splash");
  WriteFile(splash, "html");

  EXPECT_EQ(1009, Cleanup());
  EXPECT_FALSE(base::PathExists(c27_cache));
  EXPECT_TRUE(base::PathExists(cache_dir_.Append("settings.json")));
  EXPECT_TRUE(base::PathExists(cache_dir_.Append("compiled_js").Append("1")));
  EXPECT_TRUE(base::PathExists(splash));
}

TEST_F(C27StorageCleanupTest, DeletesLinuxDevBuildMarkerFilesOnly) {
  // linux-x64x11 builds of Cobalt 27 use $HOME/.cache, the parent of Cobalt
  // 25's cache directory, which other programs share.
  const base::FilePath xdg_cache = cache_dir_.DirName();
  WriteFile(xdg_cache.Append(kC27MigrationMarkerFileName), "0-0-0");
  WriteFile(xdg_cache.Append(kC27LegacyRecordDigestFileName), "abcd");
  WriteFile(xdg_cache.Append("Cache").Append("Cache_Data").Append("f_1"), "x");
  WriteFile(xdg_cache.Append("other_program").Append("data"), "keep");

  EXPECT_EQ(9, Cleanup());
  EXPECT_FALSE(base::PathExists(xdg_cache.Append(kC27MigrationMarkerFileName)));
  EXPECT_FALSE(
      base::PathExists(xdg_cache.Append(kC27LegacyRecordDigestFileName)));
  EXPECT_TRUE(base::PathExists(xdg_cache.Append("Cache")));
  EXPECT_TRUE(base::PathExists(xdg_cache.Append("other_program")));
}

TEST_F(C27StorageCleanupTest, KeepsDirectoryNamedLikeTheMarker) {
  // In the shared $HOME/.cache only a file may be removed.
  const base::FilePath data =
      cache_dir_.DirName().Append(kC27MigrationMarkerFileName).Append("data");
  WriteFile(data, "keep");
  EXPECT_EQ(0, Cleanup());
  EXPECT_TRUE(base::PathExists(data));
}

TEST_F(C27StorageCleanupTest, RejectsInvalidAppKeys) {
  WriteFile(files_dir_.Append(kAppKey).Append("Cookies"), "mine");
  WriteFile(home_.GetPath().Append("precious"), "keep");
  for (const char* bad_key : {"", ".", "..", "../precious", "a/b"}) {
    EXPECT_EQ(-1, Cleanup(bad_key)) << "key: " << bad_key;
  }
  EXPECT_TRUE(base::PathExists(files_dir_.Append(kAppKey).Append("Cookies")));
  EXPECT_TRUE(base::PathExists(home_.GetPath().Append("precious")));
  EXPECT_TRUE(base::DirectoryExists(storage_dir_));
  EXPECT_TRUE(base::DirectoryExists(cache_dir_));
}

TEST_F(C27StorageCleanupTest, EmptyDirectoriesAreSkipped) {
  WriteFile(files_dir_.Append(kAppKey).Append("Cookies"), "mine");
  WriteFile(cache_dir_.Append(kAppKey).Append("f"), "cache");

  EXPECT_EQ(
      0, DeleteOrphanedC27Storage(base::FilePath(), base::FilePath(), kAppKey));
  // No kSbSystemPathStorageDirectory: only the cache side is cleaned.
  EXPECT_EQ(5, DeleteOrphanedC27Storage(base::FilePath(), cache_dir_, kAppKey));
  EXPECT_TRUE(base::PathExists(files_dir_.Append(kAppKey)));
  // No kSbSystemPathCacheDirectory: only the profile is cleaned.
  EXPECT_EQ(4,
            DeleteOrphanedC27Storage(storage_dir_, base::FilePath(), kAppKey));
  EXPECT_FALSE(base::PathExists(files_dir_));
}

// The Evergreen loader has no chdir() or symlink().
#if !SB_IS(EVERGREEN)
TEST_F(C27StorageCleanupTest, RelativeDirectoriesAreSkipped) {
  WriteFile(files_dir_.Append(kAppKey).Append("Cookies"), "mine");
  WriteFile(cache_dir_.Append(kAppKey).Append("f"), "cache");
  WriteFile(cache_dir_.DirName().Append(kC27MigrationMarkerFileName), "0-0-0");

  // From $HOME these relative paths name the real directories.
  char previous_dir[4096];
  ASSERT_TRUE(getcwd(previous_dir, sizeof(previous_dir)));
  ASSERT_EQ(0, chdir(home_.GetPath().value().c_str()));
  const int64_t bytes = DeleteOrphanedC27Storage(
      base::FilePath(".cobalt_storage"),
      base::FilePath(".cache").Append("cobalt"), kAppKey);
  ASSERT_EQ(0, chdir(previous_dir));

  EXPECT_EQ(0, bytes);
  EXPECT_TRUE(base::PathExists(files_dir_.Append(kAppKey).Append("Cookies")));
  EXPECT_TRUE(base::PathExists(cache_dir_.Append(kAppKey).Append("f")));
  EXPECT_TRUE(base::PathExists(
      cache_dir_.DirName().Append(kC27MigrationMarkerFileName)));
}

TEST_F(C27StorageCleanupTest, RemovesLinkedProfileWithoutFollowingIt) {
  const base::FilePath target = home_.GetPath().Append("elsewhere");
  WriteFile(target.Append("data"), "keep");
  ASSERT_TRUE(base::CreateDirectory(files_dir_));
  ASSERT_EQ(0, symlink(target.value().c_str(),
                       files_dir_.Append(kAppKey).value().c_str()));

  EXPECT_EQ(0, Cleanup());
  // The link is gone, so its directory was empty and is gone too.
  EXPECT_FALSE(base::PathExists(files_dir_));
  EXPECT_TRUE(base::PathExists(target.Append("data")));
}

TEST_F(C27StorageCleanupTest, DoesNotFollowLinksInsideTheProfile) {
  const base::FilePath target_dir = home_.GetPath().Append("elsewhere");
  const base::FilePath target_file = home_.GetPath().Append("precious");
  WriteFile(target_dir.Append("sub").Append("data"), "keep");
  WriteFile(target_file, "keep too");
  const base::FilePath profile = files_dir_.Append(kAppKey);
  const base::FilePath nested = profile.Append("Local Storage");
  WriteFile(nested.Append("1.log"), "log");
  ASSERT_EQ(0, symlink(target_dir.value().c_str(),
                       nested.Append("dir_link").value().c_str()));
  ASSERT_EQ(0, symlink(target_file.value().c_str(),
                       profile.Append("file_link").value().c_str()));
  ASSERT_EQ(0, symlink("/nonexistent/target",
                       profile.Append("dangling_link").value().c_str()));

  Cleanup();
  EXPECT_FALSE(base::PathExists(files_dir_));
  EXPECT_TRUE(base::PathExists(target_dir.Append("sub").Append("data")));
  EXPECT_TRUE(base::PathExists(target_file));
}
#endif  // !SB_IS(EVERGREEN)

}  // namespace
}  // namespace storage
}  // namespace cobalt
