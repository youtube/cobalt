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

#include <sys/stat.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/threading/scoped_blocking_call.h"

namespace cobalt {

namespace {

struct StoragePathRule {
  StorageRoot root;
  std::string_view path;
  StorageBucket bucket;
};

constexpr StoragePathRule kStoragePathRules[] = {
#define COBALT_STORAGE_BUCKET(name)
#define COBALT_STORAGE_PATH(root, path, bucket) \
  {StorageRoot::k##root, path, StorageBucket::k##bucket},
#include "cobalt/browser/metrics/cobalt_storage_buckets.inc"
#undef COBALT_STORAGE_PATH
#undef COBALT_STORAGE_BUCKET
};

constexpr std::string_view kStorageBucketNames[] = {
#define COBALT_STORAGE_BUCKET(name) #name,
#define COBALT_STORAGE_PATH(root, path, bucket)
#include "cobalt/browser/metrics/cobalt_storage_buckets.inc"
#undef COBALT_STORAGE_PATH
#undef COBALT_STORAGE_BUCKET
};
static_assert(std::size(kStorageBucketNames) == kStorageBucketCount);

constexpr std::string_view kStorageRootNames[] = {
    "UserData",
    "DirCache",
    "AppData",
};
static_assert(std::size(kStorageRootNames) == kStorageRootCount);

// Every root needs a rule with an empty path, which matches everything, so
// that every entry has a bucket.
constexpr bool HasCatchAllRule(StorageRoot root) {
  for (const StoragePathRule& rule : kStoragePathRules) {
    if (rule.root == root && rule.path.empty()) {
      return true;
    }
  }
  return false;
}
static_assert(HasCatchAllRule(StorageRoot::kUserData) &&
              HasCatchAllRule(StorageRoot::kDirCache) &&
              HasCatchAllRule(StorageRoot::kAppData));

// 1 KB to 4 GB, 100 exponential buckets. Empty buckets record 0 (underflow).
constexpr int kMinSizeKB = 1;
constexpr int kMaxSizeKB = 4 * 1024 * 1024;
constexpr int kSizeHistogramBuckets = 100;

// st_blocks is in 512-byte units on Linux, independent of st_blksize.
constexpr int64_t kStatBlockSizeBytes = 512;

constexpr std::string_view kSqliteCompanionSuffixes[] = {"-journal", "-wal",
                                                         "-shm"};

// Returns whether |relative_path| is |rule_path|, is inside it, or is one of
// its SQLite companion files. An empty |rule_path| matches everything.
bool MatchesRule(std::string_view relative_path, std::string_view rule_path) {
  if (rule_path.empty()) {
    return true;
  }
  if (!relative_path.starts_with(rule_path)) {
    return false;
  }
  const std::string_view rest = relative_path.substr(rule_path.size());
  if (rest.empty() || rest.front() == '/') {
    return true;
  }
  for (std::string_view suffix : kSqliteCompanionSuffixes) {
    if (rest == suffix) {
      return true;
    }
  }
  return false;
}

std::string_view GetPhaseName(StorageFootprintPhase phase) {
  switch (phase) {
    case StorageFootprintPhase::kStartup:
      return "Startup";
    case StorageFootprintPhase::kPostCleanup:
      return "PostCleanup";
    case StorageFootprintPhase::kUptime:
      return "Uptime";
  }
  NOTREACHED();
}

int ToKB(int64_t bytes) {
  return base::saturated_cast<int>(bytes / 1024);
}

struct MeasuredRoot {
  StorageRoot root;
  // Absolute, with symbolic links resolved.
  base::FilePath path;
};

class FootprintWalker {
 public:
  FootprintWalker(std::vector<MeasuredRoot> roots,
                  int64_t max_entries,
                  StorageFootprint* footprint)
      : roots_(std::move(roots)),
        max_entries_(max_entries),
        footprint_(footprint) {}

  void Walk() {
    for (size_t i = 0; i < roots_.size(); ++i) {
      if (IsCoveredByAnotherRoot(i)) {
        continue;
      }
      const base::FilePath& top = roots_[i].path;
      base::stat_wrapper_t root_stat;
      if (base::File::Lstat(top, &root_stat) == 0 && !Add(top, root_stat)) {
        return;
      }
      base::FileEnumerator enumerator(top, /*recursive=*/true,
                                      base::FileEnumerator::FILES |
                                          base::FileEnumerator::DIRECTORIES |
                                          base::FileEnumerator::SHOW_SYM_LINKS);
      for (base::FilePath path = enumerator.Next(); !path.empty();
           path = enumerator.Next()) {
        const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
        // SHOW_SYM_LINKS reports links with lstat() and never follows them.
        // Skip them: on Android, <app data>/lib links to the APK's libraries.
        if (S_ISLNK(info.stat().st_mode)) {
          continue;
        }
        if (!Add(path, info.stat())) {
          return;
        }
      }
    }
  }

 private:
  // Returns whether root |index| is inside another root, or equal to an
  // earlier one, so that walking the other root already covers it.
  bool IsCoveredByAnotherRoot(size_t index) const {
    const base::FilePath& path = roots_[index].path;
    for (size_t i = 0; i < roots_.size(); ++i) {
      if (i == index) {
        continue;
      }
      if (roots_[i].path.IsParent(path) ||
          (i < index && roots_[i].path == path)) {
        return true;
      }
    }
    return false;
  }

  // Adds |path| to every root that contains it, and to the bucket of the
  // innermost one (the first one in StorageRoot order if two roots are the
  // same directory). Returns false if the walk must stop.
  bool Add(const base::FilePath& path, const base::stat_wrapper_t& st) {
    if (footprint_->entry_count >= max_entries_) {
      footprint_->truncated = true;
      return false;
    }
    ++footprint_->entry_count;

    const int64_t allocated_bytes =
        static_cast<int64_t>(st.st_blocks) * kStatBlockSizeBytes;
    const bool is_file = S_ISREG(st.st_mode);
    const MeasuredRoot* innermost = nullptr;
    for (const MeasuredRoot& root : roots_) {
      if (root.path != path && !root.path.IsParent(path)) {
        continue;
      }
      StorageFootprint::RootUsage& usage =
          footprint_->roots[static_cast<size_t>(root.root)];
      usage.allocated_bytes += allocated_bytes;
      if (is_file) {
        usage.logical_bytes += st.st_size;
        ++usage.file_count;
      }
      if (!innermost ||
          root.path.value().size() > innermost->path.value().size()) {
        innermost = &root;
      }
    }
    // |path| comes from walking one of the roots.
    CHECK(innermost);

    base::FilePath relative_path;
    if (path != innermost->path) {
      innermost->path.AppendRelativePath(path, &relative_path);
    }
    const StorageBucket bucket =
        ClassifyStoragePath(innermost->root, relative_path.AsUTF8Unsafe());
    footprint_->bucket_allocated_bytes[static_cast<size_t>(bucket)] +=
        allocated_bytes;
    return true;
  }

  const std::vector<MeasuredRoot> roots_;
  const int64_t max_entries_;
  const raw_ptr<StorageFootprint> footprint_;
};

}  // namespace

std::string_view GetStorageRootName(StorageRoot root) {
  return kStorageRootNames[static_cast<size_t>(root)];
}

std::string_view GetStorageBucketName(StorageBucket bucket) {
  return kStorageBucketNames[static_cast<size_t>(bucket)];
}

StorageBucket ClassifyStoragePath(StorageRoot root,
                                  std::string_view relative_path) {
  for (const StoragePathRule& rule : kStoragePathRules) {
    if (rule.root == root && MatchesRule(relative_path, rule.path)) {
      return rule.bucket;
    }
  }
  // Every root has a catch-all rule (see HasCatchAllRule()).
  NOTREACHED();
}

StorageFootprint ComputeStorageFootprint(const StorageRootPaths& paths,
                                         int64_t max_entries) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  StorageFootprint footprint;

  const std::pair<StorageRoot, const base::FilePath*> configured_roots[] = {
      {StorageRoot::kUserData, &paths.user_data},
      {StorageRoot::kDirCache, &paths.dir_cache},
      {StorageRoot::kAppData, &paths.app_data},
  };
  std::vector<MeasuredRoot> roots;
  for (const auto& [root, path] : configured_roots) {
    if (path->empty()) {
      continue;
    }
    // Resolve symbolic links, e.g. Android's /data/user/0 -> /data/data, so
    // that nested roots can be recognized by their paths.
    base::FilePath real_path = base::MakeAbsoluteFilePath(*path);
    if (real_path.empty() || !base::DirectoryExists(real_path)) {
      continue;
    }
    footprint.roots[static_cast<size_t>(root)].measured = true;
    roots.push_back({root, std::move(real_path)});
  }
  for (const StoragePathRule& rule : kStoragePathRules) {
    if (footprint.root(rule.root).measured) {
      footprint.bucket_measured[static_cast<size_t>(rule.bucket)] = true;
    }
  }

  FootprintWalker(std::move(roots), max_entries, &footprint).Walk();
  return footprint;
}

void RecordStorageFootprintHistograms(StorageFootprintPhase phase,
                                      const StorageFootprint& footprint,
                                      base::TimeDelta walk_duration) {
  const std::string prefix =
      base::StrCat({"Cobalt.Storage.Footprint.", GetPhaseName(phase), "."});
  base::UmaHistogramMediumTimes(prefix + "WalkDuration", walk_duration);
  base::UmaHistogramBoolean("Cobalt.Storage.Footprint.WalkTruncated",
                            footprint.truncated);
  if (footprint.truncated) {
    // Partial sums would look like a smaller footprint.
    return;
  }

  for (size_t i = 0; i < kStorageRootCount; ++i) {
    const StorageFootprint::RootUsage& usage = footprint.roots[i];
    if (!usage.measured) {
      continue;
    }
    const std::string root_prefix =
        base::StrCat({prefix, kStorageRootNames[i], "."});
    base::UmaHistogramCustomCounts(root_prefix + "SizeKB",
                                   ToKB(usage.allocated_bytes), kMinSizeKB,
                                   kMaxSizeKB, kSizeHistogramBuckets);
    base::UmaHistogramCounts100000(root_prefix + "FileCount",
                                   base::saturated_cast<int>(usage.file_count));
  }
  for (size_t i = 0; i < kStorageBucketCount; ++i) {
    if (!footprint.bucket_measured[i]) {
      continue;
    }
    base::UmaHistogramCustomCounts(
        base::StrCat({prefix, "Bucket.", kStorageBucketNames[i], ".SizeKB"}),
        ToKB(footprint.bucket_allocated_bytes[i]), kMinSizeKB, kMaxSizeKB,
        kSizeHistogramBuckets);
  }
}

}  // namespace cobalt
