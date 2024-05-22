// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/statistics_recorder.h"

#include <memory>

#include "base/at_exit.h"
#include "base/containers/contains.h"
#include "base/debug/leak_annotations.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/record_histogram_checker.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace base {
namespace {

bool HistogramNameLesser(const base::HistogramBase* a,
                         const base::HistogramBase* b) {
  return strcmp(a->histogram_name(), b->histogram_name()) < 0;
}

}  // namespace

// static
LazyInstance<Lock>::Leaky StatisticsRecorder::lock_ = LAZY_INSTANCE_INITIALIZER;

// static
LazyInstance<base::Lock>::Leaky StatisticsRecorder::snapshot_lock_ =
    LAZY_INSTANCE_INITIALIZER;

// static
StatisticsRecorder::SnapshotTransactionId
    StatisticsRecorder::last_snapshot_transaction_id_ = 0;

// static
StatisticsRecorder* StatisticsRecorder::top_ = nullptr;

// static
bool StatisticsRecorder::is_vlog_initialized_ = false;

// static
std::atomic<bool> StatisticsRecorder::have_active_callbacks_{false};

// static
std::atomic<StatisticsRecorder::GlobalSampleCallback>
    StatisticsRecorder::global_sample_callback_{nullptr};

StatisticsRecorder::ScopedHistogramSampleObserver::
    ScopedHistogramSampleObserver(const std::string& name,
                                  OnSampleCallback callback)
    : histogram_name_(name), callback_(callback) {
  StatisticsRecorder::AddHistogramSampleObserver(histogram_name_, this);
}

StatisticsRecorder::ScopedHistogramSampleObserver::
    ~ScopedHistogramSampleObserver() {
  StatisticsRecorder::RemoveHistogramSampleObserver(histogram_name_, this);
}

void StatisticsRecorder::ScopedHistogramSampleObserver::RunCallback(
    const char* histogram_name,
    uint64_t name_hash,
    HistogramBase::Sample sample) {
  callback_.Run(histogram_name, name_hash, sample);
}

StatisticsRecorder::~StatisticsRecorder() {
  const AutoLock auto_lock(lock_.Get());
  DCHECK_EQ(this, top_);
  top_ = previous_;
}

// static
void StatisticsRecorder::EnsureGlobalRecorderWhileLocked() {
  lock_.Get().AssertAcquired();
  if (top_)
    return;

  const StatisticsRecorder* const p = new StatisticsRecorder;
  // The global recorder is never deleted.
  ANNOTATE_LEAKING_OBJECT_PTR(p);
  DCHECK_EQ(p, top_);
}

// static
void StatisticsRecorder::RegisterHistogramProvider(
    const WeakPtr<HistogramProvider>& provider) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();
  top_->providers_.push_back(provider);
}

// static
HistogramBase* StatisticsRecorder::RegisterOrDeleteDuplicate(
    HistogramBase* histogram) {
  // Declared before |auto_lock| to ensure correct destruction order.
  std::unique_ptr<HistogramBase> histogram_deleter;
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  const char* const name = histogram->histogram_name();
  HistogramBase*& registered = top_->histograms_[name];

  if (!registered) {
    // |name| is guaranteed to never change or be deallocated so long
    // as the histogram is alive (which is forever).
    registered = histogram;
    ANNOTATE_LEAKING_OBJECT_PTR(histogram);  // see crbug.com/79322
    // If there are callbacks for this histogram, we set the kCallbackExists
    // flag.
    if (base::Contains(top_->observers_, name))
      histogram->SetFlags(HistogramBase::kCallbackExists);

    return histogram;
  }

  if (histogram == registered) {
    // The histogram was registered before.
    return histogram;
  }

  // We already have one histogram with this name.
  histogram_deleter.reset(histogram);
  return registered;
}

// static
const BucketRanges* StatisticsRecorder::RegisterOrDeleteDuplicateRanges(
    const BucketRanges* ranges) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  const BucketRanges* const registered =
      top_->ranges_manager_.RegisterOrDeleteDuplicateRanges(ranges);

  if (registered == ranges)
    ANNOTATE_LEAKING_OBJECT_PTR(ranges);

  return registered;
}

// static
void StatisticsRecorder::WriteGraph(const std::string& query,
                                    std::string* output) {
  if (query.length())
    StringAppendF(output, "Collections of histograms for %s\n", query.c_str());
  else
    output->append("Collections of all histograms\n");

  for (const HistogramBase* const histogram :
       Sort(WithName(GetHistograms(), query))) {
    histogram->WriteAscii(output);
    output->append("\n");
  }
}

// static
std::string StatisticsRecorder::ToJSON(JSONVerbosityLevel verbosity_level) {
  std::string output = "{\"histograms\":[";
  const char* sep = "";
  for (const HistogramBase* const histogram : Sort(GetHistograms())) {
    output += sep;
    sep = ",";
    std::string json;
    histogram->WriteJSON(&json, verbosity_level);
    output += json;
  }
  output += "]}";
  return output;
}

// static
std::vector<const BucketRanges*> StatisticsRecorder::GetBucketRanges() {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  return top_->ranges_manager_.GetBucketRanges();
}

// static
HistogramBase* StatisticsRecorder::FindHistogram(base::StringPiece name) {
  // This must be called *before* the lock is acquired below because it will
  // call back into this object to register histograms. Those called methods
  // will acquire the lock at that time.
  ImportGlobalPersistentHistograms();

  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  const HistogramMap::const_iterator it = top_->histograms_.find(name);
  return it != top_->histograms_.end() ? it->second : nullptr;
}

// static
StatisticsRecorder::HistogramProviders
StatisticsRecorder::GetHistogramProviders() {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();
  return top_->providers_;
}

// static
void StatisticsRecorder::ImportProvidedHistograms() {
  // Merge histogram data from each provider in turn.
  for (const WeakPtr<HistogramProvider>& provider : GetHistogramProviders()) {
    // Weak-pointer may be invalid if the provider was destructed, though they
    // generally never are.
    if (provider)
      provider->MergeHistogramDeltas();
  }
}

// static
StatisticsRecorder::SnapshotTransactionId StatisticsRecorder::PrepareDeltas(
    bool include_persistent,
    HistogramBase::Flags flags_to_set,
    HistogramBase::Flags required_flags,
    HistogramSnapshotManager* snapshot_manager) {
  Histograms histograms = Sort(GetHistograms(include_persistent));
  base::AutoLock lock(snapshot_lock_.Get());
  snapshot_manager->PrepareDeltas(std::move(histograms), flags_to_set,
                                  required_flags);
  return ++last_snapshot_transaction_id_;
}

// static
StatisticsRecorder::SnapshotTransactionId
StatisticsRecorder::SnapshotUnloggedSamples(
    HistogramBase::Flags required_flags,
    HistogramSnapshotManager* snapshot_manager) {
  Histograms histograms = Sort(GetHistograms());
  base::AutoLock lock(snapshot_lock_.Get());
  snapshot_manager->SnapshotUnloggedSamples(std::move(histograms),
                                            required_flags);
  return ++last_snapshot_transaction_id_;
}

// static
StatisticsRecorder::SnapshotTransactionId
StatisticsRecorder::GetLastSnapshotTransactionId() {
  base::AutoLock lock(snapshot_lock_.Get());
  return last_snapshot_transaction_id_;
}

// static
void StatisticsRecorder::InitLogOnShutdown() {
  const AutoLock auto_lock(lock_.Get());
  InitLogOnShutdownWhileLocked();
}

// static
void StatisticsRecorder::AddHistogramSampleObserver(
    const std::string& name,
    StatisticsRecorder::ScopedHistogramSampleObserver* observer) {
  DCHECK(observer);
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  auto iter = top_->observers_.find(name);
  if (iter == top_->observers_.end()) {
    top_->observers_.insert(
        {name, base::MakeRefCounted<HistogramSampleObserverList>()});
  }

  top_->observers_[name]->AddObserver(observer);

  const HistogramMap::const_iterator it = top_->histograms_.find(name);
  if (it != top_->histograms_.end())
    it->second->SetFlags(HistogramBase::kCallbackExists);

  have_active_callbacks_.store(
      global_sample_callback() || !top_->observers_.empty(),
      std::memory_order_relaxed);
}

// static
void StatisticsRecorder::RemoveHistogramSampleObserver(
    const std::string& name,
    StatisticsRecorder::ScopedHistogramSampleObserver* observer) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  auto iter = top_->observers_.find(name);
  DCHECK(iter != top_->observers_.end());

  auto result = iter->second->RemoveObserver(observer);
  if (result ==
      HistogramSampleObserverList::RemoveObserverResult::kWasOrBecameEmpty) {
    top_->observers_.erase(name);

    // We also clear the flag from the histogram (if it exists).
    const HistogramMap::const_iterator it = top_->histograms_.find(name);
    if (it != top_->histograms_.end())
      it->second->ClearFlags(HistogramBase::kCallbackExists);
  }

  have_active_callbacks_.store(
      global_sample_callback() || !top_->observers_.empty(),
      std::memory_order_relaxed);
}

// static
void StatisticsRecorder::FindAndRunHistogramCallbacks(
    base::PassKey<HistogramBase>,
    const char* histogram_name,
    uint64_t name_hash,
    HistogramBase::Sample sample) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  auto it = top_->observers_.find(histogram_name);

  // Ensure that this observer is still registered, as it might have been
  // unregistered before we acquired the lock.
  if (it == top_->observers_.end())
    return;

  it->second->Notify(FROM_HERE, &ScopedHistogramSampleObserver::RunCallback,
                     histogram_name, name_hash, sample);
}

// static
void StatisticsRecorder::SetGlobalSampleCallback(
    const GlobalSampleCallback& new_global_sample_callback) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  DCHECK(!global_sample_callback() || !new_global_sample_callback);
  global_sample_callback_.store(new_global_sample_callback);

  have_active_callbacks_.store(
      new_global_sample_callback || !top_->observers_.empty(),
      std::memory_order_relaxed);
}

// static
size_t StatisticsRecorder::GetHistogramCount() {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();
  return top_->histograms_.size();
}

// static
void StatisticsRecorder::ForgetHistogramForTesting(base::StringPiece name) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  const HistogramMap::iterator found = top_->histograms_.find(name);
  if (found == top_->histograms_.end())
    return;

  HistogramBase* const base = found->second;
  if (base->GetHistogramType() != SPARSE_HISTOGRAM) {
    // When forgetting a histogram, it's likely that other information is
    // also becoming invalid. Clear the persistent reference that may no
    // longer be valid. There's no danger in this as, at worst, duplicates
    // will be created in persistent memory.
    static_cast<Histogram*>(base)->bucket_ranges()->set_persistent_reference(0);
  }

  top_->histograms_.erase(found);
}

// static
std::unique_ptr<StatisticsRecorder>
StatisticsRecorder::CreateTemporaryForTesting() {
  const AutoLock auto_lock(lock_.Get());
  std::unique_ptr<StatisticsRecorder> temporary_recorder =
      WrapUnique(new StatisticsRecorder());
  temporary_recorder->ranges_manager_
      .DoNotReleaseRangesOnDestroyForTesting();  // IN-TEST
  return temporary_recorder;
}

// static
void StatisticsRecorder::SetRecordChecker(
    std::unique_ptr<RecordHistogramChecker> record_checker) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();
  top_->record_checker_ = std::move(record_checker);
}

// static
bool StatisticsRecorder::ShouldRecordHistogram(uint32_t histogram_hash) {
  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();
  return !top_->record_checker_ ||
         top_->record_checker_->ShouldRecord(histogram_hash);
}

// static
StatisticsRecorder::Histograms StatisticsRecorder::GetHistograms(
    bool include_persistent) {
  // This must be called *before* the lock is acquired below because it will
  // call back into this object to register histograms. Those called methods
  // will acquire the lock at that time.
  ImportGlobalPersistentHistograms();

  Histograms out;

  const AutoLock auto_lock(lock_.Get());
  EnsureGlobalRecorderWhileLocked();

  out.reserve(top_->histograms_.size());
  for (const auto& entry : top_->histograms_) {
    bool is_persistent = entry.second->HasFlags(HistogramBase::kIsPersistent);
    if (!include_persistent && is_persistent)
      continue;
    out.push_back(entry.second);
  }

  return out;
}

// static
StatisticsRecorder::Histograms StatisticsRecorder::Sort(Histograms histograms) {
  ranges::sort(histograms, &HistogramNameLesser);
  return histograms;
}

// static
StatisticsRecorder::Histograms StatisticsRecorder::WithName(
    Histograms histograms,
    const std::string& query,
    bool case_sensitive) {
  // Need a C-string query for comparisons against C-string histogram name.
  std::string lowercase_query;
  const char* query_string;
  if (case_sensitive) {
    query_string = query.c_str();
  } else {
    lowercase_query = base::ToLowerASCII(query);
    query_string = lowercase_query.c_str();
  }

  histograms.erase(
      ranges::remove_if(
          histograms,
          [query_string, case_sensitive](const HistogramBase* const h) {
            return !strstr(
                case_sensitive
                    ? h->histogram_name()
                    : base::ToLowerASCII(h->histogram_name()).c_str(),
                query_string);
          }),
      histograms.end());
  return histograms;
}

// static
void StatisticsRecorder::ImportGlobalPersistentHistograms() {
  // Import histograms from known persistent storage. Histograms could have been
  // added by other processes and they must be fetched and recognized locally.
  // If the persistent memory segment is not shared between processes, this call
  // does nothing.
  if (GlobalHistogramAllocator* allocator = GlobalHistogramAllocator::Get())
    allocator->ImportHistogramsToStatisticsRecorder();
}

StatisticsRecorder::StatisticsRecorder() {
  lock_.Get().AssertAcquired();
  previous_ = top_;
  top_ = this;
  InitLogOnShutdownWhileLocked();
}

// static
void StatisticsRecorder::InitLogOnShutdownWhileLocked() {
  lock_.Get().AssertAcquired();
  if (!is_vlog_initialized_ && VLOG_IS_ON(1)) {
    is_vlog_initialized_ = true;
    const auto dump_to_vlog = [](void*) {
      std::string output;
      WriteGraph("", &output);
      VLOG(1) << output;
    };
    AtExitManager::RegisterCallback(dump_to_vlog, nullptr);
  }
}

}  // namespace base
