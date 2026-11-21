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

#ifndef COBALT_BROWSER_METRICS_COBALT_STORAGE_FOOTPRINT_H_
#define COBALT_BROWSER_METRICS_COBALT_STORAGE_FOOTPRINT_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace cobalt {

// The directories whose disk usage is measured. See cobalt_storage_buckets.inc.
enum class StorageRoot {
  kUserData,
  kDirCache,
  kAppData,
};
inline constexpr size_t kStorageRootCount = 3;

// Storage buckets, in the order of cobalt_storage_buckets.inc.
enum class StorageBucket {
#define COBALT_STORAGE_BUCKET(name) k##name,
#define COBALT_STORAGE_PATH(root, path, bucket)
#include "cobalt/browser/metrics/cobalt_storage_buckets.inc"
#undef COBALT_STORAGE_PATH
#undef COBALT_STORAGE_BUCKET
};
inline constexpr size_t kStorageBucketCount =
#define COBALT_STORAGE_BUCKET(name) 1 +
#define COBALT_STORAGE_PATH(root, path, bucket)
#include "cobalt/browser/metrics/cobalt_storage_buckets.inc"
#undef COBALT_STORAGE_PATH
#undef COBALT_STORAGE_BUCKET
    0;

// Names used in histogram names, e.g. "UserData" and "LocalStorage".
std::string_view GetStorageRootName(StorageRoot root);
std::string_view GetStorageBucketName(StorageBucket bucket);

// Returns the bucket of an entry at |relative_path| (using '/' separators)
// under |root|. The root directory itself has an empty |relative_path|.
StorageBucket ClassifyStoragePath(StorageRoot root,
                                  std::string_view relative_path);

// Paths of the roots. An empty path means the root is not measured.
struct StorageRootPaths {
  base::FilePath user_data;
  base::FilePath dir_cache;
  // Android only: the app's private data directory, which contains the other
  // two roots and the Java app's directories (files/, shared_prefs/, ...).
  base::FilePath app_data;
};

// Disk usage of the roots and buckets at one point in time.
struct StorageFootprint {
  struct RootUsage {
    // Whether the root exists and was measured.
    bool measured = false;
    // Disk space allocated to the root directory and every file and directory
    // under it (st_blocks * 512), which is what `du` and the OS count. On f2fs
    // every file and directory also takes a 4 KB inode block.
    int64_t allocated_bytes = 0;
    // Sum of the sizes (st_size) of the regular files under the root, as
    // base::ComputeDirectorySize() reports.
    int64_t logical_bytes = 0;
    // Number of regular files under the root.
    int64_t file_count = 0;
  };
  std::array<RootUsage, kStorageRootCount> roots = {};
  // Allocated bytes per bucket. Every entry is in exactly one bucket, so the
  // buckets add up to the allocated bytes of the outermost roots.
  std::array<int64_t, kStorageBucketCount> bucket_allocated_bytes = {};
  // Whether the bucket has a rule for a measured root. Buckets of unmeasured
  // roots, e.g. the Android buckets on other platforms, are not recorded.
  std::array<bool, kStorageBucketCount> bucket_measured = {};
  // Number of files and directories visited, including the roots. Symbolic
  // links are skipped.
  int64_t entry_count = 0;
  // Whether the walk stopped at |max_entries| before visiting everything.
  bool truncated = false;

  const RootUsage& root(StorageRoot r) const {
    return roots[static_cast<size_t>(r)];
  }
  int64_t bucket_bytes(StorageBucket b) const {
    return bucket_allocated_bytes[static_cast<size_t>(b)];
  }
};

// Walks the roots once, without following symbolic links, and attributes every
// file and directory to the roots that contain it and to one bucket, which is
// classified against the innermost root. Roots are compared after resolving
// symbolic links in their paths. Stops after |max_entries| entries. Must be
// called where blocking is allowed.
inline constexpr int64_t kMaxStorageFootprintEntries = 100000;
StorageFootprint ComputeStorageFootprint(
    const StorageRootPaths& paths,
    int64_t max_entries = kMaxStorageFootprintEntries);

// When in the app lifetime a footprint is sampled.
enum class StorageFootprintPhase {
  // 10 s after browser startup.
  kStartup,
  // After the delayed storage cleanup task (30 s after startup), whether or not
  // kCobaltStorageCleanup is enabled.
  kPostCleanup,
  // Every CobaltStorageFootprintSampler_uptime_interval (default 24 h) of
  // browser process uptime.
  kUptime,
};

// Records the Cobalt.Storage.Footprint.* histograms for |footprint|. A
// truncated walk only records WalkDuration and WalkTruncated.
void RecordStorageFootprintHistograms(StorageFootprintPhase phase,
                                      const StorageFootprint& footprint,
                                      base::TimeDelta walk_duration);

}  // namespace cobalt

#endif  // COBALT_BROWSER_METRICS_COBALT_STORAGE_FOOTPRINT_H_
