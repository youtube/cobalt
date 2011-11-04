// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracked_objects.h"

#include <math.h>

#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

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

void DeathData::RecordDeath(const Duration& queue_duration,
                            const Duration& run_duration) {
  ++count_;
  queue_time_.AddDuration(queue_duration);
  run_time_.AddDuration(run_duration);
}

int DeathData::AverageMsRunDuration() const {
  return run_time_.AverageMsDuration(count_);
}

int DeathData::AverageMsQueueDuration() const {
  return queue_time_.AverageMsDuration(count_);
}

void DeathData::AddDeathData(const DeathData& other) {
  count_ += other.count_;
  queue_time_.AddData(other.queue_time_);
  run_time_.AddData(other.run_time_);
}

void DeathData::WriteHTML(std::string* output) const {
  if (!count_)
    return;
  base::StringAppendF(output, "%s:%d, ",
                      (count_ == 1) ? "Life" : "Lives", count_);
  output->append("Run:");
  run_time_.WriteHTML(count_, output);

  output->append("Queue:");
  queue_time_.WriteHTML(count_, output);
}

base::DictionaryValue* DeathData::ToValue() const {
  base::DictionaryValue* dictionary = new base::DictionaryValue;
  dictionary->Set("count", base::Value::CreateIntegerValue(count_));
  dictionary->Set("run_ms",
      base::Value::CreateIntegerValue(run_time_.duration().InMilliseconds()));
  dictionary->Set("queue_ms",
      base::Value::CreateIntegerValue(queue_time_.duration().InMilliseconds()));
  dictionary->Set("run_ms_max",
      base::Value::CreateIntegerValue(run_time_.max().InMilliseconds()));
  dictionary->Set("queue_ms_max",
      base::Value::CreateIntegerValue(queue_time_.max().InMilliseconds()));
  return dictionary;
}

void DeathData::Clear() {
  count_ = 0;
  run_time_.Clear();
  queue_time_.Clear();
}

//------------------------------------------------------------------------------

void DeathData::Data::WriteHTML(int count, std::string* output) const {
  // Be careful to leave static_casts intact, as the type returned by
  // InMilliseconds() may not always be an int, even if it can generally fit
  // into an int.
  base::StringAppendF(output, "%dms",
                      static_cast<int>(duration_.InMilliseconds()));
  if (count == 1) {
    output->append(" ");
    return;
  }
  base::StringAppendF(output, "(%dms/life,max:%dms) ",
                      AverageMsDuration(count),
                      static_cast<int>(max_.InMilliseconds()));
}

void DeathData::Data::AddData(const Data& other) {
  duration_ += other.duration_;
  if (max_ > other.max_)
    return;
  max_ = other.max_;
}

void DeathData::Data::AddDuration(const Duration& duration) {
  duration_ += duration;
  if (max_ > duration)
    return;
  max_ = duration;
}

int DeathData::Data::AverageMsDuration(int count) const {
  if (duration_ == Duration() || !count)
    return 0;
  return static_cast<int>(duration_.InMilliseconds() + count / 2) / count;
}

void DeathData::Data::Clear() {
  duration_ = Duration();
  max_ = Duration();
}
//------------------------------------------------------------------------------
BirthOnThread::BirthOnThread(const Location& location,
                             const ThreadData& current)
    : location_(location),
      birth_thread_(&current) {}

//------------------------------------------------------------------------------
Births::Births(const Location& location, const ThreadData& current)
    : BirthOnThread(location, current),
      birth_count_(1) { }

//------------------------------------------------------------------------------
// ThreadData maintains the central data for all births and deaths.

// TODO(jar): We should pull all these static vars together, into a struct, and
// optimize layout so that we benefit from locality of reference during accesses
// to them.

// A TLS slot which points to the ThreadData instance for the current thread. We
// do a fake initialization here (zeroing out data), and then the real in-place
// construction happens when we call tls_index_.Initialize().
// static
base::ThreadLocalStorage::Slot ThreadData::tls_index_(base::LINKER_INITIALIZED);

// A lock-protected counter to assign sequence number to threads.
// static
int ThreadData::thread_number_counter_ = 0;

// static
int ThreadData::incarnation_counter_ = 0;

// static
ThreadData* ThreadData::all_thread_data_list_head_ = NULL;

// static
ThreadData::ThreadDataPool* ThreadData::unregistered_thread_data_pool_ = NULL;

// static
base::Lock* ThreadData::list_lock_;

// static
ThreadData::Status ThreadData::status_ = ThreadData::UNINITIALIZED;

ThreadData::ThreadData(const std::string& suggested_name)
    : incarnation_count_for_pool_(-1),
      next_(NULL),
      is_a_worker_thread_(false) {
  DCHECK_GE(suggested_name.size(), 0u);
  thread_name_ = suggested_name;
  PushToHeadOfList();  // Which sets real incarnation_count_for_pool_.
}

ThreadData::ThreadData()
    : incarnation_count_for_pool_(-1),
      next_(NULL),
      is_a_worker_thread_(true) {
  int thread_number;
  {
    base::AutoLock lock(*list_lock_);
    thread_number = ++thread_number_counter_;
  }
  base::StringAppendF(&thread_name_, "WorkerThread-%d", thread_number);
  PushToHeadOfList();  // Which sets real incarnation_count_for_pool_.
}

ThreadData::~ThreadData() {}

void ThreadData::PushToHeadOfList() {
  DCHECK(!next_);
  base::AutoLock lock(*list_lock_);
  incarnation_count_for_pool_ = incarnation_counter_;
  next_ = all_thread_data_list_head_;
  all_thread_data_list_head_ = this;
}

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
  {
    base::AutoLock lock(*list_lock_);
    if (!unregistered_thread_data_pool_->empty()) {
      worker_thread_data =
        const_cast<ThreadData*>(unregistered_thread_data_pool_->top());
      unregistered_thread_data_pool_->pop();
    }
  }

  // If we can't find a previously used instance, then we have to create one.
  if (!worker_thread_data)
    worker_thread_data = new ThreadData();

  tls_index_.Set(worker_thread_data);
  return worker_thread_data;
}

// static
void ThreadData::OnThreadTermination(void* thread_data) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.
  if (!thread_data)
    return;
  reinterpret_cast<ThreadData*>(thread_data)->OnThreadTerminationCleanup();
}

void ThreadData::OnThreadTerminationCleanup() const {
  if (!is_a_worker_thread_)
    return;
  base::AutoLock lock(*list_lock_);
  if (incarnation_counter_ != incarnation_count_for_pool_)
    return;  // ThreadData was constructed in an earlier unit test.

  // Handle case where we are in unit tests, and have become UNINITIALIZED.
  // In that case, the pool might be NULL.  We really should detect this via the
  // incarnation_counter_, but this call is rarely made, so we can afford to
  // code defensively.
  if (unregistered_thread_data_pool_)
    unregistered_thread_data_pool_->push(this);
}

// static
void ThreadData::WriteHTML(const std::string& query, std::string* output) {
  if (status_ == UNINITIALIZED)
    return;  // Not yet initialized.

  DataCollector collected_data;  // Gather data.
  collected_data.AddListOfLivingObjects();  // Add births that are still alive.

  // Data Gathering is complete. Now to sort/process/render.
  DataCollector::Collection* collection = collected_data.collection();

  // Create filtering and sort comparison object.
  Comparator comparator;
  comparator.ParseQuery(query);

  // Filter out acceptable (matching) instances.
  DataCollector::Collection match_array;
  for (DataCollector::Collection::iterator it = collection->begin();
       it != collection->end(); ++it) {
    if (comparator.Acceptable(*it))
      match_array.push_back(*it);
  }

  comparator.Sort(&match_array);

  WriteHTMLTotalAndSubtotals(match_array, comparator, output);

  comparator.Clear();  // Delete tiebreaker_ instances.

  output->append("</pre>");

  const char* help_string = "The following are the keywords that can be used to"
    " sort and aggregate the data, or to select data.<br><ul>"
    "<li><b>Count</b> Number of instances seen."
    "<li><b>Duration</b> Average duration in ms of Run() time."
    "<li><b>TotalDuration</b> Summed durations in ms of Run() times."
    "<li><b>MaxDuration</b> Largest duration in ms of Run() times."
    "<li><b>AverageQueueDuration</b> Average duration in ms of queueing time."
    "<li><b>TotalQueueDuration</b> Summed queuing durations in ms."
    "<li><b>MaxQueueDuration</b> Largest duration in ms of queueing times."
    "<li><b>Birth</b> Thread on which the task was constructed."
    "<li><b>Death</b> Thread on which the task was run and deleted."
    "<li><b>File</b> File in which the task was contructed."
    "<li><b>Function</b> Function in which the task was constructed."
    "<li><b>Line</b> Line number of the file in which the task was constructed."
    "</ul><br>"
    "As examples:<ul>"
    "<li><b>about:tracking/file</b> would sort the above data by file, and"
    " aggregate data on a per-file basis."
    "<li><b>about:tracking/file=Dns</b> would only list data for tasks"
    " constructed in a file containing the text |Dns|."
    "<li><b>about:tracking/death/duration</b> would sort the data by death"
    " thread(i.e., where tasks ran) and then by the average runtime for the"
    " tasks. Form an aggregation group, one per thread, showing the results on"
    " each thread."
    "<li><b>about:tracking/birth/death</b> would sort the above list by birth"
    " thread, and then by death thread, and would aggregate data for each pair"
    " of lifetime events."
    "</ul>"
    " The data can be reset to zero (discarding all births, deaths, etc.) using"
    " <b>about:tracking/reset</b>. The existing stats will be displayed, but"
    " the internal stats will be set to zero, and start accumulating afresh."
    " This option is very helpful if you only wish to consider tasks created"
    " after some point in time.<br><br>"
    "If you wish to monitor Renderer events, be sure to run in --single-process"
    " mode.";
  output->append(help_string);
}

// static
void ThreadData::WriteHTMLTotalAndSubtotals(
    const DataCollector::Collection& match_array,
    const Comparator& comparator,
    std::string* output) {
  if (match_array.empty()) {
    output->append("There were no tracked matches.");
    return;
  }
  // Aggregate during printing
  Aggregation totals;
  for (size_t i = 0; i < match_array.size(); ++i) {
    totals.AddDeathSnapshot(match_array[i]);
  }
  output->append("Aggregate Stats: ");
  totals.WriteHTML(output);
  output->append("<hr><hr>");

  Aggregation subtotals;
  for (size_t i = 0; i < match_array.size(); ++i) {
    if (0 == i || !comparator.Equivalent(match_array[i - 1],
                                         match_array[i])) {
      // Print group's defining characteristics.
      comparator.WriteSortGrouping(match_array[i], output);
      output->append("<br><br>");
    }
    comparator.WriteSnapshotHTML(match_array[i], output);
    output->append("<br>");
    subtotals.AddDeathSnapshot(match_array[i]);
    if (i + 1 >= match_array.size() ||
        !comparator.Equivalent(match_array[i],
                               match_array[i + 1])) {
      // Print aggregate stats for the group.
      output->append("<br>");
      subtotals.WriteHTML(output);
      output->append("<br><hr><br>");
      subtotals.Clear();
    }
  }
}

// static
base::DictionaryValue* ThreadData::ToValue() {
  DataCollector collected_data;  // Gather data.
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
  base::AutoLock lock(lock_);
  birth_map_[location] = tracker;
  return tracker;
}

void ThreadData::TallyADeath(const Births& birth,
                             const Duration& queue_duration,
                             const Duration& run_duration) {
  DeathMap::iterator it = death_map_.find(&birth);
  DeathData* death_data;
  if (it != death_map_.end()) {
    death_data = &it->second;
  } else {
    base::AutoLock lock(lock_);  // Lock since the map may get relocated now.
    death_data = &death_map_[&birth];
  }  // Release lock ASAP.
  death_data->RecordDeath(queue_duration, run_duration);
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
  Duration queue_duration;
  Duration run_duration;
  if (!start_of_run.is_null()) {
    queue_duration = start_of_run - effective_post_time;
    if (!end_of_run.is_null())
      run_duration = end_of_run - start_of_run;
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

  Duration queue_duration;
  Duration run_duration;
  if (!start_of_run.is_null()) {
    queue_duration = start_of_run - time_posted;
    if (!end_of_run.is_null())
      run_duration = end_of_run - start_of_run;
  }
  current_thread_data->TallyADeath(*birth, queue_duration, run_duration);
}

// static
ThreadData* ThreadData::first() {
  base::AutoLock lock(*list_lock_);
  return all_thread_data_list_head_;
}

// This may be called from another thread.
void ThreadData::SnapshotBirthMap(BirthMap *output) const {
  base::AutoLock lock(lock_);
  for (BirthMap::const_iterator it = birth_map_.begin();
       it != birth_map_.end(); ++it)
    (*output)[it->first] = it->second;
}

// This may be called from another thread.
void ThreadData::SnapshotDeathMap(DeathMap *output) const {
  base::AutoLock lock(lock_);
  for (DeathMap::const_iterator it = death_map_.begin();
       it != death_map_.end(); ++it)
    (*output)[it->first] = it->second;
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
  base::AutoLock lock(lock_);
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
    return true;
  // Initialize all leaking constants that are difficult to toggle in and out
  // of existance.
  // First call must be made when single threaded at startup.
  // Perform the "real" TLS initialization now, and leave it intact through
  // process termination.
  if (!tls_index_.initialized())  // Testing may have initialized this.
    tls_index_.Initialize(&ThreadData::OnThreadTermination);
  DCHECK(tls_index_.initialized());
  ThreadDataPool* pool = new ThreadDataPool;
  // TODO(jar): A linker initialized spin lock would be much safer than this
  // allocation, which relies on being called while single threaded.
  if (!list_lock_)  // In case testing deleted this.
    list_lock_ = new base::Lock;
  status_ = kInitialStartupState;

  base::AutoLock lock(*list_lock_);
  DCHECK_EQ(unregistered_thread_data_pool_,
            reinterpret_cast<ThreadDataPool*>(NULL));
  unregistered_thread_data_pool_ = pool;
  ++incarnation_counter_;
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
TrackedTime ThreadData::Now() {
  if (kTrackAllTaskObjects && tracking_status())
    return TrackedTime::Now();
  return TrackedTime();  // Super fast when disabled, or not compiled.
}

// static
void ThreadData::ShutdownSingleThreadedCleanup(bool leak) {
  // This is only called from test code, where we need to cleanup so that
  // additional tests can be run.
  // We must be single threaded... but be careful anyway.
  if (!InitializeAndSetTrackingStatus(false))
    return;
  ThreadData* thread_data_list;
  ThreadDataPool* final_pool;
  {
    base::AutoLock lock(*list_lock_);
    thread_data_list = all_thread_data_list_head_;
    all_thread_data_list_head_ = NULL;
    final_pool = unregistered_thread_data_pool_;
    unregistered_thread_data_pool_ = NULL;
    ++incarnation_counter_;
  }

  // Put most global static back in pristine shape.
  thread_number_counter_ = 0;
  tls_index_.Set(NULL);
  status_ = UNINITIALIZED;

  // To avoid any chance of racing in unit tests, which is the only place we
  // call this function, we may sometimes leak all the data structures we
  // recovered, as they may still be in use on threads from prior tests!
  if (leak)
    return;

  // When we want to cleanup (on a single thread), here is what we do.

  if (final_pool) {
    // The thread_data_list contains *all* the instances, and we'll use it to
    // delete them.  This pool has pointers to some instances, and we just
    // have to drop those pointers (and not do the deletes here).
    while (!final_pool->empty())
      final_pool->pop();
    delete final_pool;
  }

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

void Snapshot::WriteHTML(std::string* output) const {
  death_data_.WriteHTML(output);
  base::StringAppendF(output, "%s->%s ",
                      birth_->birth_thread()->thread_name().c_str(),
                      DeathThreadName().c_str());
  birth_->location().Write(true, true, output);
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

DataCollector::DataCollector() {
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
    Append(*thread_data);
  }
}

DataCollector::~DataCollector() {
}

void DataCollector::Append(const ThreadData& thread_data) {
  // Get copy of data.
  ThreadData::BirthMap birth_map;
  thread_data.SnapshotBirthMap(&birth_map);
  ThreadData::DeathMap death_map;
  thread_data.SnapshotDeathMap(&death_map);

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

//------------------------------------------------------------------------------
// Aggregation

Aggregation::Aggregation()
    : birth_count_(0) {
}

Aggregation::~Aggregation() {
}

void Aggregation::AddDeathSnapshot(const Snapshot& snapshot) {
  AddBirth(snapshot.birth());
  death_threads_[snapshot.death_thread()]++;
  AddDeathData(snapshot.death_data());
}

void Aggregation::AddBirths(const Births& births) {
  AddBirth(births);
  birth_count_ += births.birth_count();
}
void Aggregation::AddBirth(const BirthOnThread& birth) {
  AddBirthPlace(birth.location());
  birth_threads_[birth.birth_thread()]++;
}

void Aggregation::AddBirthPlace(const Location& location) {
  locations_[location]++;
  birth_files_[location.file_name()]++;
}

void Aggregation::WriteHTML(std::string* output) const {
  if (locations_.size() == 1) {
    locations_.begin()->first.Write(true, true, output);
  } else {
    base::StringAppendF(output, "%" PRIuS " Locations. ", locations_.size());
    if (birth_files_.size() > 1) {
      base::StringAppendF(output, "%" PRIuS " Files. ", birth_files_.size());
    } else {
      base::StringAppendF(output, "All born in %s. ",
                          birth_files_.begin()->first.c_str());
    }
  }

  if (birth_threads_.size() > 1) {
    base::StringAppendF(output, "%" PRIuS " BirthingThreads. ",
                        birth_threads_.size());
  } else {
    base::StringAppendF(output, "All born on %s. ",
                        birth_threads_.begin()->first->thread_name().c_str());
  }

  if (death_threads_.size() > 1) {
    base::StringAppendF(output, "%" PRIuS " DeathThreads. ",
                        death_threads_.size());
  } else {
    if (death_threads_.begin()->first) {
      base::StringAppendF(output, "All deleted on %s. ",
                          death_threads_.begin()->first->thread_name().c_str());
    } else {
      output->append("All these objects are still alive.");
    }
  }

  if (birth_count_ > 1)
    base::StringAppendF(output, "Births=%d ", birth_count_);

  DeathData::WriteHTML(output);
}

void Aggregation::Clear() {
  birth_count_ = 0;
  birth_files_.clear();
  locations_.clear();
  birth_threads_.clear();
  DeathData::Clear();
  death_threads_.clear();
}

//------------------------------------------------------------------------------
// Comparison object for sorting.

Comparator::Comparator()
    : selector_(NIL),
      tiebreaker_(NULL),
      combined_selectors_(0),
      use_tiebreaker_for_sort_only_(false) {}

void Comparator::Clear() {
  if (tiebreaker_) {
    tiebreaker_->Clear();
    delete tiebreaker_;
    tiebreaker_ = NULL;
  }
  use_tiebreaker_for_sort_only_ = false;
  selector_ = NIL;
}

// static
Comparator::Selector Comparator::FindSelector(const std::string& keyword) {
  // Sorting and aggretation keywords, which specify how to sort the data, or
  // can specify a required match from the specified field in the record.
  if (0 == keyword.compare("count"))
    return COUNT;
  if (0 == keyword.compare("totalduration"))
    return TOTAL_RUN_DURATION;
  if (0 == keyword.compare("duration"))
    return AVERAGE_RUN_DURATION;
  if (0 == keyword.compare("maxduration"))
    return MAX_RUN_DURATION;
  if (0 == keyword.compare("totalqueueduration"))
    return TOTAL_QUEUE_DURATION;
  if (0 == keyword.compare("averagequeueduration"))
    return AVERAGE_QUEUE_DURATION;
  if (0 == keyword.compare("maxqueueduration"))
    return MAX_QUEUE_DURATION;
  if (0 == keyword.compare("birth"))
    return BIRTH_THREAD;
  if (0 == keyword.compare("death"))
    return DEATH_THREAD;
  if (0 == keyword.compare("file"))
    return BIRTH_FILE;
  if (0 == keyword.compare("function"))
    return BIRTH_FUNCTION;
  if (0 == keyword.compare("line"))
    return BIRTH_LINE;
  if (0 == keyword.compare("reset"))
    return RESET_ALL_DATA;
  return UNKNOWN_KEYWORD;
}

bool Comparator::operator()(const Snapshot& left,
                            const Snapshot& right) const {
  switch (selector_) {
    case BIRTH_THREAD:
      if (left.birth_thread() != right.birth_thread() &&
          left.birth_thread()->thread_name() !=
          right.birth_thread()->thread_name())
        return left.birth_thread()->thread_name() <
            right.birth_thread()->thread_name();
      break;

    case DEATH_THREAD:
      if (left.death_thread() != right.death_thread() &&
          left.DeathThreadName() !=
          right.DeathThreadName()) {
        if (!left.death_thread())
          return true;
        if (!right.death_thread())
          return false;
        return left.DeathThreadName() <
             right.DeathThreadName();
      }
      break;

    case BIRTH_FILE:
      if (left.location().file_name() != right.location().file_name()) {
        int comp = strcmp(left.location().file_name(),
                          right.location().file_name());
        if (comp)
          return 0 > comp;
      }
      break;

    case BIRTH_FUNCTION:
      if (left.location().function_name() != right.location().function_name()) {
        int comp = strcmp(left.location().function_name(),
                          right.location().function_name());
        if (comp)
          return 0 > comp;
      }
      break;

    case BIRTH_LINE:
      if (left.location().line_number() != right.location().line_number())
        return left.location().line_number() <
            right.location().line_number();
      break;

    case COUNT:
      if (left.count() != right.count())
        return left.count() > right.count();  // Sort large at front of vector.
      break;

    case AVERAGE_RUN_DURATION:
      if (!left.count() || !right.count())
        break;
      if (left.AverageMsRunDuration() != right.AverageMsRunDuration())
        return left.AverageMsRunDuration() > right.AverageMsRunDuration();
      break;

    case TOTAL_RUN_DURATION:
      if (!left.count() || !right.count())
        break;
      if (left.run_duration() != right.run_duration())
        return left.run_duration() > right.run_duration();
      break;

    case MAX_RUN_DURATION:
      if (!left.count() || !right.count())
        break;
      if (left.run_duration_max() != right.run_duration_max())
        return left.run_duration_max() > right.run_duration_max();
      break;

    case AVERAGE_QUEUE_DURATION:
      if (!left.count() || !right.count())
        break;
      if (left.AverageMsQueueDuration() != right.AverageMsQueueDuration())
        return left.AverageMsQueueDuration() > right.AverageMsQueueDuration();
      break;

    case TOTAL_QUEUE_DURATION:
      if (!left.count() || !right.count())
        break;
      if (left.queue_duration() != right.queue_duration())
        return left.queue_duration() > right.queue_duration();
      break;

    case MAX_QUEUE_DURATION:
      if (!left.count() || !right.count())
        break;
      if (left.queue_duration_max() != right.queue_duration_max())
        return left.queue_duration_max() > right.queue_duration_max();
      break;

    default:
      break;
  }
  if (tiebreaker_)
    return tiebreaker_->operator()(left, right);
  return false;
}

void Comparator::Sort(DataCollector::Collection* collection) const {
  std::sort(collection->begin(), collection->end(), *this);
}

bool Comparator::Equivalent(const Snapshot& left,
                            const Snapshot& right) const {
  switch (selector_) {
    case BIRTH_THREAD:
      if (left.birth_thread() != right.birth_thread() &&
          left.birth_thread()->thread_name() !=
              right.birth_thread()->thread_name())
        return false;
      break;

    case DEATH_THREAD:
      if (left.death_thread() != right.death_thread() &&
          left.DeathThreadName() != right.DeathThreadName())
        return false;
      break;

    case BIRTH_FILE:
      if (!required_.empty())
        break;  // No reason to aggregate when we've filtered out some.
      if (left.location().file_name() != right.location().file_name()) {
        int comp = strcmp(left.location().file_name(),
                          right.location().file_name());
        if (comp)
          return false;
      }
      break;

    case BIRTH_FUNCTION:
      if (!required_.empty())
        break;  // No reason to aggregate when we've filtered out some.
      if (left.location().function_name() != right.location().function_name()) {
        int comp = strcmp(left.location().function_name(),
                          right.location().function_name());
        if (comp)
          return false;
      }
      break;

    case COUNT:
    case AVERAGE_RUN_DURATION:
    case TOTAL_RUN_DURATION:
    case MAX_RUN_DURATION:
    case AVERAGE_QUEUE_DURATION:
    case TOTAL_QUEUE_DURATION:
    case MAX_QUEUE_DURATION:
      // We don't produce separate aggretation when only counts or times differ.
      break;

    default:
      break;
  }
  if (tiebreaker_ && !use_tiebreaker_for_sort_only_)
    return tiebreaker_->Equivalent(left, right);
  return true;
}

bool Comparator::Acceptable(const Snapshot& sample) const {
  if (required_.size()) {
    switch (selector_) {
      case BIRTH_THREAD:
        if (sample.birth_thread()->thread_name().find(required_) ==
            std::string::npos)
          return false;
        break;

      case DEATH_THREAD:
        if (sample.DeathThreadName().find(required_) == std::string::npos)
          return false;
        break;

      case BIRTH_FILE:
        if (!strstr(sample.location().file_name(), required_.c_str()))
          return false;
        break;

      case BIRTH_FUNCTION:
        if (!strstr(sample.location().function_name(), required_.c_str()))
          return false;
        break;

      default:
        break;
    }
  }
  if (tiebreaker_ && !use_tiebreaker_for_sort_only_)
    return tiebreaker_->Acceptable(sample);
  return true;
}

void Comparator::SetTiebreaker(Selector selector, const std::string& required) {
  if (selector == selector_ || NIL == selector)
    return;
  combined_selectors_ |= selector;
  if (NIL == selector_) {
    selector_ = selector;
    if (required.size())
      required_ = required;
    return;
  }
  if (tiebreaker_) {
    if (use_tiebreaker_for_sort_only_) {
      Comparator* temp = new Comparator;
      temp->tiebreaker_ = tiebreaker_;
      tiebreaker_ = temp;
    }
  } else {
    tiebreaker_ = new Comparator;
    DCHECK(!use_tiebreaker_for_sort_only_);
  }
  tiebreaker_->SetTiebreaker(selector, required);
}

bool Comparator::IsGroupedBy(Selector selector) const {
  return 0 != (selector & combined_selectors_);
}

void Comparator::SetSubgroupTiebreaker(Selector selector) {
  if (selector == selector_ || NIL == selector)
    return;
  if (!tiebreaker_) {
    use_tiebreaker_for_sort_only_ = true;
    tiebreaker_ = new Comparator;
    tiebreaker_->SetTiebreaker(selector, "");
  } else {
    tiebreaker_->SetSubgroupTiebreaker(selector);
  }
}

void Comparator::ParseKeyphrase(const std::string& key_phrase) {
  std::string required;
  // Watch for: "sort_key=value" as we parse.
  size_t equal_offset = key_phrase.find('=', 0);
  if (key_phrase.npos != equal_offset) {
    // There is a value that must be matched for the data to display.
    required = key_phrase.substr(equal_offset + 1, key_phrase.npos);
  }
  std::string keyword(key_phrase.substr(0, equal_offset));
  keyword = StringToLowerASCII(keyword);
  Selector selector = FindSelector(keyword);
  if (selector == UNKNOWN_KEYWORD)
    return;
  if (selector == RESET_ALL_DATA) {
    ThreadData::ResetAllThreadData();
    return;
  }
  SetTiebreaker(selector, required);
}

bool Comparator::ParseQuery(const std::string& query) {
  // Parse each keyphrase between consecutive slashes.
  for (size_t i = 0; i < query.size();) {
    size_t slash_offset = query.find('/', i);
    ParseKeyphrase(query.substr(i, slash_offset - i));
    if (query.npos == slash_offset)
      break;
    i = slash_offset + 1;
  }

  // Select subgroup ordering (if we want to display the subgroup)
  SetSubgroupTiebreaker(COUNT);
  SetSubgroupTiebreaker(AVERAGE_RUN_DURATION);
  SetSubgroupTiebreaker(TOTAL_RUN_DURATION);
  SetSubgroupTiebreaker(MAX_RUN_DURATION);
  SetSubgroupTiebreaker(AVERAGE_QUEUE_DURATION);
  SetSubgroupTiebreaker(TOTAL_QUEUE_DURATION);
  SetSubgroupTiebreaker(MAX_QUEUE_DURATION);
  SetSubgroupTiebreaker(BIRTH_THREAD);
  SetSubgroupTiebreaker(DEATH_THREAD);
  SetSubgroupTiebreaker(BIRTH_FUNCTION);
  SetSubgroupTiebreaker(BIRTH_FILE);
  SetSubgroupTiebreaker(BIRTH_LINE);

  return true;
}

bool Comparator::WriteSortGrouping(const Snapshot& sample,
                                       std::string* output) const {
  bool wrote_data = false;
  switch (selector_) {
    case BIRTH_THREAD:
      base::StringAppendF(output, "All new on %s ",
                          sample.birth_thread()->thread_name().c_str());
      wrote_data = true;
      break;

    case DEATH_THREAD:
      if (sample.death_thread()) {
        base::StringAppendF(output, "All deleted on %s ",
                            sample.DeathThreadName().c_str());
      } else {
        output->append("All still alive ");
      }
      wrote_data = true;
      break;

    case BIRTH_FILE:
      base::StringAppendF(output, "All born in %s ",
                          sample.location().file_name());
      break;

    case BIRTH_FUNCTION:
      output->append("All born in ");
      sample.location().WriteFunctionName(output);
      output->push_back(' ');
      break;

    default:
      break;
  }
  if (tiebreaker_ && !use_tiebreaker_for_sort_only_) {
    wrote_data |= tiebreaker_->WriteSortGrouping(sample, output);
  }
  return wrote_data;
}

void Comparator::WriteSnapshotHTML(const Snapshot& sample,
                                   std::string* output) const {
  sample.death_data().WriteHTML(output);
  if (!(combined_selectors_ & BIRTH_THREAD) ||
      !(combined_selectors_ & DEATH_THREAD))
    base::StringAppendF(output, "%s->%s ",
                        (combined_selectors_ & BIRTH_THREAD) ? "*" :
                          sample.birth().birth_thread()->thread_name().c_str(),
                        (combined_selectors_ & DEATH_THREAD) ? "*" :
                          sample.DeathThreadName().c_str());
  sample.birth().location().Write(!(combined_selectors_ & BIRTH_FILE),
                                  !(combined_selectors_ & BIRTH_FUNCTION),
                                  output);
}

}  // namespace tracked_objects
