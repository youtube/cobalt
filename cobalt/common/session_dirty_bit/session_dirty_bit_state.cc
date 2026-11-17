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

#include "cobalt/common/session_dirty_bit/session_dirty_bit_state.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"

namespace cobalt {

const base::FilePath::CharType kSessionDirtyBitFilename[] =
    FILE_PATH_LITERAL("CobaltSessionDirtyBit");

namespace {

enum class CachedPreviousStage : uint8_t {
  kUnset = 0,
  kClean = 1,
  kEarly = 2,
  kLate = 3,
};

constexpr uint8_t kDiskByteClean = static_cast<uint8_t>('C');
constexpr uint8_t kDiskByteEarly = static_cast<uint8_t>('E');
constexpr uint8_t kDiskByteLate = static_cast<uint8_t>('L');
constexpr size_t kMappedRegionSize = 4;
constexpr char kSingleProcessSwitchName[] = "single-process";

std::atomic<CachedPreviousStage> g_previous_session_stage{
    CachedPreviousStage::kUnset};

CachedPreviousStage ToCachedStage(SessionDirtyBitStage stage) {
  switch (stage) {
    case SessionDirtyBitStage::kClean:
      return CachedPreviousStage::kClean;
    case SessionDirtyBitStage::kEarly:
      return CachedPreviousStage::kEarly;
    case SessionDirtyBitStage::kLate:
      return CachedPreviousStage::kLate;
  }
  return CachedPreviousStage::kClean;
}

SessionDirtyBitStage FromCachedStage(CachedPreviousStage cached) {
  switch (cached) {
    case CachedPreviousStage::kEarly:
      return SessionDirtyBitStage::kEarly;
    case CachedPreviousStage::kLate:
      return SessionDirtyBitStage::kLate;
    case CachedPreviousStage::kUnset:
    case CachedPreviousStage::kClean:
      return SessionDirtyBitStage::kClean;
  }
  return SessionDirtyBitStage::kClean;
}

uint8_t StageToDiskByte(SessionDirtyBitStage stage) {
  switch (stage) {
    case SessionDirtyBitStage::kClean:
      return kDiskByteClean;
    case SessionDirtyBitStage::kEarly:
      return kDiskByteEarly;
    case SessionDirtyBitStage::kLate:
      return kDiskByteLate;
  }
  return kDiskByteClean;
}

SessionDirtyBitStage DiskByteToStage(uint8_t byte) {
  switch (byte) {
    case kDiskByteEarly:
      return SessionDirtyBitStage::kEarly;
    case kDiskByteLate:
      return SessionDirtyBitStage::kLate;
    case kDiskByteClean:
    default:
      return SessionDirtyBitStage::kClean;
  }
}

// Manages browser-side session dirty-bit state and coordinates zero-syscall
// updates to `<DIR_CACHE>/CobaltSessionDirtyBit`.
//
// Lifetime and Ownership:
// Process-wide singleton owned by `base::NoDestructor` in `GetTracker()` for
// the lifetime of the browser process.
//
// Threading Model:
// Thread-safe. All member and `mapped_file` accesses are serialized by `lock`.
struct BrowserDirtyBitTracker {
  base::Lock lock;
  std::unique_ptr<base::MemoryMappedFile> mapped_file GUARDED_BY(lock);
  SessionDirtyBitStage on_disk_stage GUARDED_BY(lock) =
      SessionDirtyBitStage::kClean;
  SessionDirtyBitStage initial_prior_stage GUARDED_BY(lock) =
      SessionDirtyBitStage::kClean;
  bool initialized GUARDED_BY(lock) = false;
  bool prior_stage_consumed GUARDED_BY(lock) = true;
  bool is_foreground GUARDED_BY(lock) = false;
  bool has_promoted_to_late GUARDED_BY(lock) = false;
  base::RepeatingClosure previous_stage_cleared_callback GUARDED_BY(lock);

  void WriteStageIfChangedLocked(SessionDirtyBitStage target)
      EXCLUSIVE_LOCKS_REQUIRED(lock) {
    if (on_disk_stage == target) {
      return;
    }
    if (mapped_file && mapped_file->IsValid() &&
        !mapped_file->mutable_bytes().empty()) {
      mapped_file->mutable_bytes()[0] = StageToDiskByte(target);
    }
    on_disk_stage = target;
  }

  base::RepeatingClosure ClearPreviousStageLocked()
      EXCLUSIVE_LOCKS_REQUIRED(lock) {
    const CachedPreviousStage current =
        g_previous_session_stage.load(std::memory_order_acquire);
    if (current == CachedPreviousStage::kUnset && !initialized) {
      return {};
    }
    const CachedPreviousStage prev = g_previous_session_stage.exchange(
        CachedPreviousStage::kClean, std::memory_order_acq_rel);
    if (prev == CachedPreviousStage::kEarly ||
        prev == CachedPreviousStage::kLate) {
      return previous_stage_cleared_callback;
    }
    return {};
  }
};

BrowserDirtyBitTracker& GetTracker() {
  static base::NoDestructor<BrowserDirtyBitTracker> tracker;
  return *tracker;
}

}  // namespace

std::string_view SessionDirtyBitStageToString(SessionDirtyBitStage stage) {
  switch (stage) {
    case SessionDirtyBitStage::kClean:
      return "clean";
    case SessionDirtyBitStage::kEarly:
      return "early";
    case SessionDirtyBitStage::kLate:
      return "late";
  }
  return "clean";
}

SessionDirtyBitStage SessionDirtyBitStageFromString(std::string_view value) {
  if (value == "early") {
    return SessionDirtyBitStage::kEarly;
  }
  if (value == "late") {
    return SessionDirtyBitStage::kLate;
  }
  return SessionDirtyBitStage::kClean;
}

bool IsSessionDirtyBitSharedInProcess() {
  return base::CommandLine::InitializedForCurrentProcess() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             kSingleProcessSwitchName);
}

void InitializeSessionDirtyBit(const base::FilePath& storage_dir,
                               bool is_initially_visible) {
  // Called during early browser startup (`CreateFeatureListAndFieldTrials()`,
  // alongside `CleanExitBeacon::Initialize()`) before `BrowserMainLoop`
  // disallows blocking I/O on the UI thread; `base::File` and
  // `MemoryMappedFile` internally verify blocking permission via
  // `ScopedBlockingCall`.
  std::unique_ptr<base::MemoryMappedFile> new_mapped_file;
  SessionDirtyBitStage prior_stage = SessionDirtyBitStage::kClean;

  if (!storage_dir.empty()) {
    const base::FilePath file_path =
        storage_dir.Append(kSessionDirtyBitFilename);
    base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                   base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    if (file.IsValid()) {
      if (file.GetLength() > static_cast<int64_t>(kMappedRegionSize)) {
        file.SetLength(kMappedRegionSize);
      }
      auto mapped = std::make_unique<base::MemoryMappedFile>();
      const base::MemoryMappedFile::Region region = {0, kMappedRegionSize};
      if (mapped->Initialize(std::move(file), region,
                             base::MemoryMappedFile::READ_WRITE_EXTEND) &&
          !mapped->bytes().empty()) {
        prior_stage = DiskByteToStage(mapped->bytes()[0]);
        new_mapped_file = std::move(mapped);
      }
    }
  }

  std::unique_ptr<base::MemoryMappedFile> old_mapped_file;
  {
    auto& tracker = GetTracker();
    base::AutoLock auto_lock(tracker.lock);
    old_mapped_file = std::move(tracker.mapped_file);
    tracker.mapped_file = std::move(new_mapped_file);
    g_previous_session_stage.store(ToCachedStage(prior_stage),
                                   std::memory_order_release);

    tracker.initialized = true;
    tracker.on_disk_stage = prior_stage;
    tracker.initial_prior_stage = prior_stage;
    tracker.prior_stage_consumed =
        (prior_stage == SessionDirtyBitStage::kClean);
    tracker.is_foreground = is_initially_visible;
    tracker.has_promoted_to_late = false;

    if (is_initially_visible) {
      tracker.WriteStageIfChangedLocked(SessionDirtyBitStage::kEarly);
    } else if (prior_stage == SessionDirtyBitStage::kClean) {
      // Ensure a freshly created preload file contains 'C' explicitly.
      if (tracker.mapped_file && tracker.mapped_file->IsValid() &&
          !tracker.mapped_file->mutable_bytes().empty()) {
        tracker.mapped_file->mutable_bytes()[0] = kDiskByteClean;
      }
    }
  }
}

SessionDirtyBitStage GetPreviousSessionDirtyBitStage() {
  CachedPreviousStage state =
      g_previous_session_stage.load(std::memory_order_acquire);
  if (state == CachedPreviousStage::kUnset) {
    if (!base::CommandLine::InitializedForCurrentProcess()) {
      return SessionDirtyBitStage::kClean;
    }
    const std::string switch_val =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kPreviousSessionDirtyBitStageSwitch);
    const CachedPreviousStage computed =
        ToCachedStage(SessionDirtyBitStageFromString(switch_val));
    if (g_previous_session_stage.compare_exchange_strong(
            state, computed, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      state = computed;
    }
  }
  return FromCachedStage(state);
}

void SetPreviousSessionDirtyBitStage(SessionDirtyBitStage stage) {
  g_previous_session_stage.store(ToCachedStage(stage),
                                 std::memory_order_release);
}

void PromoteSessionDirtyBit() {
  auto& tracker = GetTracker();
  base::AutoLock auto_lock(tracker.lock);
  tracker.prior_stage_consumed = true;
  tracker.has_promoted_to_late = true;
  if (tracker.is_foreground) {
    tracker.WriteStageIfChangedLocked(SessionDirtyBitStage::kLate);
  }
}

void MarkPreviousSessionDirtyBitConsumed() {
  auto& tracker = GetTracker();
  base::AutoLock auto_lock(tracker.lock);
  tracker.prior_stage_consumed = true;
}

void ClearSessionDirtyBit() {
  base::RepeatingClosure callback_to_run;
  {
    auto& tracker = GetTracker();
    base::AutoLock auto_lock(tracker.lock);
    tracker.prior_stage_consumed = true;
    tracker.has_promoted_to_late = false;
    tracker.WriteStageIfChangedLocked(SessionDirtyBitStage::kClean);
    callback_to_run = tracker.ClearPreviousStageLocked();
  }
  if (callback_to_run) {
    callback_to_run.Run();
  }
}

void OnSessionDirtyBitPlatformDisarm(bool reset_promotion) {
  base::RepeatingClosure callback_to_run;
  {
    auto& tracker = GetTracker();
    base::AutoLock auto_lock(tracker.lock);
    tracker.is_foreground = false;
    if (reset_promotion) {
      tracker.has_promoted_to_late = false;
    }
    if (!tracker.prior_stage_consumed) {
      // A prior-session crash (`kEarly` or `kLate`) has not yet been consumed
      // by web bootstrap in this session (e.g., transient early-boot blur or
      // user backgrounding before JS loaded). Restore the prior crash stage on
      // disk and preserve `g_previous_session_stage` so it is never lost.
      tracker.WriteStageIfChangedLocked(tracker.initial_prior_stage);
      return;
    }
    tracker.WriteStageIfChangedLocked(SessionDirtyBitStage::kClean);
    callback_to_run = tracker.ClearPreviousStageLocked();
  }
  if (callback_to_run) {
    callback_to_run.Run();
  }
}

void OnSessionDirtyBitPlatformForeground() {
  auto& tracker = GetTracker();
  base::AutoLock auto_lock(tracker.lock);
  tracker.is_foreground = true;
  const SessionDirtyBitStage target = tracker.has_promoted_to_late
                                          ? SessionDirtyBitStage::kLate
                                          : SessionDirtyBitStage::kEarly;
  tracker.WriteStageIfChangedLocked(target);
}

void OnSessionDirtyBitMainFrameNavigationCommitted() {
  auto& tracker = GetTracker();
  base::AutoLock auto_lock(tracker.lock);
  tracker.has_promoted_to_late = false;
  if (tracker.is_foreground) {
    tracker.WriteStageIfChangedLocked(SessionDirtyBitStage::kEarly);
  }
}

void SetPreviousSessionDirtyBitClearedCallback(
    base::RepeatingClosure callback) {
  auto& tracker = GetTracker();
  base::AutoLock auto_lock(tracker.lock);
  tracker.previous_stage_cleared_callback = std::move(callback);
}

SessionDirtyBitStage ReadSessionDirtyBitFileForTesting(
    const base::FilePath& storage_dir) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File file(storage_dir.Append(kSessionDirtyBitFilename),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return SessionDirtyBitStage::kClean;
  }
  uint8_t first_byte = 0;
  if (file.Read(0, base::span_from_ref(first_byte)) != 1u) {
    return SessionDirtyBitStage::kClean;
  }
  return DiskByteToStage(first_byte);
}

void WriteSessionDirtyBitFileForTesting(const base::FilePath& storage_dir,
                                        SessionDirtyBitStage stage) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const uint8_t byte = StageToDiskByte(stage);
  base::WriteFile(storage_dir.Append(kSessionDirtyBitFilename),
                  base::span_from_ref(byte));
}

void ResetSessionDirtyBitForTesting() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<base::MemoryMappedFile> old_mapped_file;
  {
    auto& tracker = GetTracker();
    base::AutoLock auto_lock(tracker.lock);
    old_mapped_file = std::move(tracker.mapped_file);
    tracker.on_disk_stage = SessionDirtyBitStage::kClean;
    tracker.initial_prior_stage = SessionDirtyBitStage::kClean;
    tracker.initialized = false;
    tracker.prior_stage_consumed = true;
    tracker.is_foreground = false;
    tracker.has_promoted_to_late = false;
    tracker.previous_stage_cleared_callback.Reset();
    g_previous_session_stage.store(CachedPreviousStage::kUnset,
                                   std::memory_order_release);
  }
}

}  // namespace cobalt
