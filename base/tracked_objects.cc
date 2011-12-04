// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracked_objects.h"

#include <math.h>

#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "base/port.h"

using base::TimeDelta;

namespace tracked_objects {

namespace {
// Flag to compile out almost all of the task tracking code.
static const bool kTrackAllTaskObjects = true;

// When ThreadData is first initialized, should we start in an ACTIVE state to
// record all of the startup-time tasks, or should we start up DEACTIVATED, so
// that we only record after parsing the command line flag --enable-tracking.
// Note that the flag may force either state, so this really controls only the
// period of time up until that flag is parsed. If there is no flag seen, then
// this state may prevail for much or all of the process lifetime.
static const ThreadData::Status kInitialStartupState = ThreadData::ACTIVE;
}  // anonymous namespace.

//------------------------------------------------------------------------------
// DeathData tallies durations when a death takes place.

DeathData::DeathData() {
  Clear();
}

DeathData::DeathData(int count) {
  Clear();
  count_ = count;
}

// TODO(jar): I need to see if this macro to optimize branching is worth it.
//
// This macro has no branching, so it is surely fast, and is equivalent to:
//             if (assign_it)
//               target = source;
// We use a macro rather than a template to force this to inline.
// Related code for calculating max is discussed on the web.
#define CONDITIONAL_ASSIGN(assign_it, target, source) \
    ((target) ^= ((target) ^ (source)) & -static_cast<DurationInt>(assign_it))

void DeathData::RecordDeath(const DurationInt queue_duration,
                            const DurationInt run_duration,
                            int32 random_number) {
  queue_duration_sum_ += queue_duration;
  run_duration_sum_ += run_duration;
  ++count_;

  // Take a uniformly distributed sample over all durations ever supplied.
  // The probability that we (instead) use this new sample is 1/count_.  This
  // results in a completely uniform selection of the sample.
  // We ignore the fact that we correlated our selection of a sample of run
  // and queue times.
  bool take_sample = 0 == (random_number % count_);
  CONDITIONAL_ASSIGN(take_sample, queue_duration_sample_, queue_duration);
  CONDITIONAL_ASSIGN(take_sample, run_duration_sample_, run_duration);

  CONDITIONAL_ASSIGN(queue_duration_max_ < queue_duration, queue_duration_max_,
                     queue_duration);
  CONDITIONAL_ASSIGN(run_duration_max_ < run_duration, run_duration_max_,
                     run_duration);
  // Ensure we got the macros right.
  DCHECK_GE(queue_duration_max_, queue_duration);
  DCHECK_GE(run_duration_max_, run_duration);
  DCHECK(!take_sample || run_duration_sample_ == run_duration);
  DCHECK(!take_sample || queue_duration_sample_ == queue_duration);
}

int DeathData::count() const { return count_; }

DurationInt DeathData::run_duration_sum() const { return run_duration_sum_; }

DurationInt DeathData::run_duration_max() const { return run_duration_max_; }

DurationInt DeathData::run_duration_sample() const {
  return run_duration_sample_;
}

DurationInt DeathData::queue_duration_sum() const {
  return queue_duration_sum_;
}

DurationInt DeathData::queue_duration_max() const {
  return queue_duration_max_;
}

DurationInt DeathData::queue_duration_sample() const {
  return queue_duration_sample_;
}


base::DictionaryValue* DeathData::ToValue() const {
  base::DictionaryValue* dictionary = new base::DictionaryValue;
  dictionary->Set("count", base::Value::CreateIntegerValue(count_));
  dictionary->Set("run_ms",
      base::Value::CreateIntegerValue(run_duration_sum()));
  dictionary->Set("run_ms_max",
      base::Value::CreateIntegerValue(run_duration_max()));
  dictionary->Set("run_ms_sample",
      base::Value::CreateIntegerValue(run_duration_sample()));
  dictionary->Set("queue_ms",
      base::Value::CreateIntegerValue(queue_duration_sum()));
  dictionary->Set("queue_ms_max",
      base::Value::CreateIntegerValue(queue_duration_max()));
  dictionary->Set("queue_ms_sample",
      base::Value::CreateIntegerValue(queue_duration_sample()));
  return dictionary;
}

void DeathData::ResetMax() {
  run_duration_max_ = 0;
  queue_duration_max_ = 0;
}

void DeathData::Clear() {
  count_ = 0;
  run_duration_sum_ = 0;
  run_duration_max_ = 0;
  run_duration_sample_ = 0;
  queue_duration_sum_ = 0;
  queue_duration_max_ = 0;
  queue_duration_sample_ = 0;
}

//------------------------------------------------------------------------------
BirthOnThread::BirthOnThread(const Location& location,
                             const ThreadData& current)
    : location_(location),
      birth_thread_(&current) {
}

const Location BirthOnThread::location() const { return location_; }
const ThreadData* BirthOnThread::birth_thread() const { return birth_thread_; }

//------------------------------------------------------------------------------
Births::Births(const Location& location, const ThreadData& current)
    : BirthOnThread(location, current),
      birth_count_(1) { }

int Births::birth_count() const { return birth_count_; }

void Births::RecordBirth() { ++birth_count_; }

void Births::ForgetBirth() { --birth_count_; }

void Births::Clear() { birth_count_ = 0; }

//------------------------------------------------------------------------------
// ThreadData maintains the central data for all births and deaths on a single
// thread.

// TODO(jar): We should pull all these static vars together, into a struct, and
// optimize layout so that we benefit from locality of reference during accesses
// to them.

// A TLS slot which points to the ThreadData instance for the current thread. We
// do a fake initialization here (zeroing out data), and then the real in-place
// construction happens when we call tls_index_.Initialize().
// static
base::ThreadLocalStorage::Slot ThreadData::tls_index_(base::LINKER_INITIALIZED);

// static
int ThreadData::worker_thread_data_creation_count_ = 0;

// static
int ThreadData::cleanup_count_ = 0;

// static
int ThreadData::incarnation_counter_ = 0;

// static
ThreadData* ThreadData::all_thread_data_list_head_ = NULL;

// static
ThreadData* ThreadData::first_retired_worker_ = NULL;

// static
base::LazyInstance<base::Lock,
                   base::LeakyLazyInstanceTraits<base::Lock> >
    ThreadData::list_lock_ = LAZY_INSTANCE_INITIALIZER;

// static
ThreadData::Status ThreadData::status_ = ThreadData::UNINITIALIZED;

ThreadData::ThreadData(const std::string& suggested_name)
    : incarnation_count_for_pool_(-1),
      next_(NULL),
      next_retired_worker_(NULL),
      worker_thread_number_(0) {
  DCHECK_GE(suggested_name.size(), 0u);
  thread_name_ = suggested_name;
  PushToHeadOfList();  // Which sets real incarnation_count_for_pool_.
}

ThreadData::ThreadData(int thread_number)
    : incarnation_count_for_pool_(-1),
      next_(NULL),
      next_retired_worker_(NULL),
      worker_thread_number_(thread_number) {
  CHECK_GT(thread_number, 0);
  base::StringAppendF(&thread_name_, "WorkerThread-%d", thread_number);
  PushToHeadOfList();  // Which sets real incarnation_count_for_pool_.
}

ThreadData::~ThreadData() {}

void ThreadData::PushToHeadOfList() {
  // Toss in a hint of randomness (atop the uniniitalized value).
  random_number_ += static_cast<int32>(this - static_cast<ThreadData*>(0));
  random_number_ ^= (Now() - TrackedTime()).InMilliseconds();

  DCHECK(!next_);
  base::AutoLock lock(*list_lock_.Pointer());
  incarnation_count_for_pool_ = incarnation_counter_;
  next_ = all_thread_data_list_head_;
  all_thread_data_list_head_ = this;
}

// static
ThreadData* ThreadData::first() {
  base::AutoLock lock(*list_lock_.Pointer());
  return all_thread_data_list_head_;
}

ThreadData* ThreadData::next() const { return next_; }

// static
void ThreadData::InitializeThreadContext(const std::string& suggested_name) {
  if (!Initialize())  // Always initialize if needed.
    return;
  ThreadData* current_thread_data =
      reinterpret_cast<ThreadData*>(tls_index_.Get());
  if (current_thread_data)
    return;  // Browser tests instigate this.
  current_thread_data = new ThreadData(suggested_name);
  tls_index_.Set(current_thread_data);
}

// static
ThreadData* ThreadData::Get() {
  if (!tls_index_.initialized())
    return NULL;  // For unittests only.
  ThreadData* registered = reinterpret_cast<ThreadData*>(tls_index_.Get());
  if (registered)
    return registered;

  // We must be a worker thread, since we didn't pre-register.
  ThreadData* worker_thread_data = NULL;
  int worker_thread_number = 0;
  {
    base::AutoLock lock(*list_lock_.Pointer());
    if (first_retired_worker_) {
      worker_thread_data = first_retired_worker_;
      first_retired_worker_ = first_retired_worker_->next_retired_worker_;
      worker_thread_data->next_retired_worker_ = NULL;
    } else {
      worker_thread_number = ++worker_thread_data_creation_count_;
    }
  }

  // If we can't find a previously used instance, then we have to create one.
  if (!worker_thread_data) {
    DCHECK_GT(worker_thread_number, 0);
    worker_thread_data = new ThreadData(worker_thread_number);
  }
  DCHECK_GT(worker_thread_data->worker_thread_number_, 0);

  tls_index_.Set(worker_thread_data);
  return worker_thread_data;
}

// static
void ThreadData::OnThreadTermination(void* thread_data) {
  // We must NOT do any allocations during this callback. There is a chance
  // that the allocator is no longer active on this thread.
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.
  if (!thread_data)
    return;
  reinterpret_cast<ThreadData*>(thread_data)->OnThreadTerminationCleanup();
}

void ThreadData::OnThreadTerminationCleanup() {
  // The list_lock_ was created when we registered the callback, so it won't be
  // allocated here despite the lazy reference.
  base::AutoLock lock(*list_lock_.Pointer());
  if (incarnation_counter_ != incarnation_count_for_pool_)
    return;  // ThreadData was constructed in an earlier unit test.
  ++cleanup_count_;
  // Only worker threads need to be retired and reused.
  if (!worker_thread_number_) {
    return;
  }
  // We must NOT do any allocations during this callback.
  // Using the simple linked lists avoids all allocations.
  DCHECK_EQ(this->next_retired_worker_, reinterpret_cast<ThreadData*>(NULL));
  this->next_retired_worker_ = first_retired_worker_;
  first_retired_worker_ = this;
}

// static
base::DictionaryValue* ThreadData::ToValue(bool reset_max) {
  DataCollector collected_data;  // Gather data.
  // Request multiple calls to collected_data.Append() for all threads.
  SendAllMaps(reset_max, &collected_data);
  collected_data.AddListOfLivingObjects();  // Add births that are still alive.
  base::ListValue* list = collected_data.ToValue();
  base::DictionaryValue* dictionary = new base::DictionaryValue();
  dictionary->Set("list", list);
  return dictionary;
}

Births* ThreadData::TallyABirth(const Location& location) {
  BirthMap::iterator it = birth_map_.find(location);
  if (it != birth_map_.end()) {
    it->second->RecordBirth();
    return it->second;
  }

  Births* tracker = new Births(location, *this);
  // Lock since the map may get relocated now, and other threads sometimes
  // snapshot it (but they lock before copying it).
  base::AutoLock lock(map_lock_);
  birth_map_[location] = tracker;
  return tracker;
}

void ThreadData::TallyADeath(const Births& birth,
                             DurationInt queue_duration,
                             DurationInt run_duration) {
  // Stir in some randomness, plus add constant in case durations are zero.
  const DurationInt kSomePrimeNumber = 5939;  // To big is 4294967279;
  random_number_ += queue_duration + run_duration + kSomePrimeNumber;
  // An address is going to have some randomness to it as well ;-).
  random_number_ ^= static_cast<int32>(&birth - reinterpret_cast<Births*>(0));

  DeathMap::iterator it = death_map_.find(&birth);
  DeathData* death_data;
  if (it != death_map_.end()) {
    death_data = &it->second;
  } else {
    base::AutoLock lock(map_lock_);  // Lock as the map may get relocated now.
    death_data = &death_map_[&birth];
  }  // Release lock ASAP.
  death_data->RecordDeath(queue_duration, run_duration, random_number_);
}

// static
Births* ThreadData::TallyABirthIfActive(const Location& location) {
  if (!kTrackAllTaskObjects)
    return NULL;  // Not compiled in.

  if (!tracking_status())
    return NULL;
  ThreadData* current_thread_data = Get();
  if (!current_thread_data)
    return NULL;
  return current_thread_data->TallyABirth(location);
}

// static
void ThreadData::TallyRunOnNamedThreadIfTracking(
    const base::TrackingInfo& completed_task,
    const TrackedTime& start_of_run,
    const TrackedTime& end_of_run) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.

  // Even if we have been DEACTIVATED, we will process any pending births so
  // that our data structures (which counted the outstanding births) remain
  // consistent.
  const Births* birth = completed_task.birth_tally;
  if (!birth)
    return;
  ThreadData* current_thread_data = Get();
  if (!current_thread_data)
    return;

  // To avoid conflating our stats with the delay duration in a PostDelayedTask,
  // we identify such tasks, and replace their post_time with the time they
  // were scheduled (requested?) to emerge from the delayed task queue. This
  // means that queueing delay for such tasks will show how long they went
  // unserviced, after they *could* be serviced.  This is the same stat as we
  // have for non-delayed tasks, and we consistently call it queueing delay.
  TrackedTime effective_post_time = completed_task.delayed_run_time.is_null()
      ? tracked_objects::TrackedTime(completed_task.time_posted)
      : tracked_objects::TrackedTime(completed_task.delayed_run_time);

  // Watch out for a race where status_ is changing, and hence one or both
  // of start_of_run or end_of_run is zero.  IN that case, we didn't bother to
  // get a time value since we "weren't tracking" and we were trying to be
  // efficient by not calling for a genuine time value. For simplicity, we'll
  // use a default zero duration when we can't calculate a true value.
  DurationInt queue_duration = 0;
  DurationInt run_duration = 0;
  if (!start_of_run.is_null()) {
    queue_duration = (start_of_run - effective_post_time).InMilliseconds();
    if (!end_of_run.is_null())
      run_duration = (end_of_run - start_of_run).InMilliseconds();
  }
  current_thread_data->TallyADeath(*birth, queue_duration, run_duration);
}

// static
void ThreadData::TallyRunOnWorkerThreadIfTracking(
    const Births* birth,
    const TrackedTime& time_posted,
    const TrackedTime& start_of_run,
    const TrackedTime& end_of_run) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.

  // Even if we have been DEACTIVATED, we will process any pending births so
  // that our data structures (which counted the outstanding births) remain
  // consistent.
  if (!birth)
    return;

  // TODO(jar): Support the option to coalesce all worker-thread activity under
  // one ThreadData instance that uses locks to protect *all* access.  This will
  // reduce memory (making it provably bounded), but run incrementally slower
  // (since we'll use locks on TallyBirth and TallyDeath).  The good news is
  // that the locks on TallyDeath will be *after* the worker thread has run, and
  // hence nothing will be waiting for the completion (... besides some other
  // thread that might like to run).  Also, the worker threads tasks are
  // generally longer, and hence the cost of the lock may perchance be amortized
  // over the long task's lifetime.
  ThreadData* current_thread_data = Get();
  if (!current_thread_data)
    return;

  DurationInt queue_duration = 0;
  DurationInt run_duration = 0;
  if (!start_of_run.is_null()) {
    queue_duration = (start_of_run - time_posted).InMilliseconds();
    if (!end_of_run.is_null())
      run_duration = (end_of_run - start_of_run).InMilliseconds();
  }
  current_thread_data->TallyADeath(*birth, queue_duration, run_duration);
}

// static
void ThreadData::TallyRunInAScopedRegionIfTracking(
    const Births* birth,
    const TrackedTime& start_of_run,
    const TrackedTime& end_of_run) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.

  // Even if we have been DEACTIVATED, we will process any pending births so
  // that our data structures (which counted the outstanding births) remain
  // consistent.
  if (!birth)
    return;

  ThreadData* current_thread_data = Get();
  if (!current_thread_data)
    return;

  DurationInt queue_duration = 0;
  DurationInt run_duration = 0;
  if (!start_of_run.is_null() && !end_of_run.is_null())
    run_duration = (end_of_run - start_of_run).InMilliseconds();
  current_thread_data->TallyADeath(*birth, queue_duration, run_duration);
}

const std::string ThreadData::thread_name() const { return thread_name_; }

// This may be called from another thread.
void ThreadData::SnapshotMaps(bool reset_max,
                              BirthMap* birth_map,
                              DeathMap* death_map) {
  base::AutoLock lock(map_lock_);
  for (BirthMap::const_iterator it = birth_map_.begin();
       it != birth_map_.end(); ++it)
    (*birth_map)[it->first] = it->second;
  for (DeathMap::iterator it = death_map_.begin();
       it != death_map_.end(); ++it) {
    (*death_map)[it->first] = it->second;
    if (reset_max)
      it->second.ResetMax();
  }
}

// static
void ThreadData::SendAllMaps(bool reset_max, class DataCollector* target) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.
  // Get an unchanging copy of a ThreadData list.
  ThreadData* my_list = ThreadData::first();

  // Gather data serially.
  // This hackish approach *can* get some slighly corrupt tallies, as we are
  // grabbing values without the protection of a lock, but it has the advantage
  // of working even with threads that don't have message loops.  If a user
  // sees any strangeness, they can always just run their stats gathering a
  // second time.
  for (ThreadData* thread_data = my_list;
       thread_data;
       thread_data = thread_data->next()) {
    // Get copy of data.
    ThreadData::BirthMap birth_map;
    ThreadData::DeathMap death_map;
    thread_data->SnapshotMaps(reset_max, &birth_map, &death_map);
    target->Append(*thread_data, birth_map, death_map);
  }
}

// static
void ThreadData::ResetAllThreadData() {
  ThreadData* my_list = first();

  for (ThreadData* thread_data = my_list;
       thread_data;
       thread_data = thread_data->next())
    thread_data->Reset();
}

void ThreadData::Reset() {
  base::AutoLock lock(map_lock_);
  for (DeathMap::iterator it = death_map_.begin();
       it != death_map_.end(); ++it)
    it->second.Clear();
  for (BirthMap::iterator it = birth_map_.begin();
       it != birth_map_.end(); ++it)
    it->second->Clear();
}

bool ThreadData::Initialize() {
  if (!kTrackAllTaskObjects)
    return false;  // Not compiled in.
  if (status_ != UNINITIALIZED)
    return true;  // Someone else did the initialization.
  // Due to racy lazy initialization in tests, we'll need to recheck status_
  // after we acquire the lock.

  // Ensure that we don't double initialize tls.  We are called when single
  // threaded in the product, but some tests may be racy and lazy about our
  // initialization.
  base::AutoLock lock(*list_lock_.Pointer());
  if (status_ != UNINITIALIZED)
    return true;  // Someone raced in here and beat us.

  // Perform the "real" TLS initialization now, and leave it intact through
  // process termination.
  if (!tls_index_.initialized()) {  // Testing may have initialized this.
    tls_index_.Initialize(&ThreadData::OnThreadTermination);
    if (!tls_index_.initialized())
      return false;
  }

  // Incarnation counter is only significant to testing, as it otherwise will
  // never again change in this process.
  ++incarnation_counter_;

  // The lock is not critical for setting status_, but it doesn't hurt. It also
  // ensures that if we have a racy initialization, that we'll bail as soon as
  // we get the lock earlier in this method.
  status_ = kInitialStartupState;
  DCHECK(status_ != UNINITIALIZED);
  return true;
}

// static
bool ThreadData::InitializeAndSetTrackingStatus(bool status) {
  if (!Initialize())  // No-op if already initialized.
    return false;  // Not compiled in.

  status_ = status ? ACTIVE : DEACTIVATED;
  return true;
}

// static
bool ThreadData::tracking_status() {
  return status_ == ACTIVE;
}

// static
TrackedTime ThreadData::NowForStartOfRun() {
  return Now();
}

// static
TrackedTime ThreadData::NowForEndOfRun() {
  return Now();
}

// static
TrackedTime ThreadData::Now() {
  if (kTrackAllTaskObjects && tracking_status())
    return TrackedTime::Now();
  return TrackedTime();  // Super fast when disabled, or not compiled.
}

// static
void ThreadData::EnsureCleanupWasCalled(int major_threads_shutdown_count) {
  base::AutoLock lock(*list_lock_.Pointer());
  if (worker_thread_data_creation_count_ == 0)
    return;  // We haven't really run much, and couldn't have leaked.
  // Verify that we've at least shutdown/cleanup the major namesd threads.  The
  // caller should tell us how many thread shutdowns should have taken place by
  // now.
  return;  // TODO(jar): until this is working on XP, don't run the real test.
  CHECK_GT(cleanup_count_, major_threads_shutdown_count);
}

// static
void ThreadData::ShutdownSingleThreadedCleanup(bool leak) {
  // This is only called from test code, where we need to cleanup so that
  // additional tests can be run.
  // We must be single threaded... but be careful anyway.
  if (!InitializeAndSetTrackingStatus(false))
    return;
  ThreadData* thread_data_list;
  {
    base::AutoLock lock(*list_lock_.Pointer());
    thread_data_list = all_thread_data_list_head_;
    all_thread_data_list_head_ = NULL;
    ++incarnation_counter_;
    // To be clean, break apart the retired worker list (though we leak them).
    while (first_retired_worker_) {
      ThreadData* worker = first_retired_worker_;
      CHECK_GT(worker->worker_thread_number_, 0);
      first_retired_worker_ = worker->next_retired_worker_;
      worker->next_retired_worker_ = NULL;
    }
  }

  // Put most global static back in pristine shape.
  worker_thread_data_creation_count_ = 0;
  cleanup_count_ = 0;
  tls_index_.Set(NULL);
  status_ = UNINITIALIZED;

  // To avoid any chance of racing in unit tests, which is the only place we
  // call this function, we may sometimes leak all the data structures we
  // recovered, as they may still be in use on threads from prior tests!
  if (leak)
    return;

  // When we want to cleanup (on a single thread), here is what we do.

  // Do actual recursive delete in all ThreadData instances.
  while (thread_data_list) {
    ThreadData* next_thread_data = thread_data_list;
    thread_data_list = thread_data_list->next();

    for (BirthMap::iterator it = next_thread_data->birth_map_.begin();
         next_thread_data->birth_map_.end() != it; ++it)
      delete it->second;  // Delete the Birth Records.
    next_thread_data->birth_map_.clear();
    next_thread_data->death_map_.clear();
    delete next_thread_data;  // Includes all Death Records.
  }
}

//------------------------------------------------------------------------------
// Individual 3-tuple of birth (place and thread) along with death thread, and
// the accumulated stats for instances (DeathData).

Snapshot::Snapshot(const BirthOnThread& birth_on_thread,
                   const ThreadData& death_thread,
                   const DeathData& death_data)
    : birth_(&birth_on_thread),
      death_thread_(&death_thread),
      death_data_(death_data) {
}

Snapshot::Snapshot(const BirthOnThread& birth_on_thread, int count)
    : birth_(&birth_on_thread),
      death_thread_(NULL),
      death_data_(DeathData(count)) {
}

const std::string Snapshot::DeathThreadName() const {
  if (death_thread_)
    return death_thread_->thread_name();
  return "Still_Alive";
}

base::DictionaryValue* Snapshot::ToValue() const {
  base::DictionaryValue* dictionary = new base::DictionaryValue;
  dictionary->Set("death_data", death_data_.ToValue());
  dictionary->Set("birth_thread",
      base::Value::CreateStringValue(birth_->birth_thread()->thread_name()));
  dictionary->Set("death_thread",
      base::Value::CreateStringValue(DeathThreadName()));
  dictionary->Set("location", birth_->location().ToValue());
  return dictionary;
}

//------------------------------------------------------------------------------
// DataCollector

DataCollector::DataCollector() {}

DataCollector::~DataCollector() {
}

void DataCollector::Append(const ThreadData &thread_data,
                           const ThreadData::BirthMap &birth_map,
                           const ThreadData::DeathMap &death_map) {
  for (ThreadData::DeathMap::const_iterator it = death_map.begin();
       it != death_map.end(); ++it) {
    collection_.push_back(Snapshot(*it->first, thread_data, it->second));
    global_birth_count_[it->first] -= it->first->birth_count();
  }

  for (ThreadData::BirthMap::const_iterator it = birth_map.begin();
       it != birth_map.end(); ++it) {
    global_birth_count_[it->second] += it->second->birth_count();
  }
}

DataCollector::Collection* DataCollector::collection() {
  return &collection_;
}

void DataCollector::AddListOfLivingObjects() {
  for (BirthCount::iterator it = global_birth_count_.begin();
       it != global_birth_count_.end(); ++it) {
    if (it->second > 0)
      collection_.push_back(Snapshot(*it->first, it->second));
  }
}

base::ListValue* DataCollector::ToValue() const {
  base::ListValue* list = new base::ListValue;
  for (size_t i = 0; i < collection_.size(); ++i) {
    list->Append(collection_[i].ToValue());
  }
  return list;
}

}  // namespace tracked_objects
