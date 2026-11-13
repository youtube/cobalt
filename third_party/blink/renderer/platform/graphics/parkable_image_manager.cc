// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/renderer/platform/graphics/parkable_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

#if BUILDFLAG(IS_COBALT)
#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#endif  // BUILDFLAG(IS_COBALT)

namespace blink {

#if BUILDFLAG(IS_COBALT)
namespace {

// OnPurgeMemory() runs right after OnMemoryPressure(CRITICAL), and not for
// MODERATE pressure, which Cobalt reports often.
class ParkableImagePurgeMemoryListener final
    : public GarbageCollected<ParkableImagePurgeMemoryListener>,
      public MemoryPressureListener {
 public:
  void OnPurgeMemory() override {
    ParkableImageManager::Instance().OnPurgeMemory();
  }
};

}  // namespace
#endif  // BUILDFLAG(IS_COBALT)

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
          MainThreadTaskRunnerRestricted())) {}

void ParkableImageManager::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner);
  task_runner_ = std::move(task_runner);
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
  base::AutoLock lock(lock_);

  has_pending_parking_task_ = false;
  has_posted_accounting_task_ = false;
  unparked_images_.clear();
  on_disk_images_.clear();
  allocator_for_testing_ = nullptr;
  total_disk_read_time_ = base::TimeDelta();
  total_disk_write_time_ = base::TimeDelta();
#if BUILDFLAG(IS_COBALT)
  renderer_backgrounded_ = false;
  renderer_hidden_ = false;
#endif  // BUILDFLAG(IS_COBALT)
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
  // Registered lazily, as for ParkableStrings, and outside |lock_|, since a
  // garbage-collected allocation can run finalizers that destroy images.
  if (!did_register_memory_pressure_listener_ && IsUnparkedBudgetEnabled()) {
    // The registry holds its clients weakly, so keep the listener alive.
    DEFINE_STATIC_LOCAL(
        Persistent<ParkableImagePurgeMemoryListener>, listener,
        (MakeGarbageCollected<ParkableImagePurgeMemoryListener>()));
    MemoryPressureListenerRegistry::Instance().RegisterClient(listener.Get());
    did_register_memory_pressure_listener_ = true;
  }
#endif  // BUILDFLAG(IS_COBALT)

  base::AutoLock lock(lock_);

#if BUILDFLAG(IS_COBALT)
  // A new image is empty, so it cannot push RAM over the budget.
  // OnImageFrozen() checks the budget once the image's size is final.
  if (!IsUnparkedBudgetActive()) {
    ScheduleDelayedParkingTaskIfNeeded();
  }
#else
  ScheduleDelayedParkingTaskIfNeeded();
#endif  // BUILDFLAG(IS_COBALT)

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

#if BUILDFLAG(IS_COBALT)
  const bool use_budget = IsUnparkedBudgetActive();
#endif  // BUILDFLAG(IS_COBALT)

  // We unlock here so that we can avoid a deadlock, since if the data for the
  // image is already written to disk, we can discard our copy of the data
  // synchronously, which calls back into the manager.
  lock_.Release();

#if BUILDFLAG(IS_COBALT)
  if (use_budget) {
    // Under the budget, the task runs again only while RAM is over the budget
    // and the swap file is writable. OnImageFrozen() and OnReadFromDisk()
    // schedule it when RAM grows.
    const bool should_reschedule_for_budget =
        ParkLeastRecentlyUsedImages(unparked_images, UnparkedBudgetBytes()) &&
        data_allocator().may_write();
    lock_.Acquire();
    has_pending_parking_task_ = false;
    if (should_reschedule_for_budget) {
      ScheduleDelayedParkingTaskIfNeeded();
    }
    return;
  }
#endif  // BUILDFLAG(IS_COBALT)

  bool should_reschedule = false;
  for (auto* image : unparked_images) {
    if (image->ShouldReschedule())
      should_reschedule = true;
    image->MaybePark(task_runner_);
  }

  lock_.Acquire();

  has_pending_parking_task_ = false;

  if (should_reschedule)
    ScheduleDelayedParkingTaskIfNeeded();
}

#if BUILDFLAG(IS_COBALT)
// static
bool ParkableImageManager::IsUnparkedBudgetEnabled() {
  return IsParkableImagesToDiskEnabled() &&
         base::FeatureList::IsEnabled(
             features::kCobaltParkableMemoryOptimization);
}

// static
size_t ParkableImageManager::UnparkedBudgetBytes() {
  // 0 parks every parkable image. 64 MiB is far more encoded image data than
  // a TV app keeps alive, so it means no foreground parking.
  constexpr int kMaxUnparkedKb = 64 * 1024;
  const int unparked_kb =
      std::clamp(features::kCobaltParkableMemoryOptimizationMaxUnparkedKb.Get(),
                 0, kMaxUnparkedKb);
  return static_cast<size_t>(unparked_kb) * 1024;
}

void ParkableImageManager::SetRendererBackgrounded(bool backgrounded) {
  DCHECK(IsMainThread());
  base::AutoLock lock(lock_);
  const bool was_budget_active = IsUnparkedBudgetActive();
  renderer_backgrounded_ = backgrounded;
  OnRendererStateChanged(was_budget_active);
}

void ParkableImageManager::SetRendererHidden(bool hidden) {
  DCHECK(IsMainThread());
  base::AutoLock lock(lock_);
  const bool was_budget_active = IsUnparkedBudgetActive();
  renderer_hidden_ = hidden;
  OnRendererStateChanged(was_budget_active);
}

void ParkableImageManager::OnRendererStateChanged(bool was_budget_active) {
  const bool is_budget_active = IsUnparkedBudgetActive();
  if (was_budget_active == is_budget_active) {
    return;
  }
  VLOG(1) << "ParkableImageManager: RAM budget "
          << (is_budget_active ? "on" : "off")
          << " (backgrounded=" << renderer_backgrounded_
          << ", hidden=" << renderer_hidden_ << ")";
  if (!is_budget_active) {
    // Park every parkable image, as without the budget. Parking back in the
    // foreground follows the budget again, so there is nothing to do then.
    ScheduleDelayedParkingTaskIfNeeded();
  }
}

bool ParkableImageManager::IsUnparkedBudgetActive() const {
  return IsUnparkedBudgetEnabled() && !renderer_backgrounded_ &&
         !renderer_hidden_;
}

void ParkableImageManager::OnImageFrozen() {
  DCHECK(IsMainThread());
  base::AutoLock lock(lock_);
  if (!IsUnparkedBudgetActive()) {
    // Add() already scheduled the parking task.
    return;
  }
  size_t unparked_size = 0;
  for (ParkableImageImpl* image : unparked_images_) {
    unparked_size += image->size();
  }
  if (unparked_size > UnparkedBudgetBytes()) {
    ScheduleDelayedParkingTaskIfNeeded();
  }
}

void ParkableImageManager::OnPurgeMemory() {
  DCHECK(IsMainThread());
  if (!IsUnparkedBudgetEnabled()) {
    return;
  }
  VLOG(1) << "ParkableImageManager: parking all images on critical memory "
             "pressure";
  WTF::Vector<ParkableImageImpl*> unparked_images;
  {
    base::AutoLock lock(lock_);
    unparked_images = WTF::Vector<ParkableImageImpl*>(unparked_images_);
  }
  // A budget of 0 parks every image that can be parked now. The parking task
  // handles the rest.
  ParkLeastRecentlyUsedImages(unparked_images, /*budget=*/0);
}

bool ParkableImageManager::ParkLeastRecentlyUsedImages(
    const WTF::Vector<ParkableImageImpl*>& unparked_images,
    size_t budget) {
  // As in MaybeParkImages(), the raw pointers stay valid because images are
  // only destroyed on the main thread.
  DCHECK(IsMainThread());

  WTF::Vector<std::pair<base::TimeTicks, ParkableImageImpl*>> candidates;
  candidates.reserve(unparked_images.size());
  size_t unparked_size = 0;
  for (ParkableImageImpl* image : unparked_images) {
    base::AutoLock image_lock(image->lock_);
    // The data is released once the write completes, so count it as parked.
    if (image->background_task_in_progress_) {
      continue;
    }
    unparked_size += image->size();
    candidates.emplace_back(image->LastUsedTime(), image);
  }
  if (unparked_size <= budget) {
    return false;
  }

  std::stable_sort(
      candidates.begin(), candidates.end(),
      [](const auto& a, const auto& b) { return a.first < b.first; });
  for (const auto& candidate : candidates) {
    if (unparked_size <= budget) {
      break;
    }
    ParkableImageImpl* image = candidate.second;
    if (image->MaybePark(task_runner_)) {
      unparked_size -= image->size();
    }
  }
  return unparked_size > budget;
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace blink
