// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of FieldTrial class

#include "base/metrics/field_trial.h"

#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <limits>

namespace base {

class FieldTrialTest : public testing::Test {
 public:
  FieldTrialTest() : trial_list_("client_id") {
    Time now = Time::NowFromSystemTime();
    TimeDelta oneYear = TimeDelta::FromDays(365);
    Time::Exploded exploded;

    Time next_year_time = now + oneYear;
    next_year_time.LocalExplode(&exploded);
    next_year_ = exploded.year;

    Time last_year_time = now - oneYear;
    last_year_time.LocalExplode(&exploded);
    last_year_ = exploded.year;
  }

 protected:
  int next_year_;
  int last_year_;

 private:
  FieldTrialList trial_list_;
};

// Test registration, and also check that destructors are called for trials
// (and that Purify doesn't catch us leaking).
TEST_F(FieldTrialTest, Registration) {
  const char* name1 = "name 1 test";
  const char* name2 = "name 2 test";
  EXPECT_FALSE(FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial1 =
      new FieldTrial(name1, 10, "default name 1 test", next_year_, 12, 31);
  EXPECT_EQ(FieldTrial::kNotFinalized, trial1->group_);
  EXPECT_EQ(name1, trial1->name());
  EXPECT_EQ("", trial1->group_name_internal());

  trial1->AppendGroup("", 7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial2 =
      new FieldTrial(name2, 10, "default name 2 test", next_year_, 12, 31);
  EXPECT_EQ(FieldTrial::kNotFinalized, trial2->group_);
  EXPECT_EQ(name2, trial2->name());
  EXPECT_EQ("", trial2->group_name_internal());

  trial2->AppendGroup("a first group", 7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_EQ(trial2, FieldTrialList::Find(name2));
  // Note: FieldTrialList should delete the objects at shutdown.
}

TEST_F(FieldTrialTest, AbsoluteProbabilities) {
  char always_true[] = " always true";
  char default_always_true[] = " default always true";
  char always_false[] = " always false";
  char default_always_false[] = " default always false";
  for (int i = 1; i < 250; ++i) {
    // Try lots of names, by changing the first character of the name.
    always_true[0] = i;
    default_always_true[0] = i;
    always_false[0] = i;
    default_always_false[0] = i;

    FieldTrial* trial_true =
        new FieldTrial(
            always_true, 10, default_always_true, next_year_, 12, 31);
    const std::string winner = "TheWinner";
    int winner_group = trial_true->AppendGroup(winner, 10);

    EXPECT_EQ(winner_group, trial_true->group());
    EXPECT_EQ(winner, trial_true->group_name());

    FieldTrial* trial_false =
        new FieldTrial(
            always_false, 10, default_always_false, next_year_, 12, 31);
    int loser_group = trial_false->AppendGroup("ALoser", 0);

    EXPECT_NE(loser_group, trial_false->group());
  }
}

TEST_F(FieldTrialTest, RemainingProbability) {
  // First create a test that hasn't had a winner yet.
  const std::string winner = "Winner";
  const std::string loser = "Loser";
  scoped_refptr<FieldTrial> trial;
  int counter = 0;
  do {
    std::string name = StringPrintf("trial%d", ++counter);
    trial = new FieldTrial(name, 10, winner, next_year_, 12, 31);
    trial->AppendGroup(loser, 5);  // 50% chance of not being chosen.
    // If a group is not assigned, group_ will be kNotFinalized.
  } while (trial->group_ != FieldTrial::kNotFinalized);

  // And that 'default' group (winner) should always win.
  EXPECT_EQ(FieldTrial::kDefaultGroupNumber, trial->group());

  // And that winner should ALWAYS win.
  EXPECT_EQ(winner, trial->group_name());
}

TEST_F(FieldTrialTest, FiftyFiftyProbability) {
  // Check that even with small divisors, we have the proper probabilities, and
  // all outcomes are possible.  Since this is a 50-50 test, it should get both
  // outcomes in a few tries, but we'll try no more than 100 times (and be flaky
  // with probability around 1 in 2^99).
  bool first_winner = false;
  bool second_winner = false;
  int counter = 0;
  do {
    std::string name = base::StringPrintf("FiftyFifty%d", ++counter);
    std::string default_group_name = base::StringPrintf("Default FiftyFifty%d",
                                                        ++counter);
    scoped_refptr<FieldTrial> trial(
        new FieldTrial(name, 2, default_group_name, next_year_, 12, 31));
    trial->AppendGroup("first", 1);  // 50% chance of being chosen.
    // If group_ is kNotFinalized, then a group assignement hasn't been done.
    if (trial->group_ != FieldTrial::kNotFinalized) {
      first_winner = true;
      continue;
    }
    trial->AppendGroup("second", 1);  // Always chosen at this point.
    EXPECT_NE(FieldTrial::kNotFinalized, trial->group());
    second_winner = true;
  } while ((!second_winner || !first_winner) && counter < 100);
  EXPECT_TRUE(second_winner);
  EXPECT_TRUE(first_winner);
}

TEST_F(FieldTrialTest, MiddleProbabilities) {
  char name[] = " same name";
  char default_group_name[] = " default same name";
  bool false_event_seen = false;
  bool true_event_seen = false;
  for (int i = 1; i < 250; ++i) {
    name[0] = i;
    default_group_name[0] = i;
    FieldTrial* trial =
        new FieldTrial(name, 10, default_group_name, next_year_, 12, 31);
    int might_win = trial->AppendGroup("MightWin", 5);

    if (trial->group() == might_win) {
      true_event_seen = true;
    } else {
      false_event_seen = true;
    }
    if (false_event_seen && true_event_seen)
      return;  // Successful test!!!
  }
  // Very surprising to get here. Probability should be around 1 in 2 ** 250.
  // One of the following will fail.
  EXPECT_TRUE(false_event_seen);
  EXPECT_TRUE(true_event_seen);
}

TEST_F(FieldTrialTest, OneWinner) {
  char name[] = "Some name";
  char default_group_name[] = "Default some name";
  int group_count(10);

  FieldTrial* trial =
      new FieldTrial(
          name, group_count, default_group_name, next_year_, 12, 31);
  int winner_index(-2);
  std::string winner_name;

  for (int i = 1; i <= group_count; ++i) {
    int might_win = trial->AppendGroup("", 1);

    // Because we keep appending groups, we want to see if the last group that
    // was added has been assigned or not.
    if (trial->group_ == might_win) {
      EXPECT_EQ(-2, winner_index);
      winner_index = might_win;
      StringAppendF(&winner_name, "%d", might_win);
      EXPECT_EQ(winner_name, trial->group_name());
    }
  }
  EXPECT_GE(winner_index, 0);
  EXPECT_EQ(trial->group(), winner_index);
  EXPECT_EQ(trial->group_name(), winner_name);
}

TEST_F(FieldTrialTest, DisableProbability) {
  const std::string default_group_name = "Default group";
  const std::string loser = "Loser";
  const std::string name = "Trial";

  // Create a field trail that has expired.
  scoped_refptr<FieldTrial> trial;
  trial = new FieldTrial(
      name, 1000000000, default_group_name, last_year_, 1, 1);
  trial->AppendGroup(loser, 999999999);  // 99.9999999% chance of being chosen.

  // Because trial has expired, we should always be in the default group.
  EXPECT_EQ(FieldTrial::kDefaultGroupNumber, trial->group());

  // And that default_group_name should ALWAYS win.
  EXPECT_EQ(default_group_name, trial->group_name());
}

TEST_F(FieldTrialTest, Save) {
  std::string save_string;

  FieldTrial* trial =
      new FieldTrial(
          "Some name", 10, "Default some name", next_year_, 12, 31);
  // There is no winner yet, so no textual group name is associated with trial.
  // In this case, the trial should not be included.
  EXPECT_EQ("", trial->group_name_internal());
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("", save_string);
  save_string.clear();

  // Create a winning group.
  trial->AppendGroup("Winner", 10);
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/", save_string);
  save_string.clear();

  // Create a second trial and winning group.
  FieldTrial* trial2 =
      new FieldTrial("xxx", 10, "Default xxx", next_year_, 12, 31);
  trial2->AppendGroup("yyyy", 10);

  FieldTrialList::StatesToString(&save_string);
  // We assume names are alphabetized... though this is not critical.
  EXPECT_EQ("Some name/Winner/xxx/yyyy/", save_string);
}

TEST_F(FieldTrialTest, Restore) {
  EXPECT_TRUE(FieldTrialList::Find("Some_name") == NULL);
  EXPECT_TRUE(FieldTrialList::Find("xxx") == NULL);

  FieldTrialList::CreateTrialsInChildProcess("Some_name/Winner/xxx/yyyy/");

  FieldTrial* trial = FieldTrialList::Find("Some_name");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ("Winner", trial->group_name());
  EXPECT_EQ("Some_name", trial->name());

  trial = FieldTrialList::Find("xxx");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ("yyyy", trial->group_name());
  EXPECT_EQ("xxx", trial->name());
}

TEST_F(FieldTrialTest, BogusRestore) {
  EXPECT_FALSE(FieldTrialList::CreateTrialsInChildProcess("MissingSlash"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsInChildProcess("MissingGroupName/"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsInChildProcess(
      "MissingFinalSlash/gname"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsInChildProcess(
      "noname, only group/"));
}

TEST_F(FieldTrialTest, DuplicateRestore) {
  FieldTrial* trial =
      new FieldTrial(
          "Some name", 10, "Default some name", next_year_, 12, 31);
  trial->AppendGroup("Winner", 10);
  std::string save_string;
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/", save_string);

  // It is OK if we redundantly specify a winner.
  EXPECT_TRUE(FieldTrialList::CreateTrialsInChildProcess(save_string));

  // But it is an error to try to change to a different winner.
  EXPECT_FALSE(FieldTrialList::CreateTrialsInChildProcess(
      "Some name/Loser/"));
}

TEST_F(FieldTrialTest, CreateFieldTrial) {
  EXPECT_TRUE(FieldTrialList::Find("Some_name") == NULL);

  FieldTrialList::CreateFieldTrial("Some_name", "Winner");

  FieldTrial* trial = FieldTrialList::Find("Some_name");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ("Winner", trial->group_name());
  EXPECT_EQ("Some_name", trial->name());
}

TEST_F(FieldTrialTest, DuplicateFieldTrial) {
  FieldTrial* trial =
      new FieldTrial(
          "Some_name", 10, "Default some name", next_year_, 12, 31);
  trial->AppendGroup("Winner", 10);

  // It is OK if we redundantly specify a winner.
  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Some_name", "Winner");
  EXPECT_TRUE(trial1 != NULL);

  // But it is an error to try to change to a different winner.
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Some_name", "Loser");
  EXPECT_TRUE(trial2 == NULL);
}

TEST_F(FieldTrialTest, MakeName) {
  FieldTrial* trial =
      new FieldTrial("Field Trial", 10, "Winner", next_year_, 12, 31);
  trial->group();
  EXPECT_EQ("Histogram_Winner",
            FieldTrial::MakeName("Histogram", "Field Trial"));
}

TEST_F(FieldTrialTest, HashClientId) {
  double results[] = {
      FieldTrial::HashClientId("hi", "1"),
      FieldTrial::HashClientId("there", "1"),
  };
  ASSERT_NE(results[0], results[1]);
  for (size_t i = 0; i < arraysize(results); ++i) {
    ASSERT_LE(0.0, results[i]);
    ASSERT_GT(1.0, results[i]);
  }

  ASSERT_EQ(FieldTrial::HashClientId("yo", "1"),
            FieldTrial::HashClientId("yo", "1"));
  ASSERT_NE(FieldTrial::HashClientId("yo", "something"),
            FieldTrial::HashClientId("yo", "else"));
}

TEST_F(FieldTrialTest, HashClientIdIsUniform) {
  // Choose a random start number but go sequentially from there, so
  // that each test tries a different range but we never provide uniformly
  // distributed input data.
  int current_number = RandInt(0, std::numeric_limits<int>::max());

  // The expected value of a random distribution is the average over all
  // samples as the number of samples approaches infinity.  For a uniform
  // distribution from [0.0, 1.0) this would be 0.5.
  //
  // We do kSamplesBetweenChecks at a time and check if the value has converged
  // to a narrow interval around 0.5.  A non-uniform distribution would likely
  // converge at something different, or not converge consistently within this
  // range (i.e. the test would start timing out occasionally).
  int kSamplesBetweenChecks = 300;
  int num_samples = 0;
  double total_value = 0.0;
  while (true) {
    for (int i = 0; i < kSamplesBetweenChecks; ++i) {
      total_value += FieldTrial::HashClientId(
          IntToString(current_number++), "salt");
      num_samples++;
    }

    double average = total_value / num_samples;
    double kExpectedMin = 0.48;
    double kExpectedMax = 0.52;

    if (num_samples > 1000 &&
        (average < kExpectedMin || average > kExpectedMax)) {
      // Only printed once we have enough samples that it's very unlikely
      // things haven't converged.
      printf("After %d samples, the average was %f, outside the expected\n"
             "range (%f, %f). We will add more samples and check after every\n"
             "%d samples. If the average does not converge, something\n"
             "is broken. If it does converge, the test will pass.\n",
             num_samples, average,
             kExpectedMin, kExpectedMax, kSamplesBetweenChecks);
    } else {
      // Success.
      break;
    }
  }
}

TEST_F(FieldTrialTest, UseOneTimeRandomization) {
  // Simply asserts that two trials using one-time randomization
  // that have different names, normally generate different results.
  //
  // Note that depending on the one-time random initialization, they
  // _might_ actually give the same result, but we know that given
  // the particular client_id we use for unit tests they won't.
  scoped_refptr<FieldTrial> trials[] = {
    new FieldTrial("one", 100, "default", next_year_, 1, 1),
    new FieldTrial("two", 100, "default", next_year_, 1, 1),
  };

  for (size_t i = 0; i < arraysize(trials); ++i) {
    trials[i]->UseOneTimeRandomization();

    for (int j = 0; j < 100; ++j) {
      trials[i]->AppendGroup("", 1);
    }
  }

  // The trials are most likely to give different results since they have
  // different names.
  ASSERT_NE(trials[0]->group(), trials[1]->group());
  ASSERT_NE(trials[0]->group_name(), trials[1]->group_name());
}

TEST_F(FieldTrialTest, DisableImmediately) {
  FieldTrial* trial =
      new FieldTrial("trial", 100, "default", next_year_, 12, 31);
  trial->Disable();
  ASSERT_EQ("default", trial->group_name());
  ASSERT_EQ(FieldTrial::kDefaultGroupNumber, trial->group());
}

TEST_F(FieldTrialTest, DisableAfterInitialization) {
  FieldTrial* trial =
      new FieldTrial("trial", 100, "default", next_year_, 12, 31);
  trial->AppendGroup("non_default", 100);
  ASSERT_EQ("non_default", trial->group_name());
  trial->Disable();
  ASSERT_EQ("default", trial->group_name());
}

}  // namespace base
