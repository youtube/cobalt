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

using base::TimeDelta;

namespace tracked_objects {

// A TLS slot to the TrackRegistry for the current thread.
// static
base::ThreadLocalStorage::Slot ThreadData::tls_index_(base::LINKER_INITIALIZED);

// A global state variable to prevent repeated initialization during tests.
// static
AutoTracking::State AutoTracking::state_ = AutoTracking::kNeverBeenRun;

// A locked protected counter to assign sequence number to threads.
// static
int ThreadData::thread_number_counter = 0;

//------------------------------------------------------------------------------
// Death data tallies durations when a death takes place.

void DeathData::RecordDeath(const TimeDelta& queue_duration,
                            const TimeDelta& run_duration) {
  ++count_;
  queue_duration_ += queue_duration;
  run_duration_ += run_duration;
}

int DeathData::AverageMsRunDuration() const {
  return static_cast<int>(run_duration_.InMilliseconds() / count_);
}

int DeathData::AverageMsQueueDuration() const {
  return static_cast<int>(queue_duration_.InMilliseconds() / count_);
}

void DeathData::AddDeathData(const DeathData& other) {
  count_ += other.count_;
  queue_duration_ += other.queue_duration_;
  run_duration_ += other.run_duration_;
}

void DeathData::Write(std::string* output) const {
  if (!count_)
    return;
  base::StringAppendF(output, "%s:%d, ",
                      (count_ == 1) ? "Life" : "Lives", count_);
  base::StringAppendF(output, "Run:%"PRId64"ms(%dms/life) ",
                      run_duration_.InMilliseconds(),
                      AverageMsRunDuration());
  base::StringAppendF(output, "Queue:%"PRId64"ms(%dms/life) ",
                      queue_duration_.InMilliseconds(),
                      AverageMsQueueDuration());
}

void DeathData::Clear() {
  count_ = 0;
  queue_duration_ = TimeDelta();
  run_duration_ = TimeDelta();
}

//------------------------------------------------------------------------------
BirthOnThread::BirthOnThread(const Location& location)
    : location_(location),
      birth_thread_(ThreadData::Get()) { }

//------------------------------------------------------------------------------
Births::Births(const Location& location)
    : BirthOnThread(location),
      birth_count_(1) { }

//------------------------------------------------------------------------------
// ThreadData maintains the central data for all births and death.

// static
ThreadData* ThreadData::first_ = NULL;
// static
base::Lock ThreadData::list_lock_;

// static
ThreadData::Status ThreadData::status_ = ThreadData::UNINITIALIZED;

ThreadData::ThreadData(const std::string& suggested_name) : next_(NULL) {
  DCHECK_GE(suggested_name.size(), 0u);
  thread_name_ = suggested_name;
}

ThreadData::ThreadData() : next_(NULL) {
  int thread_number;
  {
    base::AutoLock lock(list_lock_);
    thread_number = ++thread_number_counter;
  }
  base::StringAppendF(&thread_name_, "WorkerThread-%d", thread_number);
}

ThreadData::~ThreadData() {}

// static
void ThreadData::InitializeThreadContext(const std::string& suggested_name) {
  if (!tls_index_.initialized())
    return;  // For unittests only.
  RegisterCurrentContext(new ThreadData(suggested_name));
}

// static
ThreadData* ThreadData::Get() {
  if (!tls_index_.initialized())
    return NULL;  // For unittests only.
  ThreadData* registered = static_cast<ThreadData*>(tls_index_.Get());
  if (!registered) {
    // We have to create a new registry entry for this ThreadData.
    // TODO(jar): Host all unamed (Worker) threads in *one* ThreadData instance,
    // (with locking protection on that instance) or else recycle and re-use
    // worker thread ThreadData when the worker thread terminates.
    registered = RegisterCurrentContext(new ThreadData());
  }
  return registered;
}

// static
ThreadData* ThreadData::RegisterCurrentContext(ThreadData* unregistered) {
  DCHECK_EQ(tls_index_.Get(), static_cast<void*>(0));
  bool too_late_to_register = false;
  {
    base::AutoLock lock(list_lock_);
    // Use lock to insure we have most recent status.
    if (!IsActive()) {
      too_late_to_register = true;
    } else {
      // Use list_lock_ to insert as new head of list.
      unregistered->next_ = first_;
      first_ = unregistered;
    }
  }  // Release lock.
  if (too_late_to_register) {
    delete unregistered;
    unregistered = NULL;
  } else {
    tls_index_.Set(unregistered);
  }
  return unregistered;
}

// static
void ThreadData::WriteHTML(const std::string& query, std::string* output) {
  if (!ThreadData::IsActive())
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
    "<li><b>AverageQueueDuration</b> Average duration in ms of queueing time."
    "<li><b>TotalQueueDuration</b> Summed durations in ms of Run() times."
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
  if (!match_array.size()) {
    output->append("There were no tracked matches.");
  } else {
    // Aggregate during printing
    Aggregation totals;
    for (size_t i = 0; i < match_array.size(); ++i) {
      totals.AddDeathSnapshot(match_array[i]);
    }
    output->append("Aggregate Stats: ");
    totals.Write(output);
    output->append("<hr><hr>");

    Aggregation subtotals;
    for (size_t i = 0; i < match_array.size(); ++i) {
      if (0 == i || !comparator.Equivalent(match_array[i - 1],
                                           match_array[i])) {
        // Print group's defining characteristics.
        comparator.WriteSortGrouping(match_array[i], output);
        output->append("<br><br>");
      }
      comparator.WriteSnapshot(match_array[i], output);
      output->append("<br>");
      subtotals.AddDeathSnapshot(match_array[i]);
      if (i + 1 >= match_array.size() ||
          !comparator.Equivalent(match_array[i],
                                 match_array[i + 1])) {
        // Print aggregate stats for the group.
        output->append("<br>");
        subtotals.Write(output);
        output->append("<br><hr><br>");
        subtotals.Clear();
      }
    }
  }
}

Births* ThreadData::TallyABirth(const Location& location) {
  BirthMap::iterator it = birth_map_.find(location);
  if (it != birth_map_.end()) {
    it->second->RecordBirth();
    return it->second;
  }

  Births* tracker = new Births(location);
  // Lock since the map may get relocated now, and other threads sometimes
  // snapshot it (but they lock before copying it).
  base::AutoLock lock(lock_);
  birth_map_[location] = tracker;
  return tracker;
}

void ThreadData::TallyADeath(const Births& the_birth,
                             const TimeDelta& queue_duration,
                             const TimeDelta& run_duration) {
  DeathMap::iterator it = death_map_.find(&the_birth);
  if (it != death_map_.end()) {
    it->second.RecordDeath(queue_duration, run_duration);
    return;
  }

  base::AutoLock lock(lock_);  // Lock since the map may get relocated now.
  death_map_[&the_birth].RecordDeath(queue_duration, run_duration);
}

// static
Births* ThreadData::TallyABirthIfActive(const Location& location) {
#if !defined(TRACK_ALL_TASK_OBJECTS)
  return NULL;  // Not compiled in.
#else
  if (!IsActive())
    return NULL;
  ThreadData* current_thread_data = Get();
  if (!current_thread_data)
    return NULL;
  return current_thread_data->TallyABirth(location);
#endif
}

// static
void ThreadData::TallyADeathIfActive(const Births* the_birth,
                                     const base::TimeTicks& time_posted,
                                     const base::TimeTicks& delayed_start_time,
                                     const base::TimeTicks& start_of_run) {
#if !defined(TRACK_ALL_TASK_OBJECTS)
  return;  // Not compiled in.
#else
  if (!IsActive() || !the_birth)
    return;
  ThreadData* current_thread_data = Get();
  if (!current_thread_data)
    return;

  // To avoid conflating our stats with the delay duration in a PostDelayedTask,
  // we identify such tasks, and replace their post_time with the time they
  // were sechudled (requested?) to emerge from the delayed task queue. This
  // means that queueing delay for such tasks will show how long they went
  // unserviced, after they *could* be serviced.  This is the same stat as we
  // have for non-delayed tasks, and we consistently call it queueing delay.
  base::TimeTicks effective_post_time =
      (delayed_start_time.is_null()) ? time_posted : delayed_start_time;
  base::TimeDelta queue_duration = start_of_run - effective_post_time;
  base::TimeDelta run_duration = Now() - start_of_run;
  current_thread_data->TallyADeath(*the_birth, queue_duration, run_duration);
#endif
}

// static
ThreadData* ThreadData::first() {
  base::AutoLock lock(list_lock_);
  return first_;
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
  ThreadData* my_list = Get()->first();

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

// static
bool ThreadData::StartTracking(bool status) {
#if !defined(TRACK_ALL_TASK_OBJECTS)
  return false;  // Not compiled in.
#else
  if (!status) {
    base::AutoLock lock(list_lock_);
    DCHECK(status_ == ACTIVE || status_ == SHUTDOWN);
    status_ = SHUTDOWN;
    return true;
  }
  base::AutoLock lock(list_lock_);
  DCHECK_EQ(UNINITIALIZED, status_);
  CHECK(tls_index_.Initialize(NULL));
  status_ = ACTIVE;
  return true;
#endif
}

// static
bool ThreadData::IsActive() {
  return status_ == ACTIVE;
}

// static
base::TimeTicks ThreadData::Now() {
#if defined(TRACK_ALL_TASK_OBJECTS)
  if (status_ == ACTIVE)
    return base::TimeTicks::Now();
#endif
  return base::TimeTicks();  // Super fast when disabled, or not compiled in.
}

// static
void ThreadData::ShutdownSingleThreadedCleanup() {
  // We must be single threaded... but be careful anyway.
  if (!StartTracking(false))
    return;
  ThreadData* thread_data_list;
  {
    base::AutoLock lock(list_lock_);
    thread_data_list = first_;
    first_ = NULL;
  }

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

  CHECK(tls_index_.initialized());
  tls_index_.Free();
  DCHECK(!tls_index_.initialized());
  status_ = UNINITIALIZED;
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

void Snapshot::Write(std::string* output) const {
  death_data_.Write(output);
  base::StringAppendF(output, "%s->%s ",
                      birth_->birth_thread()->thread_name().c_str(),
                      death_thread_->thread_name().c_str());
  birth_->location().Write(true, true, output);
}

void Snapshot::Add(const Snapshot& other) {
  death_data_.AddDeathData(other.death_data_);
}

//------------------------------------------------------------------------------
// DataCollector

DataCollector::DataCollector() {
  DCHECK(ThreadData::IsActive());

  // Get an unchanging copy of a ThreadData list.
  ThreadData* my_list = ThreadData::Get()->first();

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

void Aggregation::Write(std::string* output) const {
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

  DeathData::Write(output);
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
      if (left.location().file_name() != right.location().file_name()) {
        int comp = strcmp(left.location().file_name(),
                          right.location().file_name());
        if (comp)
          return false;
      }
      break;

    case BIRTH_FUNCTION:
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
    case AVERAGE_QUEUE_DURATION:
    case TOTAL_QUEUE_DURATION:
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
  typedef std::map<const std::string, Selector> KeyMap;
  static KeyMap key_map;
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    // Sorting and aggretation keywords, which specify how to sort the data, or
    // can specify a required match from the specified field in the record.
    key_map["count"]                = COUNT;
    key_map["totalduration"]        = TOTAL_RUN_DURATION;
    key_map["duration"]             = AVERAGE_RUN_DURATION;
    key_map["totalqueueduration"]   = TOTAL_QUEUE_DURATION;
    key_map["averagequeueduration"] = AVERAGE_QUEUE_DURATION;
    key_map["birth"]                = BIRTH_THREAD;
    key_map["death"]                = DEATH_THREAD;
    key_map["file"]                 = BIRTH_FILE;
    key_map["function"]             = BIRTH_FUNCTION;
    key_map["line"]                 = BIRTH_LINE;

    // Immediate commands that do not involve setting sort order.
    key_map["reset"]     = RESET_ALL_DATA;
  }

  std::string required;
  // Watch for: "sort_key=value" as we parse.
  size_t equal_offset = key_phrase.find('=', 0);
  if (key_phrase.npos != equal_offset) {
    // There is a value that must be matched for the data to display.
    required = key_phrase.substr(equal_offset + 1, key_phrase.npos);
  }
  std::string keyword(key_phrase.substr(0, equal_offset));
  keyword = StringToLowerASCII(keyword);
  KeyMap::iterator it = key_map.find(keyword);
  if (key_map.end() == it)
    return;  // Unknown keyword.
  if (it->second == RESET_ALL_DATA)
    ThreadData::ResetAllThreadData();
  else
    SetTiebreaker(key_map[keyword], required);
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

void Comparator::WriteSnapshot(const Snapshot& sample,
                               std::string* output) const {
  sample.death_data().Write(output);
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
