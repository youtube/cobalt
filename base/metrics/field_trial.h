// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FieldTrial is a class for handling details of statistical experiments
// performed by actual users in the field (i.e., in a shipped or beta product).
// All code is called exclusively on the UI thread currently.
//
// The simplest example is an experiment to see whether one of two options
// produces "better" results across our user population.  In that scenario, UMA
// data is uploaded to aggregate the test results, and this FieldTrial class
// manages the state of each such experiment (state == which option was
// pseudo-randomly selected).
//
// States are typically generated randomly, either based on a one time
// randomization (generated randomly once, and then persistently reused in the
// client during each future run of the program), or by a startup randomization
// (generated each time the application starts up, but held constant during the
// duration of the process), or by continuous randomization across a run (where
// the state can be recalculated again and again, many times during a process).
// Only startup randomization is implemented thus far.

//------------------------------------------------------------------------------
// Example:  Suppose we have an experiment involving memory, such as determining
// the impact of some pruning algorithm.
// We assume that we already have a histogram of memory usage, such as:

//   HISTOGRAM_COUNTS("Memory.RendererTotal", count);

// Somewhere in main thread initialization code, we'd probably define an
// instance of a FieldTrial, with code such as:

// // Note, FieldTrials are reference counted, and persist automagically until
// // process teardown, courtesy of their automatic registration in
// // FieldTrialList.
// scoped_refptr<FieldTrial> trial = new FieldTrial("MemoryExperiment", 1000);
// int group1 = trial->AppendGroup("high_mem", 20);  // 2% in high_mem group.
// int group2 = trial->AppendGroup("low_mem", 20);   // 2% in low_mem group.
// // Take action depending of which group we randomly land in.
// if (trial->group() == group1)
//   SetPruningAlgorithm(kType1);  // Sample setting of browser state.
// else if (trial->group() == group2)
//   SetPruningAlgorithm(kType2);  // Sample alternate setting.

// We then modify any histograms we wish to correlate with our experiment to
// have slighly different names, depending on what group the trial instance
// happened (randomly) to be assigned to:

// HISTOGRAM_COUNTS(FieldTrial::MakeName("Memory.RendererTotal",
//                                       "MemoryExperiment").data(), count);

// The above code will create 3 distinct histograms, with each run of the
// application being assigned to of of the three groups, and for each group, the
// correspondingly named histogram will be populated:

// Memory.RendererTotal            // 96% of users still fill this histogram.
// Memory.RendererTotal_high_mem   // 2% of users will fill this histogram.
// Memory.RendererTotal_low_mem    // 2% of users will fill this histogram.

//------------------------------------------------------------------------------

#ifndef BASE_METRICS_FIELD_TRIAL_H_
#define BASE_METRICS_FIELD_TRIAL_H_
#pragma once

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time.h"

namespace base {

class FieldTrialList;

class FieldTrial : public RefCounted<FieldTrial> {
 public:
  typedef int Probability;  // Probability type for being selected in a trial.

  // A return value to indicate that a given instance has not yet had a group
  // assignment (and hence is not yet participating in the trial).
  static const int kNotFinalized;

  // This is the group number of the 'default' group. This provides an easy way
  // to assign all the remaining probability to a group ('default').
  static const int kDefaultGroupNumber;

  // The name is used to register the instance with the FieldTrialList class,
  // and can be used to find the trial (only one trial can be present for each
  // name).
  // Group probabilities that are later supplied must sum to less than or equal
  // to the total_probability. Arguments year, month and day_of_month specify
  // the expiration time. If the build time is after the expiration time then
  // the field trial reverts to the 'default' group.
  FieldTrial(const std::string& name, Probability total_probability,
             const std::string& default_group_name, const int year,
             const int month, const int day_of_month);

  // Establish the name and probability of the next group in this trial.
  // Sometimes, based on construction randomization, this call may cause the
  // provided group to be *THE* group selected for use in this instance.
  int AppendGroup(const std::string& name, Probability group_probability);

  // Return the name of the FieldTrial (excluding the group name).
  std::string name() const { return name_; }

  // Return the randomly selected group number that was assigned.
  // Return kDefaultGroupNumber if the instance is in the 'default' group.
  // Note that this will force an instance to participate, and make it illegal
  // to attempt to probabalistically add any other groups to the trial.
  int group();

  // If the field trial is not in an experiment, this returns the empty string.
  // if the group's name is empty, a name of "_" concatenated with the group
  // number is used as the group name.
  std::string group_name();

  // Return the default group name of the FieldTrial.
  std::string default_group_name() const { return default_group_name_; }

  // Helper function for the most common use: as an argument to specifiy the
  // name of a HISTOGRAM.  Use the original histogram name as the name_prefix.
  static std::string MakeName(const std::string& name_prefix,
                              const std::string& trial_name);

  // Enable benchmarking sets field trials to a common setting.
  static void EnableBenchmarking();

 private:
  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST(FieldTrialTest, Registration);
  FRIEND_TEST(FieldTrialTest, AbsoluteProbabilities);
  FRIEND_TEST(FieldTrialTest, RemainingProbability);
  FRIEND_TEST(FieldTrialTest, FiftyFiftyProbability);
  FRIEND_TEST(FieldTrialTest, MiddleProbabilities);
  FRIEND_TEST(FieldTrialTest, OneWinner);
  FRIEND_TEST(FieldTrialTest, DisableProbability);
  FRIEND_TEST(FieldTrialTest, Save);
  FRIEND_TEST(FieldTrialTest, DuplicateRestore);
  FRIEND_TEST(FieldTrialTest, MakeName);

  friend class base::FieldTrialList;

  friend class RefCounted<FieldTrial>;

  virtual ~FieldTrial();

  // Returns the group_name. A winner need not have been chosen.
  std::string group_name_internal() const { return group_name_; }

  // Get build time.
  static Time GetBuildTime();

  // The name of the field trial, as can be found via the FieldTrialList.
  // This is empty of the trial is not in the experiment.
  const std::string name_;

  // The maximum sum of all probabilities supplied, which corresponds to 100%.
  // This is the scaling factor used to adjust supplied probabilities.
  const Probability divisor_;

  // The name of the default group.
  const std::string default_group_name_;

  // The randomly selected probability that is used to select a group (or have
  // the instance not participate).  It is the product of divisor_ and a random
  // number between [0, 1).
  Probability random_;

  // Sum of the probabilities of all appended groups.
  Probability accumulated_group_probability_;

  int next_group_number_;

  // The pseudo-randomly assigned group number.
  // This is kNotFinalized if no group has been assigned.
  int group_;

  // A textual name for the randomly selected group. If this Trial is not a
  // member of an group, this string is empty.
  std::string group_name_;

  // When disable_field_trial_ is true, field trial reverts to the 'default'
  // group.
  bool disable_field_trial_;

  // When benchmarking is enabled, field trials all revert to the 'default'
  // group.
  static bool enable_benchmarking_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrial);
};

//------------------------------------------------------------------------------
// Class with a list of all active field trials.  A trial is active if it has
// been registered, which includes evaluating its state based on its probaility.
// Only one instance of this class exists.
class FieldTrialList {
 public:
  // Define a separator charactor to use when creating a persistent form of an
  // instance.  This is intended for use as a command line argument, passed to a
  // second process to mimic our state (i.e., provide the same group name).
  static const char kPersistentStringSeparator;  // Currently a slash.

  // This singleton holds the global list of registered FieldTrials.
  FieldTrialList();
  // Destructor Release()'s references to all registered FieldTrial instances.
  ~FieldTrialList();

  // Register() stores a pointer to the given trial in a global map.
  // This method also AddRef's the indicated trial.
  static void Register(FieldTrial* trial);

  // The Find() method can be used to test to see if a named Trial was already
  // registered, or to retrieve a pointer to it from the global map.
  static FieldTrial* Find(const std::string& name);

  static int FindValue(const std::string& name);

  static std::string FindFullName(const std::string& name);

  // Create a persistent representation of all FieldTrial instances for
  // resurrection in another process.  This allows randomization to be done in
  // one process, and secondary processes can by synchronized on the result.
  // The resulting string contains only the names, the trial name, and a "/"
  // separator.
  static void StatesToString(std::string* output);

  // Use a previously generated state string (re: StatesToString()) augment the
  // current list of field tests to include the supplied tests, and using a 100%
  // probability for each test, force them to have the same group string.  This
  // is commonly used in a sub-process, to carry randomly selected state in a
  // parent process into this sub-process.
  //  Currently only the group_name_ and name_ are restored.
  static bool CreateTrialsInChildProcess(const std::string& prior_trials);

  // The time of construction of the global map is recorded in a static variable
  // and is commonly used by experiments to identify the time since the start
  // of the application.  In some experiments it may be useful to discount
  // data that is gathered before the application has reached sufficient
  // stability (example: most DLL have loaded, etc.)
  static TimeTicks application_start_time() {
    if (global_)
      return global_->application_start_time_;
    // For testing purposes only, or when we don't yet have a start time.
    return TimeTicks::Now();
  }

  // Return the number of active field trials.
  static size_t GetFieldTrialCount();

 private:
  // A map from FieldTrial names to the actual instances.
  typedef std::map<std::string, FieldTrial*> RegistrationList;

  // Helper function should be called only while holding lock_.
  FieldTrial* PreLockedFind(const std::string& name);

  static FieldTrialList* global_;  // The singleton of this class.

  // This will tell us if there is an attempt to register a field trial without
  // creating the FieldTrialList. This is not an error, unless a FieldTrialList
  // is created after that.
  static bool register_without_global_;

  // A helper value made availabel to users, that shows when the FieldTrialList
  // was initialized.  Note that this is a singleton instance, and hence is a
  // good approximation to the start of the process.
  TimeTicks application_start_time_;

  // Lock for access to registered_.
  base::Lock lock_;
  RegistrationList registered_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialList);
};

}  // namespace base

#endif  // BASE_METRICS_FIELD_TRIAL_H_

