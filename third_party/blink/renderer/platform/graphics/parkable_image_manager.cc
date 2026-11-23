// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"

#if BUILDFLAG(IS_COBALT)
#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#endif  // BUILDFLAG(IS_COBALT)
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

struct ParkableImageManager::Statistics {
  size_t unparked_size = 0;
  size_t on_disk_size = 0;
  size_t total_size = 0;
};

constexpr const char* ParkableImageManager::kAllocatorDumpName;

constexpr base::TimeDelta ParkableImageManager::kDelayedParkingInterval;

// static
ParkableImageManager& ParkableImageManager::Instance() {
  static base::NoDestructor<ParkableImageManager> instance;
  return *instance;
}

ParkableImageManager::ParkableImageManager()
    : task_runner_(Thread::MainThread()->GetTaskRunner(
          MainThreadTaskRunnerRestricted())) {
#if BUILDFLAG(IS_COBALT)
  background_parking_timer_.SetTaskRunner(task_runner_);
#endif  // BUILDFLAG(IS_COBALT)
}

void ParkableImageManager::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  task_runner_ = std::move(task_runner);
#if BUILDFLAG(IS_COBALT)
  background_parking_timer_.SetTaskRunner(task_runner_);
#endif  // BUILDFLAG(IS_COBALT)
}

bool ParkableImageManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs&,
    base::trace_event::ProcessMemoryDump* pmd) {
  auto* dump = pmd->CreateAllocatorDump(kAllocatorDumpName);

  base::AutoLock lock(lock_);
  Statistics stats = ComputeStatistics();

  dump->AddScalar("total_size", "bytes", stats.total_size);
  dump->AddScalar("unparked_size", "bytes", stats.unparked_size);
  dump->AddScalar("on_disk_size", "bytes", stats.on_disk_size);

  return true;
}

ParkableImageManager::Statistics ParkableImageManager::ComputeStatistics()
    const {
  Statistics stats;

  for (auto* unparked : unparked_images_)
    stats.unparked_size += unparked->size();

  for (auto* on_disk : on_disk_images_)
    stats.on_disk_size += on_disk->size();

  stats.total_size = stats.on_disk_size + stats.unparked_size;

  return stats;
}

size_t ParkableImageManager::Size() const {
  base::AutoLock lock(lock_);

  return on_disk_images_.size() + unparked_images_.size();
}

DiskDataAllocator& ParkableImageManager::data_allocator() const {
  if (allocator_for_testing_)
    return *allocator_for_testing_;

  return DiskDataAllocator::Instance();
}

void ParkableImageManager::ResetForTesting() {
#if BUILDFLAG(IS_COBALT)
  renderer_backgrounded_ = false;
  background_parking_timer_.Stop();
  memory_pressure_listener_.reset();
#endif  // BUILDFLAG(IS_COBALT)
  base::AutoLock lock(lock_);

  has_pending_parking_task_ = false;
  has_posted_accounting_task_ = false;
  unparked_images_.clear();
  on_disk_images_.clear();
  allocator_for_testing_ = nullptr;
  total_disk_read_time_ = base::TimeDelta();
  total_disk_write_time_ = base::TimeDelta();
}

void ParkableImageManager::Add(ParkableImageImpl* impl) {
  DCHECK(IsMainThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(impl->lock_);
    DCHECK(!IsRegistered(impl));
  }
#endif  // DCHECK_IS_ON()

#if BUILDFLAG(IS_COBALT)
  if (IsUnparkedBudgetEnabled() && !memory_pressure_listener_) {
    memory_pressure_listener_.emplace(
        FROM_HERE, base::BindRepeating(&ParkableImageManager::OnMemoryPressure,
                                       base::Unretained(this)));
  }
#endif  // BUILDFLAG(IS_COBALT)

  base::AutoLock lock(lock_);

  ScheduleDelayedParkingTaskIfNeeded();

  if (!has_posted_accounting_task_) {
    // |base::Unretained(this)| is fine because |this| is a NoDestructor
    // singleton.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ParkableImageManager::RecordStatisticsAfter5Minutes,
                       base::Unretained(this)),
        base::Minutes(5));
    has_posted_accounting_task_ = true;
  }

  unparked_images_.insert(impl);
}

void ParkableImageManager::RecordStatisticsAfter5Minutes() const {
  DCHECK(IsMainThread());

  base::AutoLock lock(lock_);

  Statistics stats = ComputeStatistics();

  // In KiB
  base::UmaHistogramCounts100000("Memory.ParkableImage.TotalSize.5min",
                                 static_cast<int>(stats.total_size / 1024));
  base::UmaHistogramCounts100000("Memory.ParkableImage.OnDiskSize.5min",
                                 static_cast<int>(stats.on_disk_size / 1024));
  base::UmaHistogramCounts100000("Memory.ParkableImage.UnparkedSize.5min",
                                 static_cast<int>(stats.unparked_size / 1024));

  // Metrics related to parking only should be recorded if the feature is
  // enabled.
  if (IsParkableImagesToDiskEnabled() && data_allocator().may_write()) {
    base::UmaHistogramTimes("Memory.ParkableImage.TotalWriteTime.5min",
                            total_disk_write_time_);
    base::UmaHistogramTimes("Memory.ParkableImage.TotalReadTime.5min",
                            total_disk_read_time_);
  }
}

scoped_refptr<ParkableImageImpl> ParkableImageManager::CreateParkableImage(
    size_t offset) {
  base::AutoLock lock(lock_);
  scoped_refptr<ParkableImageImpl> impl = ParkableImageImpl::Create(offset);
  return impl;
}

void ParkableImageManager::DestroyParkableImageOnMainThread(
    scoped_refptr<ParkableImageImpl> image) {
  DCHECK(IsMainThread());
}

void ParkableImageManager::DestroyParkableImage(
    scoped_refptr<ParkableImageImpl> image) {
  if (IsMainThread()) {
    DestroyParkableImageOnMainThread(std::move(image));
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ParkableImageManager::DestroyParkableImageOnMainThread,
                       base::Unretained(this), std::move(image)));
  }
}

void ParkableImageManager::Remove(ParkableImageImpl* image) {
  base::AutoLock lock(lock_);

  // Image could be on disk or unparked. Remove it in either case.
  auto* map = image->is_on_disk() ? &on_disk_images_ : &unparked_images_;
  auto it = map->find(image);
  CHECK(it != map->end());
  map->erase(it);
}

void ParkableImageManager::MoveImage(ParkableImageImpl* image,
                                     WTF::HashSet<ParkableImageImpl*>* from,
                                     WTF::HashSet<ParkableImageImpl*>* to) {
  auto it = from->find(image);
  CHECK(it != from->end());
  CHECK(!to->Contains(image));
  from->erase(it);
  to->insert(image);
}

bool ParkableImageManager::IsRegistered(ParkableImageImpl* image) {
  base::AutoLock lock(lock_);

  auto* map = image->is_on_disk() ? &on_disk_images_ : &unparked_images_;
  auto it = map->find(image);

  return it != map->end();
}

void ParkableImageManager::OnWrittenToDisk(ParkableImageImpl* image) {
  base::AutoLock lock(lock_);
  MoveImage(image, &unparked_images_, &on_disk_images_);
}

void ParkableImageManager::OnReadFromDisk(ParkableImageImpl* image) {
  base::AutoLock lock(lock_);
  MoveImage(image, &on_disk_images_, &unparked_images_);
  ScheduleDelayedParkingTaskIfNeeded();
}

void ParkableImageManager::ScheduleDelayedParkingTaskIfNeeded() {
  if (!ParkableImageManager::IsParkableImagesToDiskEnabled())
    return;

  if (has_pending_parking_task_)
    return;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ParkableImageManager::MaybeParkImages,
                     base::Unretained(this)),
      ParkableImageManager::kDelayedParkingInterval);
  has_pending_parking_task_ = true;
}

void ParkableImageManager::MaybeParkImages() {
  // Because we only have a raw pointer to the ParkableImageImpl, we need to be
  // very careful here to avoid a UAF.
  // To avoid this, we make sure that ParkableImageImpl is always destroyed on
  // the main thread, using |ParkableImageManager::DestroyParkableImage|.
  DCHECK(ParkableImageManager::IsParkableImagesToDiskEnabled());
  DCHECK(IsMainThread());

  base::AutoLock lock(lock_);

  // This makes a copy of the pointers stored in |unparked_images_|. We iterate
  // over this copy in |MaybeParkImages|, instead of |unparked_images_|
  // directly, to avoid deadlock when we need to park synchronously (i.e. if we
  // have already written to disk and don't need to post a background task), as
  // synchronous parking calls |ParkableImageManager::OnWrittenToDisk()|;
  WTF::Vector<ParkableImageImpl*> unparked_images(unparked_images_);

  // We unlock here so that we can avoid a deadlock, since if the data for the
  // image is already written to disk, we can discard our copy of the data
  // synchronously, which calls back into the manager.
  lock_.Release();

  bool should_reschedule = false;
#if BUILDFLAG(IS_COBALT)
  if (IsUnparkedBudgetEnabled()) {
    const size_t budget_bytes =
        (renderer_backgrounded_ && !background_parking_timer_.IsRunning())
            ? 0
            : UnparkedBudgetBytes();
    ParkImagesToBudget(unparked_images, budget_bytes, &should_reschedule);
  } else {
#endif  // BUILDFLAG(IS_COBALT)
    for (auto* image : unparked_images) {
      if (image->ShouldReschedule())
        should_reschedule = true;
      image->MaybePark(task_runner_);
    }
#if BUILDFLAG(IS_COBALT)
  }
#endif  // BUILDFLAG(IS_COBALT)

  lock_.Acquire();

  has_pending_parking_task_ = false;

  if (should_reschedule)
    ScheduleDelayedParkingTaskIfNeeded();
}

#if BUILDFLAG(IS_COBALT)
constexpr base::TimeDelta ParkableImageManager::kCobaltBackgroundParkingDelay;

// static
bool ParkableImageManager::IsUnparkedBudgetEnabled() {
  return IsParkableImagesToDiskEnabled() &&
         base::FeatureList::IsEnabled(
             features::kCobaltParkableMemoryOptimization);
}

// static
size_t ParkableImageManager::UnparkedBudgetBytes() {
  const int kb = std::clamp(
      features::kCobaltParkableMemoryOptimizationMaxUnparkedKb.Get(), 0, 65536);
  return static_cast<size_t>(kb) * 1024;
}

void ParkableImageManager::SetRendererBackgrounded(bool backgrounded) {
  DCHECK(IsMainThread());
  if (renderer_backgrounded_ == backgrounded) {
    return;
  }
  renderer_backgrounded_ = backgrounded;
  if (!backgrounded) {
    background_parking_timer_.Stop();
    return;
  }
  if (IsUnparkedBudgetEnabled()) {
    background_parking_timer_.Start(
        FROM_HERE, kCobaltBackgroundParkingDelay,
        base::BindOnce(&ParkableImageManager::ParkAllImages,
                       base::Unretained(this)));
  }
}

void ParkableImageManager::ParkImagesToBudget(
    const WTF::Vector<ParkableImageImpl*>& unparked_images,
    size_t budget_bytes,
    bool* should_reschedule) {
  DCHECK(IsMainThread());

  struct Candidate {
    raw_ptr<ParkableImageImpl> image;
    base::TimeTicks last_used;
  };
  WTF::Vector<Candidate> candidates;
  candidates.reserve(unparked_images.size());
  size_t unparked_bytes = 0;
  for (ParkableImageImpl* image : unparked_images) {
    if (image->ShouldReschedule()) {
      *should_reschedule = true;
    }
    base::TimeTicks last_used = image->frozen_time_;
    {
      base::AutoLock image_lock(image->lock_);
      if (image->background_task_in_progress_) {
        continue;
      }
      if (!image->last_used_time_.is_null()) {
        last_used = image->last_used_time_;
      }
    }
    unparked_bytes += image->size();
    candidates.push_back(Candidate{image, last_used});
  }
  if (unparked_bytes <= budget_bytes) {
    return;
  }

  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& a, const Candidate& b) {
                     return a.last_used < b.last_used;
                   });
  for (const Candidate& candidate : candidates) {
    if (unparked_bytes <= budget_bytes) {
      break;
    }
    if (candidate.image->MaybePark(task_runner_)) {
      unparked_bytes -= candidate.image->size();
    }
  }
  if (unparked_bytes > budget_bytes && data_allocator().may_write()) {
    *should_reschedule = true;
  }
}

void ParkableImageManager::ParkAllImages() {
  DCHECK(IsMainThread());
  if (!IsUnparkedBudgetEnabled()) {
    return;
  }
  WTF::Vector<ParkableImageImpl*> unparked_images;
  {
    base::AutoLock lock(lock_);
    unparked_images = WTF::Vector<ParkableImageImpl*>(unparked_images_);
  }
  bool should_reschedule = false;
  ParkImagesToBudget(unparked_images, /*budget_bytes=*/0, &should_reschedule);
  if (should_reschedule) {
    base::AutoLock lock(lock_);
    ScheduleDelayedParkingTaskIfNeeded();
  }
}

void ParkableImageManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  DCHECK(IsMainThread());
  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }
  ParkAllImages();
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace blink
