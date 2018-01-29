// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Default group name used by several tests.
const char kDefaultGroupName[] = "DefaultGroup";

// FieldTrialList::Observer implementation for testing.
class TestFieldTrialObserver : public FieldTrialList::Observer {
 public:
  TestFieldTrialObserver() {
    FieldTrialList::AddObserver(this);
  }

  virtual ~TestFieldTrialObserver() {
    FieldTrialList::RemoveObserver(this);
  }

  virtual void OnFieldTrialGroupFinalized(const std::string& trial,
                                          const std::string& group) override {
    trial_name_ = trial;
    group_name_ = group;
  }

  const std::string& trial_name() const { return trial_name_; }
  const std::string& group_name() const { return group_name_; }

 private:
  std::string trial_name_;
  std::string group_name_;

  DISALLOW_COPY_AND_ASSIGN(TestFieldTrialObserver);
};

}  // namespace

class FieldTrialTest : public testing::Test {
 public:
  FieldTrialTest() : trial_list_(NULL) {
    Time now = Time::NowFromSystemTime();
    TimeDelta one_year = TimeDelta::FromDays(365);
    Time::Exploded exploded;

    Time next_year_time = now + one_year;
    next_year_time.LocalExplode(&exploded);
    next_year_ = exploded.year;

    Time last_year_time = now - one_year;
    last_year_time.LocalExplode(&exploded);
    last_year_ = exploded.year;
  }

 protected:
  int next_year_;
  int last_year_;
  MessageLoop message_loop_;

 private:
  FieldTrialList trial_list_;
};

// Test registration, and also check that destructors are called for trials
// (and that Valgrind doesn't catch us leaking).
TEST_F(FieldTrialTest, Registration) {
  const char* name1 = "name 1 test";
  const char* name2 = "name 2 test";
  EXPECT_FALSE(FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial1 = FieldTrialList::FactoryGetFieldTrial(
      name1, 10, "default name 1 test", next_year_, 12, 31, NULL);
  EXPECT_EQ(FieldTrial::kNotFinalized, trial1->group_);
  EXPECT_EQ(name1, trial1->trial_name());
  EXPECT_EQ("", trial1->group_name_internal());

  trial1->AppendGroup("", 7);

  EXPECT_EQ(trial1, FieldTrialList::Find(name1));
  EXPECT_FALSE(FieldTrialList::Find(name2));

  FieldTrial* trial2 = FieldTrialList::FactoryGetFieldTrial(
      name2, 10, "default name 2 test", next_year_, 12, 31, NULL);
  EXPECT_EQ(FieldTrial::kNotFinalized, trial2->group_);
  EXPECT_EQ(name2, trial2->trial_name());
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

    FieldTrial* trial_true = FieldTrialList::FactoryGetFieldTrial(
        always_true, 10, default_always_true, next_year_, 12, 31, NULL);
    const std::string winner = "TheWinner";
    int winner_group = trial_true->AppendGroup(winner, 10);

    EXPECT_EQ(winner_group, trial_true->group());
    EXPECT_EQ(winner, trial_true->group_name());

    FieldTrial* trial_false = FieldTrialList::FactoryGetFieldTrial(
        always_false, 10, default_always_false, next_year_, 12, 31, NULL);
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
  int default_group_number = -1;
  do {
    std::string name = StringPrintf("trial%d", ++counter);
    trial = FieldTrialList::FactoryGetFieldTrial(
        name, 10, winner, next_year_, 12, 31, &default_group_number);
    trial->AppendGroup(loser, 5);  // 50% chance of not being chosen.
    // If a group is not assigned, group_ will be kNotFinalized.
  } while (trial->group_ != FieldTrial::kNotFinalized);

  // And that 'default' group (winner) should always win.
  EXPECT_EQ(default_group_number, trial->group());

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
    scoped_refptr<FieldTrial> trial(FieldTrialList::FactoryGetFieldTrial(
        name, 2, default_group_name, next_year_, 12, 31, NULL));
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
    FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
        name, 10, default_group_name, next_year_, 12, 31, NULL);
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

  int default_group_number = -1;
  FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
      name, group_count, default_group_name, next_year_, 12, 31,
      &default_group_number);
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
  // Since all groups cover the total probability, we should not have
  // chosen the default group.
  EXPECT_NE(trial->group(), default_group_number);
  EXPECT_EQ(trial->group(), winner_index);
  EXPECT_EQ(trial->group_name(), winner_name);
}

TEST_F(FieldTrialTest, DisableProbability) {
  const std::string default_group_name = "Default group";
  const std::string loser = "Loser";
  const std::string name = "Trial";

  // Create a field trail that has expired.
  int default_group_number = -1;
  scoped_refptr<FieldTrial> trial;
  trial = FieldTrialList::FactoryGetFieldTrial(
      name, 1000000000, default_group_name, last_year_, 1, 1,
      &default_group_number);
  trial->AppendGroup(loser, 999999999);  // 99.9999999% chance of being chosen.

  // Because trial has expired, we should always be in the default group.
  EXPECT_EQ(default_group_number, trial->group());

  // And that default_group_name should ALWAYS win.
  EXPECT_EQ(default_group_name, trial->group_name());
}

TEST_F(FieldTrialTest, ActiveGroups) {
  std::string no_group("No Group");
  scoped_refptr<FieldTrial> trial(FieldTrialList::FactoryGetFieldTrial(
      no_group, 10, "Default", next_year_, 12, 31, NULL));

  // There is no winner yet, so no NameGroupId should be returned.
  FieldTrial::ActiveGroup active_group;
  EXPECT_FALSE(trial->GetActiveGroup(&active_group));

  // Create a single winning group.
  std::string one_winner("One Winner");
  trial = FieldTrialList::FactoryGetFieldTrial(
      one_winner, 10, "Default", next_year_, 12, 31, NULL);
  std::string winner("Winner");
  trial->AppendGroup(winner, 10);
  EXPECT_FALSE(trial->GetActiveGroup(&active_group));
  // Finalize the group selection by accessing the selected group.
  trial->group();
  EXPECT_TRUE(trial->GetActiveGroup(&active_group));
  EXPECT_EQ(one_winner, active_group.trial_name);
  EXPECT_EQ(winner, active_group.group_name);

  std::string multi_group("MultiGroup");
  scoped_refptr<FieldTrial> multi_group_trial =
      FieldTrialList::FactoryGetFieldTrial(multi_group, 9, "Default",
                                            next_year_, 12, 31, NULL);

  multi_group_trial->AppendGroup("Me", 3);
  multi_group_trial->AppendGroup("You", 3);
  multi_group_trial->AppendGroup("Them", 3);
  EXPECT_FALSE(multi_group_trial->GetActiveGroup(&active_group));
  // Finalize the group selection by accessing the selected group.
  multi_group_trial->group();
  EXPECT_TRUE(multi_group_trial->GetActiveGroup(&active_group));
  EXPECT_EQ(multi_group, active_group.trial_name);
  EXPECT_EQ(multi_group_trial->group_name(), active_group.group_name);

  // Now check if the list is built properly...
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_EQ(2U, active_groups.size());
  for (size_t i = 0; i < active_groups.size(); ++i) {
    // Order is not guaranteed, so check all values.
    EXPECT_NE(no_group, active_groups[i].trial_name);
    EXPECT_TRUE(one_winner != active_groups[i].trial_name ||
                winner == active_groups[i].group_name);
    EXPECT_TRUE(multi_group != active_groups[i].trial_name ||
                multi_group_trial->group_name() == active_groups[i].group_name);
  }
}

TEST_F(FieldTrialTest, ActiveGroupsNotFinalized) {
  const char kTrialName[] = "TestTrial";
  const char kSecondaryGroupName[] = "SecondaryGroup";

  int default_group = -1;
  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, kDefaultGroupName,
                                           next_year_, 12, 31, &default_group);
  const int secondary_group = trial->AppendGroup(kSecondaryGroupName, 50);

  // Before |group()| is called, |GetActiveGroup()| should return false.
  FieldTrial::ActiveGroup active_group;
  EXPECT_FALSE(trial->GetActiveGroup(&active_group));

  // |GetActiveFieldTrialGroups()| should also not include the trial.
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_TRUE(active_groups.empty());

  // After |group()| has been called, both APIs should succeed.
  const int chosen_group = trial->group();
  EXPECT_TRUE(chosen_group == default_group || chosen_group == secondary_group);

  EXPECT_TRUE(trial->GetActiveGroup(&active_group));
  EXPECT_EQ(kTrialName, active_group.trial_name);
  if (chosen_group == default_group)
    EXPECT_EQ(kDefaultGroupName, active_group.group_name);
  else
    EXPECT_EQ(kSecondaryGroupName, active_group.group_name);

  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  ASSERT_EQ(1U, active_groups.size());
  EXPECT_EQ(kTrialName, active_groups[0].trial_name);
  EXPECT_EQ(active_group.group_name, active_groups[0].group_name);
}

TEST_F(FieldTrialTest, Save) {
  std::string save_string;

  FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
      "Some name", 10, "Default some name", next_year_, 12, 31, NULL);
  // There is no winner yet, so no textual group name is associated with trial.
  // In this case, the trial should not be included.
  EXPECT_EQ("", trial->group_name_internal());
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("", save_string);
  save_string.clear();

  // Create a winning group.
  trial->AppendGroup("Winner", 10);
  // Finalize the group selection by accessing the selected group.
  trial->group();
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/", save_string);
  save_string.clear();

  // Create a second trial and winning group.
  FieldTrial* trial2 = FieldTrialList::FactoryGetFieldTrial(
      "xxx", 10, "Default xxx", next_year_, 12, 31, NULL);
  trial2->AppendGroup("yyyy", 10);
  // Finalize the group selection by accessing the selected group.
  trial2->group();

  FieldTrialList::StatesToString(&save_string);
  // We assume names are alphabetized... though this is not critical.
  EXPECT_EQ("Some name/Winner/xxx/yyyy/", save_string);
  save_string.clear();

  // Create a third trial with only the default group.
  FieldTrial* trial3 = FieldTrialList::FactoryGetFieldTrial(
      "zzz", 10, "default", next_year_, 12, 31, NULL);
  // Finalize the group selection by accessing the selected group.
  trial3->group();

  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/xxx/yyyy/zzz/default/", save_string);
}

TEST_F(FieldTrialTest, Restore) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Some_name"));
  ASSERT_FALSE(FieldTrialList::TrialExists("xxx"));

  FieldTrialList::CreateTrialsFromString("Some_name/Winner/xxx/yyyy/");

  FieldTrial* trial = FieldTrialList::Find("Some_name");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ("Winner", trial->group_name());
  EXPECT_EQ("Some_name", trial->trial_name());

  trial = FieldTrialList::Find("xxx");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ("yyyy", trial->group_name());
  EXPECT_EQ("xxx", trial->trial_name());
}

TEST_F(FieldTrialTest, BogusRestore) {
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("MissingSlash"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("MissingGroupName/"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString(
      "MissingFinalSlash/gname"));
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString(
      "noname, only group/"));
}

TEST_F(FieldTrialTest, DuplicateRestore) {
  FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
      "Some name", 10, "Default some name", next_year_, 12, 31, NULL);
  trial->AppendGroup("Winner", 10);
  // Finalize the group selection by accessing the selected group.
  trial->group();
  std::string save_string;
  FieldTrialList::StatesToString(&save_string);
  EXPECT_EQ("Some name/Winner/", save_string);

  // It is OK if we redundantly specify a winner.
  EXPECT_TRUE(FieldTrialList::CreateTrialsFromString(save_string));

  // But it is an error to try to change to a different winner.
  EXPECT_FALSE(FieldTrialList::CreateTrialsFromString("Some name/Loser/"));
}

TEST_F(FieldTrialTest, CreateTrialsFromStringAreActive) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Abc"));
  ASSERT_FALSE(FieldTrialList::TrialExists("Xyz"));
  ASSERT_TRUE(FieldTrialList::CreateTrialsFromString("Abc/def/Xyz/zyx/"));

  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  ASSERT_EQ(2U, active_groups.size());
  EXPECT_EQ("Abc", active_groups[0].trial_name);
  EXPECT_EQ("def", active_groups[0].group_name);
  EXPECT_EQ("Xyz", active_groups[1].trial_name);
  EXPECT_EQ("zyx", active_groups[1].group_name);
}

TEST_F(FieldTrialTest, CreateTrialsFromStringObserver) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Abc"));

  TestFieldTrialObserver observer;
  ASSERT_TRUE(FieldTrialList::CreateTrialsFromString("Abc/def/"));

  message_loop_.RunUntilIdle();
  EXPECT_EQ("Abc", observer.trial_name());
  EXPECT_EQ("def", observer.group_name());
}

TEST_F(FieldTrialTest, CreateFieldTrial) {
  ASSERT_FALSE(FieldTrialList::TrialExists("Some_name"));

  FieldTrialList::CreateFieldTrial("Some_name", "Winner");

  FieldTrial* trial = FieldTrialList::Find("Some_name");
  ASSERT_NE(static_cast<FieldTrial*>(NULL), trial);
  EXPECT_EQ("Winner", trial->group_name());
  EXPECT_EQ("Some_name", trial->trial_name());
}

TEST_F(FieldTrialTest, CreateFieldTrialIsNotActive) {
  const char kTrialName[] = "CreateFieldTrialIsActiveTrial";
  const char kWinnerGroup[] = "Winner";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));
  FieldTrialList::CreateFieldTrial(kTrialName, kWinnerGroup);

  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_TRUE(active_groups.empty());
}

TEST_F(FieldTrialTest, DuplicateFieldTrial) {
  FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
      "Some_name", 10, "Default some name", next_year_, 12, 31, NULL);
  trial->AppendGroup("Winner", 10);

  // It is OK if we redundantly specify a winner.
  FieldTrial* trial1 = FieldTrialList::CreateFieldTrial("Some_name", "Winner");
  EXPECT_TRUE(trial1 != NULL);

  // But it is an error to try to change to a different winner.
  FieldTrial* trial2 = FieldTrialList::CreateFieldTrial("Some_name", "Loser");
  EXPECT_TRUE(trial2 == NULL);
}

TEST_F(FieldTrialTest, MakeName) {
  FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
      "Field Trial", 10, "Winner", next_year_, 12, 31, NULL);
  trial->group();
  EXPECT_EQ("Histogram_Winner",
            FieldTrial::MakeName("Histogram", "Field Trial"));
}

TEST_F(FieldTrialTest, DisableImmediately) {
  int default_group_number = -1;
  FieldTrial* trial = FieldTrialList::FactoryGetFieldTrial(
      "trial", 100, "default", next_year_, 12, 31, &default_group_number);
  trial->Disable();
  ASSERT_EQ("default", trial->group_name());
  ASSERT_EQ(default_group_number, trial->group());
}

TEST_F(FieldTrialTest, DisableAfterInitialization) {
  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial("trial", 100, "default",
                                            next_year_, 12, 31, NULL);
  trial->AppendGroup("non_default", 100);
  trial->Disable();
  ASSERT_EQ("default", trial->group_name());
}

TEST_F(FieldTrialTest, ForcedFieldTrials) {
  // Validate we keep the forced choice.
  FieldTrial* forced_trial = FieldTrialList::CreateFieldTrial("Use the",
                                                              "Force");
  EXPECT_STREQ("Force", forced_trial->group_name().c_str());

  int default_group_number = -1;
  FieldTrial* factory_trial = FieldTrialList::FactoryGetFieldTrial(
      "Use the", 1000, "default", next_year_, 12, 31, &default_group_number);
  EXPECT_EQ(factory_trial, forced_trial);

  int chosen_group = factory_trial->AppendGroup("Force", 100);
  EXPECT_EQ(chosen_group, factory_trial->group());
  int not_chosen_group = factory_trial->AppendGroup("Dark Side", 100);
  EXPECT_NE(chosen_group, not_chosen_group);

  // Since we didn't force the default group, we should not be returned the
  // chosen group as the default group.
  EXPECT_NE(default_group_number, chosen_group);
  int new_group = factory_trial->AppendGroup("Duck Tape", 800);
  EXPECT_NE(chosen_group, new_group);
  // The new group should not be the default group either.
  EXPECT_NE(default_group_number, new_group);

  // Forcing the default should use the proper group ID.
  forced_trial = FieldTrialList::CreateFieldTrial("Trial Name", "Default");
  factory_trial = FieldTrialList::FactoryGetFieldTrial(
      "Trial Name", 1000, "Default", next_year_, 12, 31, &default_group_number);
  EXPECT_EQ(forced_trial, factory_trial);

  int other_group = factory_trial->AppendGroup("Not Default", 100);
  EXPECT_STREQ("Default", factory_trial->group_name().c_str());
  EXPECT_EQ(default_group_number, factory_trial->group());
  EXPECT_NE(other_group, factory_trial->group());

  int new_other_group = factory_trial->AppendGroup("Not Default Either", 800);
  EXPECT_NE(new_other_group, factory_trial->group());
}

TEST_F(FieldTrialTest, SetForced) {
  // Start by setting a trial for which we ensure a winner...
  int default_group_number = -1;
  FieldTrial* forced_trial = FieldTrialList::FactoryGetFieldTrial(
      "Use the", 1, "default", next_year_, 12, 31, &default_group_number);
  EXPECT_EQ(forced_trial, forced_trial);

  int forced_group = forced_trial->AppendGroup("Force", 1);
  EXPECT_EQ(forced_group, forced_trial->group());

  // Now force it.
  forced_trial->SetForced();

  // Now try to set it up differently as a hard coded registration would.
  FieldTrial* hard_coded_trial = FieldTrialList::FactoryGetFieldTrial(
      "Use the", 1, "default", next_year_, 12, 31, &default_group_number);
  EXPECT_EQ(hard_coded_trial, forced_trial);

  int would_lose_group = hard_coded_trial->AppendGroup("Force", 0);
  EXPECT_EQ(forced_group, hard_coded_trial->group());
  EXPECT_EQ(forced_group, would_lose_group);

  // Same thing if we would have done it to win again.
  FieldTrial* other_hard_coded_trial = FieldTrialList::FactoryGetFieldTrial(
      "Use the", 1, "default", next_year_, 12, 31, &default_group_number);
  EXPECT_EQ(other_hard_coded_trial, forced_trial);

  int would_win_group = other_hard_coded_trial->AppendGroup("Force", 1);
  EXPECT_EQ(forced_group, other_hard_coded_trial->group());
  EXPECT_EQ(forced_group, would_win_group);
}

TEST_F(FieldTrialTest, SetForcedDefaultOnly) {
  const char kTrialName[] = "SetForcedDefaultOnly";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  int default_group = -1;
  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, kDefaultGroupName,
                                           next_year_, 12, 31, &default_group);
  trial->SetForced();

  trial = FieldTrialList::FactoryGetFieldTrial(kTrialName, 100,
                                               kDefaultGroupName, next_year_,
                                               12, 31, NULL);
  EXPECT_EQ(default_group, trial->group());
  EXPECT_EQ(kDefaultGroupName, trial->group_name());
}

TEST_F(FieldTrialTest, SetForcedDefaultWithExtraGroup) {
  const char kTrialName[] = "SetForcedDefaultWithExtraGroup";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  int default_group = -1;
  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, kDefaultGroupName,
                                           next_year_, 12, 31, &default_group);
  trial->SetForced();

  trial = FieldTrialList::FactoryGetFieldTrial(kTrialName, 100,
                                               kDefaultGroupName, next_year_,
                                               12, 31, NULL);
  const int extra_group = trial->AppendGroup("Extra", 100);
  EXPECT_EQ(default_group, trial->group());
  EXPECT_NE(extra_group, trial->group());
  EXPECT_EQ(kDefaultGroupName, trial->group_name());
}

TEST_F(FieldTrialTest, Observe) {
  const char kTrialName[] = "TrialToObserve1";
  const char kSecondaryGroupName[] = "SecondaryGroup";

  TestFieldTrialObserver observer;
  int default_group = -1;
  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, kDefaultGroupName,
                                           next_year_, 12, 31, &default_group);
  const int secondary_group = trial->AppendGroup(kSecondaryGroupName, 50);
  const int chosen_group = trial->group();
  EXPECT_TRUE(chosen_group == default_group || chosen_group == secondary_group);

  message_loop_.RunUntilIdle();
  EXPECT_EQ(kTrialName, observer.trial_name());
  if (chosen_group == default_group)
    EXPECT_EQ(kDefaultGroupName, observer.group_name());
  else
    EXPECT_EQ(kSecondaryGroupName, observer.group_name());
}

TEST_F(FieldTrialTest, ObserveDisabled) {
  const char kTrialName[] = "TrialToObserve2";

  TestFieldTrialObserver observer;
  int default_group = -1;
  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, kDefaultGroupName,
                                           next_year_, 12, 31, &default_group);
  trial->AppendGroup("A", 25);
  trial->AppendGroup("B", 25);
  trial->AppendGroup("C", 25);
  trial->Disable();

  // Observer shouldn't be notified of a disabled trial.
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());

  // Observer shouldn't be notified even after a |group()| call.
  EXPECT_EQ(default_group, trial->group());
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());
}

TEST_F(FieldTrialTest, ObserveForcedDisabled) {
  const char kTrialName[] = "TrialToObserve3";

  TestFieldTrialObserver observer;
  int default_group = -1;
  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, kDefaultGroupName,
                                           next_year_, 12, 31, &default_group);
  trial->AppendGroup("A", 25);
  trial->AppendGroup("B", 25);
  trial->AppendGroup("C", 25);
  trial->SetForced();
  trial->Disable();

  // Observer shouldn't be notified of a disabled trial, even when forced.
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());

  // Observer shouldn't be notified even after a |group()| call.
  EXPECT_EQ(default_group, trial->group());
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(observer.trial_name().empty());
  EXPECT_TRUE(observer.group_name().empty());
}

TEST_F(FieldTrialTest, DisabledTrialNotActive) {
  const char kTrialName[] = "DisabledTrial";
  ASSERT_FALSE(FieldTrialList::TrialExists(kTrialName));

  FieldTrial* trial =
      FieldTrialList::FactoryGetFieldTrial(kTrialName, 100, kDefaultGroupName,
                                           next_year_, 12, 31, NULL);
  trial->AppendGroup("X", 50);
  trial->Disable();

  // Ensure the trial is not listed as active.
  FieldTrial::ActiveGroups active_groups;
  FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  EXPECT_TRUE(active_groups.empty());

  // Ensure the trial is not listed in the |StatesToString()| result.
  std::string states;
  FieldTrialList::StatesToString(&states);
  EXPECT_TRUE(states.empty());
}


}  // namespace base
