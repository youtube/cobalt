// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StatisticsRecorder holds all Histograms and BucketRanges that are used by
// Histograms in the system. It provides a general place for
// Histograms/BucketRanges to register, and supports a global API for accessing
// (i.e., dumping, or graphing) the data.

#ifndef BASE_METRICS_STATISTICS_RECORDER_H_
#define BASE_METRICS_STATISTICS_RECORDER_H_

#include <stdint.h>

#include <atomic>  // For std::memory_order_*.
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/ranges_manager.h"
#include "base/metrics/record_histogram_checker.h"
#include "base/observer_list_threadsafe.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"

namespace base {

class BucketRanges;
class HistogramSnapshotManager;

// In-memory recorder of usage statistics (aka metrics, aka histograms).
//
// All the public methods are static and act on a global recorder. This global
// recorder is internally synchronized and all the static methods are thread
// safe. This is intended to only be run/used in the browser process.
//
// StatisticsRecorder doesn't have any public constructor. For testing purpose,
// you can create a temporary recorder using the factory method
// CreateTemporaryForTesting(). This temporary recorder becomes the global one
// until deleted. When this temporary recorder is deleted, it restores the
// previous global one.
class BASE_EXPORT StatisticsRecorder {
 public:
  // An interface class that allows the StatisticsRecorder to forcibly merge
  // histograms from providers when necessary.
  class HistogramProvider {
   public:
    // Merges all histogram information into the global versions. If |async| is
    // true, the work may be done asynchronously (though this is not mandatory).
    // If false, the work must be done ASAP/synchronously (e.g., because the
    // browser is shutting down). |done_callback| should be called on the
    // calling thread when all work is finished, regardless of the value of
    // |async|.
    //
    // NOTE: It is possible for this to be called with |async| set to false
    // even before a previous call with |async| set to true has finished. Hence,
    // if the implementation allows for asynchronous work, ensure that it is
    // done in a thread-safe way.
    virtual void MergeHistogramDeltas(bool async,
                                      OnceClosure done_callback) = 0;
  };

  // OnSampleCallback is a convenient callback type that provides information
  // about a histogram sample. This is used in conjunction with
  // ScopedHistogramSampleObserver to get notified when a sample is collected.
  using OnSampleCallback =
      base::RepeatingCallback<void(const char* /*=histogram_name*/,
                                   uint64_t /*=name_hash*/,
                                   HistogramBase::Sample)>;

  // An observer that gets notified whenever a new sample is recorded for a
  // particular histogram. Clients only need to construct it with the histogram
  // name and the callback to be invoked. The class starts observing on
  // construction and removes itself from the observer list on destruction. The
  // clients are always notified on the same sequence in which they were
  // registered.
  class BASE_EXPORT ScopedHistogramSampleObserver {
   public:
    // Constructor. Called with the desired histogram name and the callback to
    // be invoked when a sample is recorded.
    explicit ScopedHistogramSampleObserver(const std::string& histogram_name,
                                           OnSampleCallback callback);
    ~ScopedHistogramSampleObserver();

   private:
    friend class StatisticsRecorder;

    // Runs the callback.
    void RunCallback(const char* histogram_name,
                     uint64_t name_hash,
                     HistogramBase::Sample sample);

    // The name of the histogram to observe.
    const std::string histogram_name_;

    // The client supplied callback that is invoked when the histogram sample is
    // collected.
    const OnSampleCallback callback_;
  };

  typedef std::vector<HistogramBase*> Histograms;
  typedef size_t SnapshotTransactionId;

  StatisticsRecorder(const StatisticsRecorder&) = delete;
  StatisticsRecorder& operator=(const StatisticsRecorder&) = delete;

  // Restores the previous global recorder.
  //
  // When several temporary recorders are created using
  // CreateTemporaryForTesting(), these recorders must be deleted in reverse
  // order of creation.
  //
  // This method is thread safe.
  //
  // Precondition: The recorder being deleted is the current global recorder.
  ~StatisticsRecorder();

  // Registers a provider of histograms that can be called to merge those into
  // the global recorder. Calls to ImportProvidedHistograms() will fetch from
  // registered providers.
  //
  // This method is thread safe.
  static void RegisterHistogramProvider(
      const WeakPtr<HistogramProvider>& provider);

  // Registers or adds a new histogram to the collection of statistics. If an
  // identically named histogram is already registered, then the argument
  // |histogram| will be deleted. The returned value is always the registered
  // histogram (either the argument, or the pre-existing registered histogram).
  //
  // This method is thread safe.
  static HistogramBase* RegisterOrDeleteDuplicate(HistogramBase* histogram);

  // Registers or adds a new BucketRanges. If an equivalent BucketRanges is
  // already registered, then the argument |ranges| will be deleted. The
  // returned value is always the registered BucketRanges (either the argument,
  // or the pre-existing one).
  //
  // This method is thread safe.
  static const BucketRanges* RegisterOrDeleteDuplicateRanges(
      const BucketRanges* ranges);

  // A method for appending histogram data to a string. Only histograms which
  // have |query| as a substring are written to |output| (an empty string will
  // process all registered histograms).
  //
  // This method is thread safe.
  static void WriteGraph(const std::string& query, std::string* output);

  // Returns the histograms with |verbosity_level| as the serialization
  // verbosity.
  //
  // This method is thread safe.
  static std::string ToJSON(JSONVerbosityLevel verbosity_level);

  // Gets existing histograms. |include_persistent| determines whether
  // histograms held in persistent storage are included.
  //
  // The order of returned histograms is not guaranteed.
  //
  // Ownership of the individual histograms remains with the StatisticsRecorder.
  //
  // This method is thread safe.
  static Histograms GetHistograms(bool include_persistent = true)
      LOCKS_EXCLUDED(GetLock());

  // Gets BucketRanges used by all histograms registered. The order of returned
  // BucketRanges is not guaranteed.
  //
  // This method is thread safe.
  static std::vector<const BucketRanges*> GetBucketRanges();

  // Finds a histogram by name. Matches the exact name. Returns a null pointer
  // if a matching histogram is not found.
  //
  // This method is thread safe.
  static HistogramBase* FindHistogram(base::StringPiece name);

  // Imports histograms from providers. If |async| is true, the providers may do
  // the work asynchronously (though this is not guaranteed and it is up to the
  // providers to decide). If false, the work will be done synchronously.
  // |done_callback| is called on the calling thread when all providers have
  // finished.
  //
  // This method must be called on the UI thread.
  static void ImportProvidedHistograms(bool async, OnceClosure done_callback);

  // Convenience function that calls ImportProvidedHistograms() with |async|
  // set to false, and with a no-op |done_callback|.
  static void ImportProvidedHistogramsSync();

  // Snapshots all histogram deltas via |snapshot_manager|. This marks the
  // deltas as logged. |include_persistent| determines whether histograms held
  // in persistent storage are snapshotted. |flags_to_set| is used to set flags
  // for each histogram. |required_flags| is used to select which histograms to
  // record. Only histograms with all required flags are selected. If all
  // histograms should be recorded, use |Histogram::kNoFlags| as the required
  // flag. This is logically equivalent to calling SnapshotUnloggedSamples()
  // followed by HistogramSnapshotManager::MarkUnloggedSamplesAsLogged() on
  // |snapshot_manager|. Returns the snapshot transaction ID associated with
  // this operation. Thread-safe.
  static SnapshotTransactionId PrepareDeltas(
      bool include_persistent,
      HistogramBase::Flags flags_to_set,
      HistogramBase::Flags required_flags,
      HistogramSnapshotManager* snapshot_manager)
      LOCKS_EXCLUDED(snapshot_lock_.Pointer());

  // Same as PrepareDeltas() above, but the samples are not marked as logged.
  // This includes persistent histograms, and no flags will be set. A call to
  // HistogramSnapshotManager::MarkUnloggedSamplesAsLogged() on the passed
  // |snapshot_manager| should be made to mark them as logged. Returns the
  // snapshot transaction ID associated with this operation. Thread-safe.
  static SnapshotTransactionId SnapshotUnloggedSamples(
      HistogramBase::Flags required_flags,
      HistogramSnapshotManager* snapshot_manager)
      LOCKS_EXCLUDED(snapshot_lock_.Pointer());

  // Returns the transaction ID of the last snapshot performed (either through
  // PrepareDeltas() or SnapshotUnloggedSamples()). Returns 0 if a snapshot was
  // never taken so far. Thread-safe.
  static SnapshotTransactionId GetLastSnapshotTransactionId()
      LOCKS_EXCLUDED(snapshot_lock_.Pointer());

  // Retrieves and runs the list of callbacks for the histogram referred to by
  // |histogram_name|, if any.
  //
  // This method is thread safe.
  static void FindAndRunHistogramCallbacks(base::PassKey<HistogramBase>,
                                           const char* histogram_name,
                                           uint64_t name_hash,
                                           HistogramBase::Sample sample);

  // Returns the number of known histograms.
  //
  // This method is thread safe.
  static size_t GetHistogramCount();

  // Initializes logging histograms with --v=1. Safe to call multiple times.
  // Is called from ctor but for browser it seems that it is more useful to
  // start logging after statistics recorder, so we need to init log-on-shutdown
  // later.
  //
  // This method is thread safe.
  static void InitLogOnShutdown();

  // Removes a histogram from the internal set of known ones. This can be
  // necessary during testing persistent histograms where the underlying
  // memory is being released.
  //
  // This method is thread safe.
  static void ForgetHistogramForTesting(base::StringPiece name);

  // Creates a temporary StatisticsRecorder object for testing purposes. All new
  // histograms will be registered in it until it is destructed or pushed aside
  // for the lifetime of yet another StatisticsRecorder object. The destruction
  // of the returned object will re-activate the previous one.
  // StatisticsRecorder objects must be deleted in the opposite order to which
  // they're created.
  //
  // This method is thread safe.
  [[nodiscard]] static std::unique_ptr<StatisticsRecorder>
  CreateTemporaryForTesting();

  // Sets the record checker for determining if a histogram should be recorded.
  // Record checker doesn't affect any already recorded histograms, so this
  // method must be called very early, before any threads have started.
  // Record checker methods can be called on any thread, so they shouldn't
  // mutate any state.
  static void SetRecordChecker(
      std::unique_ptr<RecordHistogramChecker> record_checker);

  // Checks if the given histogram should be recorded based on the
  // ShouldRecord() method of the record checker. If the record checker is not
  // set, returns true.
  // |histogram_hash| corresponds to the result of HashMetricNameAs32Bits().
  //
  // This method is thread safe.
  static bool ShouldRecordHistogram(uint32_t histogram_hash);

  // Sorts histograms by name.
  static Histograms Sort(Histograms histograms);

  // Filters histograms by name. Only histograms which have |query| as a
  // substring in their name are kept. An empty query keeps all histograms.
  // |case_sensitive| determines whether the matching should be done in a
  // case sensitive way.
  static Histograms WithName(Histograms histograms,
                             const std::string& query,
                             bool case_sensitive = true);

  using GlobalSampleCallback = void (*)(const char* /*=histogram_name*/,
                                        uint64_t /*=name_hash*/,
                                        HistogramBase::Sample);
  // Installs a global callback which will be called for every added
  // histogram sample. The given callback is a raw function pointer in order
  // to be accessed lock-free and can be called on any thread.
  static void SetGlobalSampleCallback(
      const GlobalSampleCallback& global_sample_callback);

  // Returns the global callback, if any, that should be called every time a
  // histogram sample is added.
  static GlobalSampleCallback global_sample_callback() {
    return global_sample_callback_.load(std::memory_order_relaxed);
  }

  // Returns whether there's either a global histogram callback set,
  // or if any individual histograms have callbacks set. Used for early return
  // when histogram samples are added.
  static bool have_active_callbacks() {
    return have_active_callbacks_.load(std::memory_order_relaxed);
  }

#ifdef ARCH_CPU_64_BITS
  static base::TimeDelta GetAndClearTotalWaitTime() {
    return lock_.Get().GetAndClearTotalWaitTime();
  }
#endif  // ARCH_CPU_64_BITS

  // Returns the synthetic trial group name for the R/W lock trial being ran,
  // or an empty string if no trial is being run and should not be reported.
  static StringPiece GetLockTrialGroup();

 private:
  // Wrapper lock class that provides A/B testing between a base::Lock and a
  // std::shared_mutex and tracks lock wait times. Additionally, allows the use
  // of thread locking annotations, which are not otherwise supported by
  // std::shared_mutex.
  //
  // Note: std::shared_mutex is currently not generally allowed in Chromium but
  // this specific use has been explicitly discussed and agreed on
  // cxx@chromium.org here:
  // https://groups.google.com/a/chromium.org/g/cxx/c/bIlGr1URn8I/m/ftvVCQPiAQAJ
  class BASE_EXPORT LOCKABLE SrLock {
   public:
    SrLock() : use_shared_mutex_(ShouldUseSharedMutex()) {}
    ~SrLock() = default;

    void Acquire() EXCLUSIVE_LOCK_FUNCTION() {
      TimeTicks start = TimeTicks::Now();
      if (use_shared_mutex_) {
        mutex_.lock();
      } else {
        lock_.Acquire();
      }
      IncrementLockWaitTime(TimeTicks::Now() - start);
    }

    void Release() UNLOCK_FUNCTION() {
      if (use_shared_mutex_) {
        mutex_.unlock();
      } else {
        lock_.Release();
      }
    }

    void AcquireShared() SHARED_LOCK_FUNCTION() {
      TimeTicks start = TimeTicks::Now();
      if (use_shared_mutex_) {
        mutex_.lock_shared();
      } else {
        lock_.Acquire();
      }
      IncrementLockWaitTime(TimeTicks::Now() - start);
    }

    void ReleaseShared() UNLOCK_FUNCTION() {
      if (use_shared_mutex_) {
        mutex_.unlock_shared();
      } else {
        lock_.Release();
      }
    }

    void AssertAcquired() {
      if (use_shared_mutex_) {
        // Not available with std::shared_mutex. This can be implemented on top
        // of that API, similar to what base::Lock does.
      } else {
        lock_.AssertAcquired();
      }
    }

#ifdef ARCH_CPU_64_BITS
    TimeDelta GetAndClearTotalWaitTime() {
      return Microseconds(
          subtle::NoBarrier_AtomicExchange(&total_lock_wait_time_micros_, 0));
    }
#endif  // ARCH_CPU_64_BITS

    bool use_shared_mutex() const { return use_shared_mutex_; }

   private:
    // Determines if the shared mutex should be used. Should only be called
    // once when the lock is created.
    static bool ShouldUseSharedMutex();

    void IncrementLockWaitTime(TimeDelta delta) {
#ifdef ARCH_CPU_64_BITS
      subtle::NoBarrier_AtomicIncrement(&total_lock_wait_time_micros_,
                                        delta.InMicroseconds());
#endif  // ARCH_CPU_64_BITS
    }

#ifdef ARCH_CPU_64_BITS
    // Cumulative wait time on acquiring the lock (both R and W modes) since the
    // the last call to GetAndClearTotalWaitTime().
    // Note: Requires 64-bit arch for atomic increments.
    subtle::Atomic64 total_lock_wait_time_micros_ = 0;
#endif  // ARCH_CPU_64_BITS

    // If true, |mutex_| will be used in R/W mode; otherwise |lock_| is used.
    const bool use_shared_mutex_;
    std::shared_mutex mutex_;
    Lock lock_;
  };

  class SCOPED_LOCKABLE SrAutoReaderLock {
   public:
    explicit SrAutoReaderLock(SrLock& lock) EXCLUSIVE_LOCK_FUNCTION(lock)
        : lock_(lock) {
      lock_->AcquireShared();
    }

    ~SrAutoReaderLock() UNLOCK_FUNCTION() { lock_->ReleaseShared(); }

   private:
    raw_ref<SrLock> lock_;
  };

  using SrAutoWriterLock = internal::BasicAutoLock<SrLock>;
  static SrLock& GetLock() { return lock_.Get(); }
  static void AssertLockHeld() { lock_.Get().AssertAcquired(); }

  // Returns the histogram registered with |hash|, if there is one. Returns
  // nullptr otherwise.
  // Note: |name| is only used in DCHECK builds to assert that there was no
  // collision (i.e. different histograms with the same hash).
  HistogramBase* FindHistogramByHashInternal(uint64_t hash,
                                             StringPiece name) const
      EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  // Adds an observer to be notified when a new sample is recorded on the
  // histogram referred to by |histogram_name|. Can be called before or after
  // the histogram is created.
  //
  // This method is thread safe.
  static void AddHistogramSampleObserver(
      const std::string& histogram_name,
      ScopedHistogramSampleObserver* observer);

  // Clears the given |observer| set on the histogram referred to by
  // |histogram_name|.
  //
  // This method is thread safe.
  static void RemoveHistogramSampleObserver(
      const std::string& histogram_name,
      ScopedHistogramSampleObserver* observer);

  typedef std::vector<WeakPtr<HistogramProvider>> HistogramProviders;

  // A map of histogram name hash (see HashMetricName()) to histogram object.
  typedef std::unordered_map<uint64_t, HistogramBase*> HistogramMap;

  // A map of histogram name hash (see HashMetricName()) to registered observers
  // If the histogram isn't created yet, the observers will be added after
  // creation.
  using HistogramSampleObserverList =
      base::ObserverListThreadSafe<ScopedHistogramSampleObserver>;
  typedef std::unordered_map<uint64_t,
                             scoped_refptr<HistogramSampleObserverList>>
      ObserverMap;

  friend class StatisticsRecorderTest;
  FRIEND_TEST_ALL_PREFIXES(StatisticsRecorderTest, IterationTest);

  // Initializes the global recorder if it doesn't already exist. Safe to call
  // multiple times.
  static void EnsureGlobalRecorderWhileLocked()
      EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  // Gets histogram providers.
  //
  // This method is thread safe.
  static HistogramProviders GetHistogramProviders();

  // Imports histograms from global persistent memory.
  static void ImportGlobalPersistentHistograms() LOCKS_EXCLUDED(GetLock());

  // Constructs a new StatisticsRecorder and sets it as the current global
  // recorder.
  //
  // This singleton instance should be started during the single-threaded
  // portion of startup and hence it is not thread safe. It initializes globals
  // to provide support for all future calls.
  StatisticsRecorder() EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  // Initialize implementation but without lock. Caller should guard
  // StatisticsRecorder by itself if needed (it isn't in unit tests).
  static void InitLogOnShutdownWhileLocked()
      EXCLUSIVE_LOCKS_REQUIRED(GetLock());

  HistogramMap histograms_;
  ObserverMap observers_;
  HistogramProviders providers_;
  RangesManager ranges_manager_;
  std::unique_ptr<RecordHistogramChecker> record_checker_;

  // Previous global recorder that existed when this one was created.
  raw_ptr<StatisticsRecorder> previous_ = nullptr;

  // Global lock for internal synchronization.
  // Note: Care must be taken to not read or write anything to persistent memory
  // while holding this lock, as that could cause a file I/O stall.
  static LazyInstance<SrLock>::Leaky lock_;

  // Global lock for internal synchronization of histogram snapshots.
  static LazyInstance<base::Lock>::Leaky snapshot_lock_;

  // A strictly increasing number that is incremented every time a snapshot is
  // taken (by either calling SnapshotUnloggedSamples() or PrepareDeltas()).
  // This represents the transaction ID of the last snapshot taken.
  static SnapshotTransactionId last_snapshot_transaction_id_
      GUARDED_BY(snapshot_lock_.Get());

  // Current global recorder. This recorder is used by static methods. When a
  // new global recorder is created by CreateTemporaryForTesting(), then the
  // previous global recorder is referenced by top_->previous_.
  static StatisticsRecorder* top_ GUARDED_BY(GetLock());

  // Tracks whether InitLogOnShutdownWhileLocked() has registered a logging
  // function that will be called when the program finishes.
  static bool is_vlog_initialized_;

  // Track whether there are active histogram callbacks present.
  static std::atomic<bool> have_active_callbacks_;

  // Stores a raw callback which should be called on any every histogram sample
  // which gets added.
  static std::atomic<GlobalSampleCallback> global_sample_callback_;
};

}  // namespace base

#endif  // BASE_METRICS_STATISTICS_RECORDER_H_
