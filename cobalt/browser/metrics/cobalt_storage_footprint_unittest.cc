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

#include "cobalt/browser/metrics/cobalt_storage_footprint.h"

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

namespace {

struct Rule {
  StorageRoot root;
  std::string_view path;
  StorageBucket bucket;
};

constexpr Rule kRules[] = {
#define COBALT_STORAGE_BUCKET(name)
#define COBALT_STORAGE_PATH(root, path, bucket) \
  {StorageRoot::k##root, path, StorageBucket::k##bucket},
#include "cobalt/browser/metrics/cobalt_storage_buckets.inc"
#undef COBALT_STORAGE_PATH
#undef COBALT_STORAGE_BUCKET
};

constexpr StorageBucket kAllBuckets[] = {
#define COBALT_STORAGE_BUCKET(name) StorageBucket::k##name,
#define COBALT_STORAGE_PATH(root, path, bucket)
#include "cobalt/browser/metrics/cobalt_storage_buckets.inc"
#undef COBALT_STORAGE_PATH
#undef COBALT_STORAGE_BUCKET
};

size_t Index(StorageBucket bucket) {
  return static_cast<size_t>(bucket);
}

size_t Index(StorageRoot root) {
  return static_cast<size_t>(root);
}

int64_t AllocatedBytes(const base::FilePath& path) {
  base::stat_wrapper_t st;
  EXPECT_EQ(base::File::Lstat(path, &st), 0) << path;
  return static_cast<int64_t>(st.st_blocks) * 512;
}

// Builds a directory tree and records the expected footprint of every entry.
class TestTree {
 public:
  explicit TestTree(const base::FilePath& base) : base_(base) {}

  // Creates the directory |relative| (which may already exist) and records it
  // in |bucket|.
  void AddDir(std::string_view relative, StorageBucket bucket) {
    const base::FilePath path = Path(relative);
    ASSERT_TRUE(base::CreateDirectory(path));
    entries_.push_back({path, bucket, /*is_file=*/false, 0});
  }

  // Creates a file of |size| bytes and records it in |bucket|.
  void AddFile(std::string_view relative, int size, StorageBucket bucket) {
    const base::FilePath path = Path(relative);
    ASSERT_TRUE(base::WriteFile(path, std::string(size, 'x')));
    entries_.push_back({path, bucket, /*is_file=*/true, size});
  }

  base::FilePath Path(std::string_view relative) const {
    return relative.empty() ? base_ : base_.Append(relative);
  }

  // Expected allocated bytes of every recorded entry under |root| (inclusive).
  int64_t AllocatedUnder(const base::FilePath& root) const {
    int64_t total = 0;
    for (const Entry& entry : entries_) {
      if (entry.path == root || root.IsParent(entry.path)) {
        total += AllocatedBytes(entry.path);
      }
    }
    return total;
  }

  int64_t LogicalUnder(const base::FilePath& root) const {
    int64_t total = 0;
    for (const Entry& entry : entries_) {
      if (entry.is_file && root.IsParent(entry.path)) {
        total += entry.size;
      }
    }
    return total;
  }

  int64_t FilesUnder(const base::FilePath& root) const {
    int64_t count = 0;
    for (const Entry& entry : entries_) {
      if (entry.is_file && root.IsParent(entry.path)) {
        ++count;
      }
    }
    return count;
  }

  std::array<int64_t, kStorageBucketCount> BucketBytes() const {
    std::array<int64_t, kStorageBucketCount> bytes = {};
    for (const Entry& entry : entries_) {
      bytes[Index(entry.bucket)] += AllocatedBytes(entry.path);
    }
    return bytes;
  }

  size_t entry_count() const { return entries_.size(); }

 private:
  struct Entry {
    base::FilePath path;
    StorageBucket bucket;
    bool is_file;
    int64_t size;
  };

  const base::FilePath base_;
  std::vector<Entry> entries_;
};

void ExpectBucketBytes(const StorageFootprint& footprint,
                       const std::array<int64_t, kStorageBucketCount>& bytes) {
  for (StorageBucket bucket : kAllBuckets) {
    EXPECT_EQ(footprint.bucket_bytes(bucket), bytes[Index(bucket)])
        << GetStorageBucketName(bucket);
  }
}

}  // namespace

TEST(CobaltStorageFootprintTest, Names) {
  EXPECT_EQ(kStorageBucketCount, 23u);
  EXPECT_EQ(GetStorageBucketName(StorageBucket::kCookies), "Cookies");
  EXPECT_EQ(GetStorageBucketName(StorageBucket::kLocalStorage), "LocalStorage");
  EXPECT_EQ(GetStorageBucketName(StorageBucket::kOtherAppData), "OtherAppData");
  EXPECT_EQ(GetStorageRootName(StorageRoot::kUserData), "UserData");
  EXPECT_EQ(GetStorageRootName(StorageRoot::kDirCache), "DirCache");
  EXPECT_EQ(GetStorageRootName(StorageRoot::kAppData), "AppData");
}

// Every rule is reachable: no earlier rule shadows its path, its contents, or
// its SQLite companion files.
TEST(CobaltStorageFootprintTest, ClassifiesEveryRule) {
  for (const Rule& rule : kRules) {
    if (rule.path.empty()) {
      continue;
    }
    const std::string path(rule.path);
    for (const std::string& candidate :
         {path, path + "/child", path + "/a/b", path + "-journal",
          path + "-wal", path + "-shm"}) {
      EXPECT_EQ(ClassifyStoragePath(rule.root, candidate), rule.bucket)
          << candidate << " under " << GetStorageRootName(rule.root);
    }
  }
}

TEST(CobaltStorageFootprintTest, ClassifiesPaths) {
  constexpr StorageRoot kUserData = StorageRoot::kUserData;
  constexpr StorageRoot kDirCache = StorageRoot::kDirCache;
  constexpr StorageRoot kAppData = StorageRoot::kAppData;
  const struct {
    StorageRoot root;
    std::string_view path;
    StorageBucket bucket;
  } kCases[] = {
      // The root directories themselves.
      {kUserData, "", StorageBucket::kOtherUserData},
      {kDirCache, "", StorageBucket::kOtherCache},
      {kAppData, "", StorageBucket::kOtherAppData},
      // UserData.
      {kUserData, "Cookies-journal", StorageBucket::kCookies},
      {kUserData, "Network/Cookies-journal", StorageBucket::kCookies},
      {kUserData, "Network/TransportSecurity", StorageBucket::kNetworkState},
      {kUserData, "Local Storage/leveldb/000003.log",
       StorageBucket::kLocalStorage},
      {kUserData, "WebStorage/QuotaManager-journal",
       StorageBucket::kQuotaManager},
      {kUserData, "DIPS-wal", StorageBucket::kDIPS},
      {kUserData, "Cache/Cache_Data/index", StorageBucket::kCleanupTargets},
      {kUserData, "old_Code Cache_000/js/index",
       StorageBucket::kCleanupTargets},
      {kUserData, "SharedStorage-wal", StorageBucket::kCleanupTargets},
      {kUserData, "BrowserStabilityMetrics/1.pma",
       StorageBucket::kStabilityPma},
      // Only whole path components and companion suffixes match.
      {kUserData, "Cookiesx", StorageBucket::kOtherUserData},
      {kUserData, "Cookies-journalx", StorageBucket::kOtherUserData},
      {kUserData, "Cookies-wal/x", StorageBucket::kOtherUserData},
      {kUserData, ".org.chromium.Chromium.abc", StorageBucket::kOtherUserData},
      // DirCache.
      {kDirCache, "Cache/Cache_Data/index", StorageBucket::kHttpCache},
      {kDirCache, "Code Cache/js/index-dir/the-real-index",
       StorageBucket::kCodeCacheJs},
      {kDirCache, "Code Cache/wasm/abc_0", StorageBucket::kCodeCacheWasm},
      {kDirCache, "Code Cache", StorageBucket::kOtherCache},
      {kDirCache, "ShaderCache/data_0", StorageBucket::kShaderCache},
      {kDirCache, "crashpad/pending/1.dmp", StorageBucket::kCrashpad},
      {kDirCache, "Experiment Config", StorageBucket::kOtherCache},
      // AppData.
      {kAppData, "app_content_shell", StorageBucket::kOtherAppData},
      {kAppData, "app_content_shell/BrowserStabilityMetrics/1.pma",
       StorageBucket::kStabilityPma},
      {kAppData, "files/x", StorageBucket::kAndroidFiles},
      {kAppData, "no_backup/androidx.work.workdb-wal",
       StorageBucket::kAndroidNoBackup},
      {kAppData, "databases/a.db", StorageBucket::kAndroidDbAndPrefs},
      {kAppData, "shared_prefs/b.xml", StorageBucket::kAndroidDbAndPrefs},
      {kAppData, "code_cache/c", StorageBucket::kOtherAppData},
  };
  for (const auto& test_case : kCases) {
    EXPECT_EQ(ClassifyStoragePath(test_case.root, test_case.path),
              test_case.bucket)
        << test_case.path << " under " << GetStorageRootName(test_case.root);
  }
}

// AppData contains UserData and DirCache, as on Android. Every entry is
// counted once, in every root that contains it, and in the bucket of the
// innermost root.
TEST(CobaltStorageFootprintTest, AttributesNestedRoots) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  TestTree tree(temp_dir.GetPath());
  tree.AddDir("", StorageBucket::kOtherAppData);
  tree.AddDir("app_content_shell", StorageBucket::kOtherAppData);
  tree.AddDir("app_content_shell/BrowserStabilityMetrics",
              StorageBucket::kStabilityPma);
  tree.AddFile("app_content_shell/BrowserStabilityMetrics/1.pma", 8192,
               StorageBucket::kStabilityPma);
  tree.AddDir("app_content_shell/content_shell", StorageBucket::kOtherUserData);
  tree.AddFile("app_content_shell/content_shell/Cookies", 20480,
               StorageBucket::kCookies);
  tree.AddFile("app_content_shell/content_shell/Cookies-journal", 1000,
               StorageBucket::kCookies);
  tree.AddDir("app_content_shell/content_shell/Local Storage",
              StorageBucket::kLocalStorage);
  tree.AddDir("app_content_shell/content_shell/Local Storage/leveldb",
              StorageBucket::kLocalStorage);
  tree.AddFile("app_content_shell/content_shell/Local Storage/leveldb/3.log",
               5000, StorageBucket::kLocalStorage);
  tree.AddFile("app_content_shell/content_shell/Unknown", 100,
               StorageBucket::kOtherUserData);
  tree.AddDir("cache", StorageBucket::kOtherCache);
  tree.AddDir("cache/Cache", StorageBucket::kHttpCache);
  tree.AddDir("cache/Cache/Cache_Data", StorageBucket::kHttpCache);
  tree.AddFile("cache/Cache/Cache_Data/index", 3000, StorageBucket::kHttpCache);
  tree.AddDir("cache/Code Cache", StorageBucket::kOtherCache);
  tree.AddDir("cache/Code Cache/js", StorageBucket::kCodeCacheJs);
  tree.AddFile("cache/Code Cache/js/abc_0", 6000, StorageBucket::kCodeCacheJs);
  tree.AddFile("cache/Variations", 85, StorageBucket::kOtherCache);
  tree.AddDir("files", StorageBucket::kAndroidFiles);
  tree.AddFile("files/x", 2000, StorageBucket::kAndroidFiles);
  tree.AddDir("shared_prefs", StorageBucket::kAndroidDbAndPrefs);
  tree.AddFile("shared_prefs/p.xml", 700, StorageBucket::kAndroidDbAndPrefs);
  tree.AddFile("no_backup_file", 10, StorageBucket::kOtherAppData);

  const base::FilePath user_data = tree.Path("app_content_shell/content_shell");
  const base::FilePath dir_cache = tree.Path("cache");
  const base::FilePath app_data = tree.Path("");
  const StorageFootprint footprint = ComputeStorageFootprint(
      {.user_data = user_data, .dir_cache = dir_cache, .app_data = app_data});

  EXPECT_FALSE(footprint.truncated);
  EXPECT_EQ(footprint.entry_count, static_cast<int64_t>(tree.entry_count()));
  const std::pair<StorageRoot, base::FilePath> roots[] = {
      {StorageRoot::kUserData, user_data},
      {StorageRoot::kDirCache, dir_cache},
      {StorageRoot::kAppData, app_data},
  };
  for (const auto& [root, path] : roots) {
    SCOPED_TRACE(GetStorageRootName(root));
    const StorageFootprint::RootUsage& usage = footprint.root(root);
    EXPECT_TRUE(usage.measured);
    EXPECT_EQ(usage.allocated_bytes, tree.AllocatedUnder(path));
    EXPECT_EQ(usage.logical_bytes, tree.LogicalUnder(path));
    EXPECT_EQ(usage.file_count, tree.FilesUnder(path));
  }
  EXPECT_EQ(footprint.root(StorageRoot::kUserData).logical_bytes,
            20480 + 1000 + 5000 + 100);
  ExpectBucketBytes(footprint, tree.BucketBytes());

  int64_t bucket_total = 0;
  for (StorageBucket bucket : kAllBuckets) {
    EXPECT_TRUE(footprint.bucket_measured[Index(bucket)])
        << GetStorageBucketName(bucket);
    bucket_total += footprint.bucket_bytes(bucket);
  }
  EXPECT_EQ(bucket_total,
            footprint.root(StorageRoot::kAppData).allocated_bytes);
}

// Symbolic links are neither followed nor counted, and roots are compared by
// their resolved paths (e.g. Android's /data/user/0 links to /data/data).
TEST(CobaltStorageFootprintTest, DoesNotFollowSymbolicLinks) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath outside = temp_dir.GetPath().AppendASCII("outside");
  ASSERT_TRUE(base::CreateDirectory(outside));
  ASSERT_TRUE(
      base::WriteFile(outside.AppendASCII("big"), std::string(65536, 'x')));

  TestTree tree(temp_dir.GetPath().AppendASCII("app"));
  tree.AddDir("", StorageBucket::kOtherAppData);
  tree.AddDir("app_content_shell", StorageBucket::kOtherAppData);
  tree.AddDir("app_content_shell/content_shell", StorageBucket::kOtherUserData);
  tree.AddFile("app_content_shell/content_shell/Cookies", 4000,
               StorageBucket::kCookies);
  ASSERT_TRUE(base::CreateSymbolicLink(outside, tree.Path("lib")));
  ASSERT_TRUE(base::CreateSymbolicLink(
      outside.AppendASCII("big"),
      tree.Path("app_content_shell/content_shell/big_link")));
  const base::FilePath app_link = temp_dir.GetPath().AppendASCII("app_link");
  ASSERT_TRUE(base::CreateSymbolicLink(tree.Path(""), app_link));

  // AppData is given through a link, UserData through the real path.
  const base::FilePath user_data = tree.Path("app_content_shell/content_shell");
  const StorageFootprint footprint =
      ComputeStorageFootprint({.user_data = user_data, .app_data = app_link});

  EXPECT_EQ(footprint.entry_count, static_cast<int64_t>(tree.entry_count()));
  EXPECT_EQ(footprint.root(StorageRoot::kAppData).allocated_bytes,
            tree.AllocatedUnder(tree.Path("")));
  EXPECT_EQ(footprint.root(StorageRoot::kAppData).logical_bytes, 4000);
  EXPECT_EQ(footprint.root(StorageRoot::kUserData).allocated_bytes,
            tree.AllocatedUnder(user_data));
  EXPECT_FALSE(footprint.root(StorageRoot::kDirCache).measured);
  ExpectBucketBytes(footprint, tree.BucketBytes());
}

TEST(CobaltStorageFootprintTest, StopsAtMaxEntries) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(base::WriteFile(
        temp_dir.GetPath().AppendASCII("f" + base::NumberToString(i)), "x"));
  }
  const StorageRootPaths paths{.user_data = temp_dir.GetPath()};

  // The root directory and 5 files.
  StorageFootprint footprint = ComputeStorageFootprint(paths, 6);
  EXPECT_FALSE(footprint.truncated);
  EXPECT_EQ(footprint.entry_count, 6);

  footprint = ComputeStorageFootprint(paths, 3);
  EXPECT_TRUE(footprint.truncated);
  EXPECT_EQ(footprint.entry_count, 3);
}

TEST(CobaltStorageFootprintTest, OnlyMeasuresExistingRoots) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const StorageFootprint footprint = ComputeStorageFootprint(
      {.user_data = temp_dir.GetPath(),
       .dir_cache = temp_dir.GetPath().AppendASCII("missing")});

  EXPECT_TRUE(footprint.root(StorageRoot::kUserData).measured);
  EXPECT_FALSE(footprint.root(StorageRoot::kDirCache).measured);
  EXPECT_FALSE(footprint.root(StorageRoot::kAppData).measured);
  EXPECT_TRUE(footprint.bucket_measured[Index(StorageBucket::kCookies)]);
  // Also has a UserData rule.
  EXPECT_TRUE(footprint.bucket_measured[Index(StorageBucket::kStabilityPma)]);
  EXPECT_FALSE(footprint.bucket_measured[Index(StorageBucket::kHttpCache)]);
  EXPECT_FALSE(footprint.bucket_measured[Index(StorageBucket::kAndroidFiles)]);
  EXPECT_EQ(footprint.entry_count, 1);
}

TEST(CobaltStorageFootprintTest, RecordsHistograms) {
  StorageFootprint footprint;
  footprint.roots[Index(StorageRoot::kUserData)] = {.measured = true,
                                                    .allocated_bytes = 40960,
                                                    .logical_bytes = 30000,
                                                    .file_count = 3};
  footprint.roots[Index(StorageRoot::kDirCache)] = {.measured = true,
                                                    .allocated_bytes = 1023};
  footprint.bucket_measured[Index(StorageBucket::kCookies)] = true;
  footprint.bucket_allocated_bytes[Index(StorageBucket::kCookies)] = 24576;
  footprint.bucket_measured[Index(StorageBucket::kIndexedDB)] = true;
  // A value above the histogram maximum (4 GB) still records its KB.
  footprint.bucket_measured[Index(StorageBucket::kHttpCache)] = true;
  footprint.bucket_allocated_bytes[Index(StorageBucket::kHttpCache)] =
      int64_t{5} * 1024 * 1024 * 1024;

  const std::pair<StorageFootprintPhase, std::string_view> phases[] = {
      {StorageFootprintPhase::kStartup, "Startup"},
      {StorageFootprintPhase::kPostCleanup, "PostCleanup"},
      {StorageFootprintPhase::kUptime, "Uptime"},
  };
  for (const auto& [phase, name] : phases) {
    SCOPED_TRACE(name);
    const std::string prefix =
        "Cobalt.Storage.Footprint." + std::string(name) + ".";
    base::HistogramTester histogram_tester;
    RecordStorageFootprintHistograms(phase, footprint, base::Milliseconds(12));

    histogram_tester.ExpectUniqueSample(prefix + "UserData.SizeKB", 40, 1);
    histogram_tester.ExpectUniqueSample(prefix + "UserData.FileCount", 3, 1);
    histogram_tester.ExpectUniqueSample(prefix + "DirCache.SizeKB", 0, 1);
    histogram_tester.ExpectUniqueSample(prefix + "DirCache.FileCount", 0, 1);
    histogram_tester.ExpectTotalCount(prefix + "AppData.SizeKB", 0);
    histogram_tester.ExpectTotalCount(prefix + "AppData.FileCount", 0);
    histogram_tester.ExpectUniqueSample(prefix + "Bucket.Cookies.SizeKB", 24,
                                        1);
    // Empty buckets of measured roots record 0.
    histogram_tester.ExpectUniqueSample(prefix + "Bucket.IndexedDB.SizeKB", 0,
                                        1);
    histogram_tester.ExpectUniqueSample(prefix + "Bucket.HttpCache.SizeKB",
                                        5 * 1024 * 1024, 1);
    histogram_tester.ExpectTotalCount(prefix + "Bucket.AndroidFiles.SizeKB", 0);
    histogram_tester.ExpectUniqueTimeSample(prefix + "WalkDuration",
                                            base::Milliseconds(12), 1);
    histogram_tester.ExpectUniqueSample(
        "Cobalt.Storage.Footprint.WalkTruncated", false, 1);
  }
}

TEST(CobaltStorageFootprintTest, TruncatedWalkRecordsNoSizes) {
  StorageFootprint footprint;
  footprint.roots[Index(StorageRoot::kUserData)] = {.measured = true,
                                                    .allocated_bytes = 4096};
  footprint.bucket_measured[Index(StorageBucket::kCookies)] = true;
  footprint.truncated = true;

  base::HistogramTester histogram_tester;
  RecordStorageFootprintHistograms(StorageFootprintPhase::kUptime, footprint,
                                   base::Seconds(3));

  histogram_tester.ExpectUniqueSample("Cobalt.Storage.Footprint.WalkTruncated",
                                      true, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Cobalt.Storage.Footprint.Uptime.WalkDuration", base::Seconds(3), 1);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.Footprint.Uptime.UserData.SizeKB", 0);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.Footprint.Uptime.Bucket.Cookies.SizeKB", 0);
}

// CleanupTargets is exactly what kCobaltStorageCleanup deletes: after the
// cleanup it is empty and every other bucket is unchanged.
TEST(CobaltStorageFootprintTest, CleanupDeletesExactlyCleanupTargets) {
  // Holds the deletion of renamed cache directories, which
  // disk_cache::CleanupDirectorySync() posts to the thread pool.
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED);
  base::ScopedTempDir user_data_dir;
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath& user_data = user_data_dir.GetPath();

  // The cleanup deletes these as standalone files, with SQLite companion files
  // for the databases; it deletes the other CleanupTargets as directories.
  const std::map<std::string_view, bool> kCleanupTargetFiles = {
      {"SCT Auditing Pending Reports", /*is_sqlite=*/false},
      {"Trust Tokens", /*is_sqlite=*/true},
      {"SharedStorage", /*is_sqlite=*/true},
  };
  for (const Rule& rule : kRules) {
    // The cleanup creates the old_<name>_000 directories itself.
    if (rule.root != StorageRoot::kUserData || rule.path.empty() ||
        rule.path.starts_with("old_")) {
      continue;
    }
    const base::FilePath path = user_data.Append(rule.path);
    const auto file = kCleanupTargetFiles.find(rule.path);
    if (rule.bucket == StorageBucket::kCleanupTargets &&
        file != kCleanupTargetFiles.end()) {
      ASSERT_TRUE(base::WriteFile(path, "legacy"));
      if (file->second) {
        for (std::string_view suffix : {"-journal", "-wal", "-shm"}) {
          ASSERT_TRUE(base::WriteFile(
              base::FilePath(path.value() + std::string(suffix)), "legacy"));
        }
      }
    } else {
      ASSERT_TRUE(base::CreateDirectory(path));
      ASSERT_TRUE(base::WriteFile(path.AppendASCII("data"), "data"));
    }
  }
  ASSERT_TRUE(base::WriteFile(user_data.AppendASCII("Unknown"), "data"));

  const StorageRootPaths paths{.user_data = user_data};
  const StorageFootprint before = ComputeStorageFootprint(paths);
  ASSERT_GT(before.bucket_bytes(StorageBucket::kCleanupTargets), 0);

  DeleteOrphanedUserDataCacheDirectories(user_data, cache_dir.GetPath());
  DeleteUnusedChromiumStoragePaths(user_data);

  // Cache directories are renamed to old_<name>_000 until the posted task
  // deletes them; they still belong to CleanupTargets.
  const StorageFootprint renamed = ComputeStorageFootprint(paths);
  EXPECT_TRUE(base::PathExists(user_data.AppendASCII("old_Cache_000")));
  for (StorageBucket bucket : kAllBuckets) {
    if (bucket != StorageBucket::kCleanupTargets) {
      EXPECT_EQ(renamed.bucket_bytes(bucket), before.bucket_bytes(bucket))
          << GetStorageBucketName(bucket);
    }
  }

  task_environment.RunUntilIdle();
  const StorageFootprint after = ComputeStorageFootprint(paths);
  EXPECT_EQ(after.bucket_bytes(StorageBucket::kCleanupTargets), 0);
  for (StorageBucket bucket : kAllBuckets) {
    if (bucket != StorageBucket::kCleanupTargets) {
      EXPECT_EQ(after.bucket_bytes(bucket), before.bucket_bytes(bucket))
          << GetStorageBucketName(bucket);
    }
  }
}

}  // namespace cobalt
