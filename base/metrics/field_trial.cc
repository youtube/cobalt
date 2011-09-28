// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/sha1.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"

namespace base {

// static
const int FieldTrial::kNotFinalized = -1;

// static
const int FieldTrial::kDefaultGroupNumber = 0;

// static
bool FieldTrial::enable_benchmarking_ = false;

// static
const char FieldTrialList::kPersistentStringSeparator('/');

static const char kHistogramFieldTrialSeparator('_');

// static
int FieldTrialList::kExpirationYearInFuture = 0;


//------------------------------------------------------------------------------
// FieldTrial methods and members.

FieldTrial::FieldTrial(const std::string& name,
                       const Probability total_probability,
                       const std::string& default_group_name,
                       const int year,
                       const int month,
                       const int day_of_month)
  : name_(name),
    divisor_(total_probability),
    default_group_name_(default_group_name),
    random_(static_cast<Probability>(divisor_ * RandDouble())),
    accumulated_group_probability_(0),
    next_group_number_(kDefaultGroupNumber + 1),
    group_(kNotFinalized),
    enable_field_trial_(true) {
  DCHECK_GT(total_probability, 0);
  DCHECK(!name_.empty());
  DCHECK(!default_group_name_.empty());
  FieldTrialList::Register(this);

  DCHECK_GT(year, 1970);
  DCHECK_GT(month, 0);
  DCHECK_LT(month, 13);
  DCHECK_GT(day_of_month, 0);
  DCHECK_LT(day_of_month, 32);

  Time::Exploded exploded;
  exploded.year = year;
  exploded.month = month;
  exploded.day_of_week = 0;  // Should be unused.
  exploded.day_of_month = day_of_month;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  Time expiration_time = Time::FromLocalExploded(exploded);
  if (GetBuildTime() > expiration_time)
    Disable();
}

void FieldTrial::UseOneTimeRandomization() {
  DCHECK_EQ(group_, kNotFinalized);
  DCHECK_EQ(kDefaultGroupNumber + 1, next_group_number_);
  if (!FieldTrialList::IsOneTimeRandomizationEnabled()) {
    NOTREACHED();
    Disable();
    return;
  }

  random_ = static_cast<Probability>(
      divisor_ * HashClientId(FieldTrialList::client_id(), name_));
}

void FieldTrial::Disable() {
  enable_field_trial_ = false;

  // In case we are disabled after initialization, we need to switch
  // the trial to the default group.
  if (group_ != kNotFinalized) {
    group_ = kDefaultGroupNumber;
    group_name_ = default_group_name_;
  }
}

int FieldTrial::AppendGroup(const std::string& name,
                            Probability group_probability) {
  DCHECK_LE(group_probability, divisor_);
  DCHECK_GE(group_probability, 0);

  if (enable_benchmarking_ || !enable_field_trial_)
    group_probability = 0;

  accumulated_group_probability_ += group_probability;

  DCHECK_LE(accumulated_group_probability_, divisor_);
  if (group_ == kNotFinalized && accumulated_group_probability_ > random_) {
    // This is the group that crossed the random line, so we do the assignment.
    group_ = next_group_number_;
    if (name.empty())
      StringAppendF(&group_name_, "%d", group_);
    else
      group_name_ = name;
    FieldTrialList::NotifyFieldTrialGroupSelection(name_, group_name_);
  }
  return next_group_number_++;
}

int FieldTrial::group() {
  if (group_ == kNotFinalized) {
    accumulated_group_probability_ = divisor_;
    group_ = kDefaultGroupNumber;
    group_name_ = default_group_name_;
    FieldTrialList::NotifyFieldTrialGroupSelection(name_, group_name_);
  }
  return group_;
}

std::string FieldTrial::group_name() {
  group();  // call group() to make sure group assignment was done.
  DCHECK(!group_name_.empty());
  return group_name_;
}

// static
std::string FieldTrial::MakeName(const std::string& name_prefix,
                                 const std::string& trial_name) {
  std::string big_string(name_prefix);
  big_string.append(1, kHistogramFieldTrialSeparator);
  return big_string.append(FieldTrialList::FindFullName(trial_name));
}

// static
void FieldTrial::EnableBenchmarking() {
  DCHECK_EQ(0u, FieldTrialList::GetFieldTrialCount());
  enable_benchmarking_ = true;
}

FieldTrial::~FieldTrial() {}

// static
Time FieldTrial::GetBuildTime() {
  Time integral_build_time;
  const char* kDateTime = __DATE__ " " __TIME__;
  bool result = Time::FromString(kDateTime, &integral_build_time);
  DCHECK(result);
  return integral_build_time;
}

// static
double FieldTrial::HashClientId(const std::string& client_id,
                                const std::string& trial_name) {
  // SHA-1 is designed to produce a uniformly random spread in its output space,
  // even for nearly-identical inputs, so it helps massage whatever client_id
  // and trial_name we get into something with a uniform distribution, which
  // is desirable so that we don't skew any part of the 0-100% spectrum.
  std::string input(client_id + trial_name);
  unsigned char sha1_hash[kSHA1Length];
  SHA1HashBytes(reinterpret_cast<const unsigned char*>(input.c_str()),
                input.size(),
                sha1_hash);

  COMPILE_ASSERT(sizeof(uint64) < sizeof(sha1_hash), need_more_data);
  uint64* bits = reinterpret_cast<uint64*>(&sha1_hash[0]);

  return BitsToOpenEndedUnitInterval(*bits);
}

//------------------------------------------------------------------------------
// FieldTrialList methods and members.

// static
FieldTrialList* FieldTrialList::global_ = NULL;

// static
bool FieldTrialList::used_without_global_ = false;

FieldTrialList::FieldTrialList(const std::string& client_id)
    : application_start_time_(TimeTicks::Now()),
      client_id_(client_id),
      observer_list_(ObserverList<Observer>::NOTIFY_EXISTING_ONLY) {
  DCHECK(!global_);
  DCHECK(!used_without_global_);
  global_ = this;

  Time::Exploded exploded;
  Time two_years_from_now =
      Time::NowFromSystemTime() + TimeDelta::FromDays(730);
  two_years_from_now.LocalExplode(&exploded);
  kExpirationYearInFuture = exploded.year;
}

FieldTrialList::~FieldTrialList() {
  AutoLock auto_lock(lock_);
  while (!registered_.empty()) {
    RegistrationList::iterator it = registered_.begin();
    it->second->Release();
    registered_.erase(it->first);
  }
  DCHECK_EQ(this, global_);
  global_ = NULL;
}

// static
void FieldTrialList::Register(FieldTrial* trial) {
  if (!global_) {
    used_without_global_ = true;
    return;
  }
  AutoLock auto_lock(global_->lock_);
  DCHECK(!global_->PreLockedFind(trial->name()));
  trial->AddRef();
  global_->registered_[trial->name()] = trial;
}

// static
FieldTrial* FieldTrialList::Find(const std::string& name) {
  if (!global_)
    return NULL;
  AutoLock auto_lock(global_->lock_);
  return global_->PreLockedFind(name);
}

// static
int FieldTrialList::FindValue(const std::string& name) {
  FieldTrial* field_trial = Find(name);
  if (field_trial)
    return field_trial->group();
  return FieldTrial::kNotFinalized;
}

// static
std::string FieldTrialList::FindFullName(const std::string& name) {
  FieldTrial* field_trial = Find(name);
  if (field_trial)
    return field_trial->group_name();
  return "";
}

// static
bool FieldTrialList::TrialExists(const std::string& name) {
  return Find(name) != NULL;
}

// static
void FieldTrialList::StatesToString(std::string* output) {
  if (!global_)
    return;
  DCHECK(output->empty());
  AutoLock auto_lock(global_->lock_);

  for (RegistrationList::iterator it = global_->registered_.begin();
       it != global_->registered_.end(); ++it) {
    const std::string name = it->first;
    std::string group_name = it->second->group_name_internal();
    if (group_name.empty())
      continue;  // Should not include uninitialized trials.
    DCHECK_EQ(name.find(kPersistentStringSeparator), std::string::npos);
    DCHECK_EQ(group_name.find(kPersistentStringSeparator), std::string::npos);
    output->append(name);
    output->append(1, kPersistentStringSeparator);
    output->append(group_name);
    output->append(1, kPersistentStringSeparator);
  }
}

// static
bool FieldTrialList::CreateTrialsInChildProcess(
    const std::string& parent_trials) {
  DCHECK(global_);
  if (parent_trials.empty() || !global_)
    return true;

  size_t next_item = 0;
  while (next_item < parent_trials.length()) {
    size_t name_end = parent_trials.find(kPersistentStringSeparator, next_item);
    if (name_end == parent_trials.npos || next_item == name_end)
      return false;
    size_t group_name_end = parent_trials.find(kPersistentStringSeparator,
                                               name_end + 1);
    if (group_name_end == parent_trials.npos || name_end + 1 == group_name_end)
      return false;
    std::string name(parent_trials, next_item, name_end - next_item);
    std::string group_name(parent_trials, name_end + 1,
                           group_name_end - name_end - 1);
    next_item = group_name_end + 1;

    if (!CreateFieldTrial(name, group_name))
      return false;
  }
  return true;
}

// static
FieldTrial* FieldTrialList::CreateFieldTrial(
    const std::string& name,
    const std::string& group_name) {
  DCHECK(global_);
  DCHECK_GE(name.size(), 0u);
  DCHECK_GE(group_name.size(), 0u);
  if (name.empty() || group_name.empty() || !global_)
    return NULL;

  FieldTrial *field_trial(FieldTrialList::Find(name));
  if (field_trial) {
    // In single process mode, we may have already created the field trial.
    if (field_trial->group_name_internal() != group_name)
      return NULL;
    return field_trial;
  }
  const int kTotalProbability = 100;
  field_trial = new FieldTrial(name, kTotalProbability, group_name,
                               kExpirationYearInFuture, 1, 1);
  field_trial->AppendGroup(group_name, kTotalProbability);
  return field_trial;
}

// static
void FieldTrialList::AddObserver(Observer* observer) {
  if (!global_)
    return;
  DCHECK(global_);
  global_->observer_list_.AddObserver(observer);
}

// static
void FieldTrialList::RemoveObserver(Observer* observer) {
  if (!global_)
    return;
  DCHECK(global_);
  global_->observer_list_.RemoveObserver(observer);
}

// static
void FieldTrialList::NotifyFieldTrialGroupSelection(
    const std::string& name,
    const std::string& group_name) {
  if (!global_)
    return;
  DCHECK(global_);
  FOR_EACH_OBSERVER(Observer,
                    global_->observer_list_,
                    OnFieldTrialGroupFinalized(name, group_name));
}

// static
size_t FieldTrialList::GetFieldTrialCount() {
  if (!global_)
    return 0;
  AutoLock auto_lock(global_->lock_);
  return global_->registered_.size();
}

// static
bool FieldTrialList::IsOneTimeRandomizationEnabled() {
  if (!global_) {
    used_without_global_ = true;
    return false;
  }

  return !global_->client_id_.empty();
}

// static
const std::string& FieldTrialList::client_id() {
  DCHECK(global_);
  if (!global_)
    return EmptyString();

  return global_->client_id_;
}

FieldTrial* FieldTrialList::PreLockedFind(const std::string& name) {
  RegistrationList::iterator it = registered_.find(name);
  if (registered_.end() == it)
    return NULL;
  return it->second;
}

}  // namespace base
