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

#include "cobalt/browser/metrics/cobalt_storage_metrics_helper.h"

#include <algorithm>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "cobalt/shell/common/shell_paths.h"

namespace cobalt {

namespace {

// Delay after browser startup before recording directory footprints and
// startup I/O counters. Sampled at +10s so startup metrics land inside
// Android's Initial UMA Log (+15s) and before background storage cleanup
// tasks (+30s).
constexpr base::TimeDelta kPostStartupStorageMetricsDelay = base::Seconds(10);

// Matches Cobalt's default 5-minute UMA upload interval so that each uploaded
// log carries roughly one write-rate sample.
constexpr base::TimeDelta kPeriodicWriteRateSampleInterval = base::Minutes(5);

// Histogram bucket bounds (exponential, 100 buckets). These histograms record
// one sample per session, so the larger bucket count adds no upload cost.
// Startup block reads include paging in the native library and APK assets
// (~100 MB measured on an Android TV device), so resolution below 1 MB is not
// needed. 1 MB to 500 MB gives ~6.5% bucket width.
constexpr int kMinStartupReadKB = 1000;
constexpr int kMaxStartupReadKB = 500000;
// Directory footprints and startup block writes: 1 KB to 100 MB gives ~11%
// bucket width above ~100 KB. 100 MB is 100x the 1 MB USER_DATA partner quota
// and 4x the 24 MB DIR_CACHE quota; larger values land in the overflow bucket.
constexpr int kMinStorageSizeKB = 1;
constexpr int kMaxStorageSizeKB = 100000;
constexpr int kStorageSizeBuckets = 100;

// Tracker class responsible for periodic recording of steady-state storage
// write rates. It is managed as a process-wide singleton with a lifetime
// spanning the entire run of the browser process. All state mutations and
// metric recordings are sequence-affine and execute on its internal
// BEST_EFFORT background task runner.
class StorageIoWriteRateTracker {
 public:
  static StorageIoWriteRateTracker* GetInstance() {
    static base::NoDestructor<StorageIoWriteRateTracker> instance;
    return instance.get();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }

  void InitializeBaseline(int64_t write_bytes, base::TimeTicks now) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    last_write_bytes_ = write_bytes;
    last_sample_time_ = now;
    initialized_ = true;
  }

  void MaybeRecordSampleOnTaskRunner() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!initialized_) {
      return;
    }
    ProcessIoCounters counters;
    if (!ReadCurrentProcessIoCounters(&counters)) {
      return;
    }
    const base::TimeTicks now = base::TimeTicks::Now();
    if (RecordSteadyStateWriteRateForInterval(
            last_write_bytes_, last_sample_time_, counters.write_bytes, now)) {
      last_write_bytes_ = counters.write_bytes;
      last_sample_time_ = now;
    }
  }

 private:
  friend class base::NoDestructor<StorageIoWriteRateTracker>;

  StorageIoWriteRateTracker()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // Set once in the constructor and never changed, so task_runner() is safe
  // to call from any thread.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Guards the members below, which are only accessed on |task_runner_|. The
  // constructor runs on the thread that first calls GetInstance() (the UI
  // thread, via ScheduleCobaltStorageMetricsRecording()), so it detaches the
  // checker, which then binds to |task_runner_| on first use.
  SEQUENCE_CHECKER(sequence_checker_);
  bool initialized_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  int64_t last_write_bytes_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::TimeTicks last_sample_time_ GUARDED_BY_CONTEXT(sequence_checker_);
};

void RunPeriodicWriteRateSample() {
  auto* tracker = StorageIoWriteRateTracker::GetInstance();
  tracker->MaybeRecordSampleOnTaskRunner();
  tracker->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RunPeriodicWriteRateSample),
      kPeriodicWriteRateSampleInterval);
}

void RunPostStartupStorageMetricsTask() {
  // 1. Sample /proc/self/io FIRST before any directory walk so that
  // StartupReadKB is never polluted by stat()/readdir() metadata reads.
  ProcessIoCounters io_counters;
  const bool has_io_counters = ReadCurrentProcessIoCounters(&io_counters);
  if (has_io_counters) {
    StorageIoWriteRateTracker::GetInstance()->InitializeBaseline(
        io_counters.write_bytes, base::TimeTicks::Now());
  }

  // 2. Resolve USER_DATA and DIR_CACHE paths and emit footprint + I/O metrics.
  base::FilePath user_data_dir;
  base::PathService::Get(content::SHELL_DIR_USER_DATA, &user_data_dir);

  base::FilePath cache_dir;
  base::PathService::Get(base::DIR_CACHE, &cache_dir);

  RecordStartupStorageAndIoMetricsForPaths(
      user_data_dir, cache_dir, has_io_counters ? &io_counters : nullptr);

  // 3. Schedule periodic steady-state write-rate sampling.
  if (has_io_counters) {
    StorageIoWriteRateTracker::GetInstance()->task_runner()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RunPeriodicWriteRateSample),
        kPeriodicWriteRateSampleInterval);
  }
}

}  // namespace

bool ParseProcIoCounters(std::string_view proc_io_contents,
                         ProcessIoCounters* out_counters) {
  if (!out_counters) {
    return false;
  }
  int64_t read_bytes = -1;
  int64_t write_bytes = -1;

  constexpr std::string_view kReadBytesPrefix = "read_bytes:";
  constexpr std::string_view kWriteBytesPrefix = "write_bytes:";

  for (std::string_view line :
       base::SplitStringPiece(proc_io_contents, "\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (base::StartsWith(line, kReadBytesPrefix)) {
      std::string_view val_str = base::TrimWhitespaceASCII(
          line.substr(kReadBytesPrefix.size()), base::TRIM_ALL);
      int64_t parsed = -1;
      if (base::StringToInt64(val_str, &parsed) && parsed >= 0) {
        read_bytes = parsed;
      }
    } else if (base::StartsWith(line, kWriteBytesPrefix)) {
      std::string_view val_str = base::TrimWhitespaceASCII(
          line.substr(kWriteBytesPrefix.size()), base::TRIM_ALL);
      int64_t parsed = -1;
      if (base::StringToInt64(val_str, &parsed) && parsed >= 0) {
        write_bytes = parsed;
      }
    }
  }

  if (read_bytes < 0 || write_bytes < 0) {
    return false;
  }
  out_counters->read_bytes = read_bytes;
  out_counters->write_bytes = write_bytes;
  return true;
}

bool ReadCurrentProcessIoCounters(ProcessIoCounters* out_counters) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_STARBOARD)
  // Android builds are not Starboard builds, so both are listed. On Starboard
  // platforms without procfs the read fails and no sample is recorded.
  std::string contents;
  if (!base::ReadFileToString(base::FilePath("/proc/self/io"), &contents)) {
    return false;
  }
  return ParseProcIoCounters(contents, out_counters);
#else
  return false;
#endif
}

void RecordStartupStorageAndIoMetricsForPaths(
    const base::FilePath& user_data_dir,
    const base::FilePath& cache_dir,
    const ProcessIoCounters* io_counters) {
  if (io_counters && io_counters->read_bytes >= 0 &&
      io_counters->write_bytes >= 0) {
    base::UmaHistogramCustomCounts(
        "Cobalt.Storage.ProcessIo.StartupReadKB",
        base::saturated_cast<int>(io_counters->read_bytes / 1024),
        kMinStartupReadKB, kMaxStartupReadKB, kStorageSizeBuckets);
    base::UmaHistogramCustomCounts(
        "Cobalt.Storage.ProcessIo.StartupWriteKB",
        base::saturated_cast<int>(io_counters->write_bytes / 1024),
        kMinStorageSizeKB, kMaxStorageSizeKB, kStorageSizeBuckets);
  }

  if (!user_data_dir.empty() && base::DirectoryExists(user_data_dir)) {
    const int64_t user_data_bytes = base::ComputeDirectorySize(user_data_dir);
    base::UmaHistogramCustomCounts(
        "Cobalt.Storage.UserData.StartupTotalSizeKB",
        base::saturated_cast<int>(user_data_bytes / 1024), kMinStorageSizeKB,
        kMaxStorageSizeKB, kStorageSizeBuckets);
  }

  if (!cache_dir.empty() && base::DirectoryExists(cache_dir)) {
    const int64_t dir_cache_bytes = base::ComputeDirectorySize(cache_dir);
    base::UmaHistogramCustomCounts(
        "Cobalt.Storage.DirCache.StartupTotalSizeKB",
        base::saturated_cast<int>(dir_cache_bytes / 1024), kMinStorageSizeKB,
        kMaxStorageSizeKB, kStorageSizeBuckets);
  }
}

bool RecordSteadyStateWriteRateForInterval(int64_t baseline_write_bytes,
                                           base::TimeTicks baseline_time,
                                           int64_t current_write_bytes,
                                           base::TimeTicks current_time) {
  const base::TimeDelta elapsed = current_time - baseline_time;
  if (!elapsed.is_positive() || current_write_bytes < baseline_write_bytes) {
    return false;
  }

  const int64_t delta_bytes = current_write_bytes - baseline_write_bytes;
  const double elapsed_minutes = elapsed.InSecondsF() / 60.0;
  const int write_rate_kb_per_min =
      base::ClampRound<int>((delta_bytes / 1024.0) / elapsed_minutes);
  base::UmaHistogramCounts100000("Cobalt.Storage.ProcessIo.WriteRateKBPerMin",
                                 write_rate_kb_per_min);
  return true;
}

void ScheduleCobaltStorageMetricsRecording() {
  StorageIoWriteRateTracker::GetInstance()->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RunPostStartupStorageMetricsTask),
      kPostStartupStorageMetricsDelay);
}

}  // namespace cobalt
