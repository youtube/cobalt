// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_POISSON_ALLOCATION_SAMPLER_H_
#define BASE_SAMPLING_HEAP_PROFILER_POISSON_ALLOCATION_SAMPLER_H_

#include <atomic>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/dispatcher/reentry_guard.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace heap_profiling {
class HeapProfilerControllerTest;
}

namespace base {

class SamplingHeapProfilerTest;

// This singleton class implements Poisson sampling of the incoming allocations
// stream. It hooks onto base::allocator and base::PartitionAlloc.
// The only control parameter is sampling interval that controls average value
// of the sampling intervals. The actual intervals between samples are
// randomized using Poisson distribution to mitigate patterns in the allocation
// stream.
// Once accumulated allocation sizes fill up the current sample interval,
// a sample is generated and sent to the observers via |SampleAdded| call.
// When the corresponding memory that triggered the sample is freed observers
// get notified with |SampleRemoved| call.
//
class BASE_EXPORT PoissonAllocationSampler {
 public:
  class SamplesObserver {
   public:
    virtual ~SamplesObserver() = default;
    virtual void SampleAdded(
        void* address,
        size_t size,
        size_t total,
        base::allocator::dispatcher::AllocationSubsystem type,
        const char* context) = 0;
    virtual void SampleRemoved(void* address) = 0;
  };

  // An instance of this class makes the sampler not report samples generated
  // within the object scope for the current thread.
  // It allows observers to allocate/deallocate memory while holding a lock
  // without a chance to get into reentrancy problems.
  // The current implementation doesn't support ScopedMuteThreadSamples nesting.
  class BASE_EXPORT ScopedMuteThreadSamples {
   public:
    ScopedMuteThreadSamples();
    ~ScopedMuteThreadSamples();

    ScopedMuteThreadSamples(const ScopedMuteThreadSamples&) = delete;
    ScopedMuteThreadSamples& operator=(const ScopedMuteThreadSamples&) = delete;

    static bool IsMuted();
  };

  // An instance of this class makes the sampler behave deterministically to
  // ensure test results are repeatable. Does not support nesting.
  class BASE_EXPORT ScopedSuppressRandomnessForTesting {
   public:
    ScopedSuppressRandomnessForTesting();
    ~ScopedSuppressRandomnessForTesting();

    ScopedSuppressRandomnessForTesting(
        const ScopedSuppressRandomnessForTesting&) = delete;
    ScopedSuppressRandomnessForTesting& operator=(
        const ScopedSuppressRandomnessForTesting&) = delete;

    static bool IsSuppressed();
  };

  // Must be called early during the process initialization. It creates and
  // reserves a TLS slot.
  static void Init();

  void AddSamplesObserver(SamplesObserver*);

  // Note: After an observer is removed it is still possible to receive
  // a notification to that observer. This is not a problem currently as
  // the only client of this interface is the base::SamplingHeapProfiler,
  // which is a singleton.
  // If there's a need for this functionality in the future, one might
  // want to put observers notification loop under a reader-writer lock.
  void RemoveSamplesObserver(SamplesObserver*);

  // Sets the mean number of bytes that will be allocated before taking a
  // sample.
  void SetSamplingInterval(size_t sampling_interval_bytes);

  // Returns the current mean sampling interval, in bytes.
  size_t SamplingInterval() const;

#if !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)
  ALWAYS_INLINE static void RecordAlloc(
      void* address,
      size_t,
      base::allocator::dispatcher::AllocationSubsystem,
      const char* context);
  ALWAYS_INLINE static void RecordFree(void* address);
#endif

  ALWAYS_INLINE void OnAllocation(
      void* address,
      size_t,
      base::allocator::dispatcher::AllocationSubsystem,
      const char* context);
  ALWAYS_INLINE void OnFree(void* address);

  static PoissonAllocationSampler* Get();

  PoissonAllocationSampler(const PoissonAllocationSampler&) = delete;
  PoissonAllocationSampler& operator=(const PoissonAllocationSampler&) = delete;

  // Returns true if a ScopedMuteHookedSamplesForTesting exists. Only friends
  // can create a ScopedMuteHookedSamplesForTesting but anyone can check the
  // status of this. This can be read from any thread.
  static bool AreHookedSamplesMuted() {
    return profiling_state_.load(std::memory_order_relaxed) &
           ProfilingStateFlag::kHookedSamplesMutedForTesting;
  }

 private:
  // Flags recording the state of the profiler. This does not use enum class so
  // flags can be used in a bitmask.
  enum ProfilingStateFlag {
    // Set if profiling has ever been started in this session of Chrome. Once
    // this is set, it is never reset. This is used to optimize the common case
    // where profiling is never used.
    kWasStarted = 1 << 0,
    // Set if profiling is currently running. This flag is toggled on and off
    // as sample observers are added and removed.
    kIsRunning = 1 << 1,
    // Set if a ScopedMuteHookedSamplesForTesting object exists.
    kHookedSamplesMutedForTesting = 1 << 2,
  };
  using ProfilingStateFlagMask = int;

  // An instance of this class makes the sampler only report samples with
  // AllocatorType kManualForTesting, not those from hooked allocators. This
  // allows unit tests to set test expectations based on only explicit calls to
  // RecordAlloc and RecordFree.
  //
  // The accumulated bytes on the thread that creates a
  // ScopedMuteHookedSamplesForTesting will also be reset to 0, and restored
  // when the object leaves scope. This gives tests a known state to start
  // recording samples on one thread: a full interval must pass to record a
  // sample. Other threads will still have a random number of accumulated bytes.
  //
  // Only one instance may exist at a time.
  class BASE_EXPORT ScopedMuteHookedSamplesForTesting {
   public:
    ScopedMuteHookedSamplesForTesting();
    ~ScopedMuteHookedSamplesForTesting();

    ScopedMuteHookedSamplesForTesting(
        const ScopedMuteHookedSamplesForTesting&) = delete;
    ScopedMuteHookedSamplesForTesting& operator=(
        const ScopedMuteHookedSamplesForTesting&) = delete;

   private:
    intptr_t accumulated_bytes_snapshot_;
  };

  PoissonAllocationSampler();
  ~PoissonAllocationSampler() = delete;

  static size_t GetNextSampleInterval(size_t base_interval);

  // Return the set of sampled addresses. This is only valid to call after
  // Init().
  static LockFreeAddressHashSet& sampled_addresses_set();

  // Atomically adds `flag` to `profiling_state_`. DCHECK's if it was already
  // set. If `flag` is kIsRunning, also sets kWasStarted. Uses
  // std::memory_order_relaxed semantics and therefore doesn't synchronize the
  // state of any other memory with future readers. (See the comment in
  // RecordFree() for why this is safe.)
  static void SetProfilingStateFlag(ProfilingStateFlag flag);

  // Atomically removes `flag` from `profiling_state_`. DCHECK's if it was not
  // already set. Uses std::memory_order_relaxed semantics and therefore doesn't
  // synchronize the state of any other memory with future readers. (See the
  // comment in RecordFree() for why this is safe.)
  static void ResetProfilingStateFlag(ProfilingStateFlag flag);

  void DoRecordAllocation(const ProfilingStateFlagMask state,
                          void* address,
                          size_t size,
                          base::allocator::dispatcher::AllocationSubsystem type,
                          const char* context);
  void DoRecordFree(void* address);

  void BalanceAddressesHashSet();

  Lock mutex_;

  // The |observers_| list is guarded by |mutex_|, however a copy of it
  // is made before invoking the observers (to avoid performing expensive
  // operations under the lock) as such the SamplesObservers themselves need
  // to be thread-safe and support being invoked racily after
  // RemoveSamplesObserver().
  std::vector<SamplesObserver*> observers_ GUARDED_BY(mutex_);

#if !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)
  static PoissonAllocationSampler* instance_;
#endif

  // Fast, thread-safe access to the current profiling state.
  static std::atomic<ProfilingStateFlagMask> profiling_state_;

  friend class heap_profiling::HeapProfilerControllerTest;
  friend class NoDestructor<PoissonAllocationSampler>;
  friend class PoissonAllocationSamplerStateTest;
  friend class SamplingHeapProfilerTest;
  FRIEND_TEST_ALL_PREFIXES(PoissonAllocationSamplerTest, MuteHooksWithoutInit);
  FRIEND_TEST_ALL_PREFIXES(SamplingHeapProfilerTest, HookedAllocatorMuted);
};

#if !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)
// static
ALWAYS_INLINE void PoissonAllocationSampler::RecordAlloc(
    void* address,
    size_t size,
    base::allocator::dispatcher::AllocationSubsystem type,
    const char* context) {
  instance_->OnAllocation(address, size, type, context);
}

// static
ALWAYS_INLINE void PoissonAllocationSampler::RecordFree(void* address) {
  instance_->OnFree(address);
}
#endif

ALWAYS_INLINE void PoissonAllocationSampler::OnAllocation(
    void* address,
    size_t size,
    base::allocator::dispatcher::AllocationSubsystem type,
    const char* context) {
  // The allocation hooks may be installed before the sampler is started. Check
  // if its ever been started first to avoid extra work on the fast path,
  // because it's the most common case.
  const ProfilingStateFlagMask state =
      profiling_state_.load(std::memory_order_relaxed);
  if (LIKELY(!(state & ProfilingStateFlag::kWasStarted))) {
    return;
  }

  // When sampling is muted for testing, only handle manual calls to
  // RecordAlloc. (This doesn't need to be checked in RecordFree because muted
  // allocations won't be added to sampled_addresses_set(), so RecordFree
  // already skips them.)
  if (UNLIKELY((state & ProfilingStateFlag::kHookedSamplesMutedForTesting) &&
               type != base::allocator::dispatcher::AllocationSubsystem::
                           kManualForTesting)) {
    return;
  }

  // Note: ReentryGuard prevents from recursions introduced by malloc and
  // initialization of thread local storage which happen in the allocation path
  // only (please see docs of ReentryGuard for full details).
  allocator::dispatcher::ReentryGuard reentry_guard;

  if (UNLIKELY(!reentry_guard)) {
    return;
  }

  DoRecordAllocation(state, address, size, type, context);
}

ALWAYS_INLINE void PoissonAllocationSampler::OnFree(void* address) {
  // The allocation hooks may be installed before the sampler is started. Check
  // if its ever been started first to avoid extra work on the fast path,
  // because it's the most common case. Note that DoRecordFree still needs to be
  // called if the sampler was started but is now stopped, to track allocations
  // that were recorded while the sampler was still running.
  //
  // Relaxed ordering is safe here because there's only one case where
  // RecordAlloc and RecordFree MUST see the same value of `profiling_state_`.
  // Assume thread A updates `profiling_state_` from 0 to kWasStarted |
  // kIsRunning, thread B calls RecordAlloc, and thread C calls RecordFree.
  // (Something else could update `profiling_state_` to remove kIsRunning before
  // RecordAlloc or RecordFree.)
  //
  // 1. If RecordAlloc(p) sees !kWasStarted or !kIsRunning it will return
  //    immediately, so p won't be in sampled_address_set(). So no matter what
  //    RecordFree(p) sees it will also return immediately.
  //
  // 2. If RecordFree() is called with a pointer that was never passed to
  //    RecordAlloc(), again it will return immediately no matter what it sees.
  //
  // 3. If RecordAlloc(p) sees kIsRunning it will put p in
  //    sampled_address_set(). In this case RecordFree(p) MUST see !kWasStarted
  //    or it will return without removing p:
  //
  //    3a. If the program got p as the return value from malloc() and passed it
  //        to free(), then RecordFree() happens-after RecordAlloc() and
  //        therefore will see the same value of `profiling_state_` as
  //        RecordAlloc() for all memory orders. (Proof: using the definitions
  //        of sequenced-after, happens-after and inter-thread happens-after
  //        from https://en.cppreference.com/w/cpp/atomic/memory_order, malloc()
  //        calls RecordAlloc() so its return is sequenced-after RecordAlloc();
  //        free() inter-thread happens-after malloc's return because it
  //        consumes the result; RecordFree() is sequenced-after its caller,
  //        free(); therefore RecordFree() interthread happens-after
  //        RecordAlloc().)
  //    3b. If the program is freeing a random pointer which coincidentally was
  //        also returned from malloc(), such that free(p) does not happen-after
  //        malloc(), then there is already an unavoidable race condition. If
  //        the profiler sees malloc() before free(p), then it will add p to
  //        sampled_addresses_set() and then remove it; otherwise it will do
  //        nothing in RecordFree() and add p to sampled_addresses_set() in
  //        RecordAlloc(), recording a potential leak. Reading
  //        `profiling_state_` with relaxed ordering adds another possibility:
  //        if the profiler sees malloc() with kWasStarted and then free without
  //        kWasStarted, it will add p to sampled_addresses_set() in
  //        RecordAlloc() and then do nothing in RecordFree(). This has the same
  //        outcome as the existing race.
  const ProfilingStateFlagMask state =
      profiling_state_.load(std::memory_order_relaxed);
  if (LIKELY(!(state & ProfilingStateFlag::kWasStarted))) {
    return;
  }
  if (UNLIKELY(address == nullptr)) {
    return;
  }
  if (LIKELY(!sampled_addresses_set().Contains(address))) {
    return;
  }
  if (UNLIKELY(ScopedMuteThreadSamples::IsMuted())) {
    return;
  }

  // Note: ReentryGuard prevents from recursions introduced by malloc and
  // initialization of thread local storage which happen in the allocation path
  // only (please see docs of ReentryGuard for full details). Therefore, the
  // DoNotifyFree doesn't need to be guarded.

  DoRecordFree(address);
}

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_POISSON_ALLOCATION_SAMPLER_H_
