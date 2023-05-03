// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_request_scheduler_mobile.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

// Simple method used to verify a Callback has been triggered.
void Increment(int *n) {
  (*n)++;
}

}  // namespace

TEST(VariationsRequestSchedulerMobileTest, StartNoRun) {
  TestingPrefServiceSimple prefs;
  // Initialize to as if it was just fetched. This means it should not run.
  prefs.registry()->RegisterTimePref(prefs::kVariationsLastFetchTime,
                                     base::Time::Now());
  int executed = 0;
  const base::Closure task = base::Bind(&Increment, &executed);
  VariationsRequestSchedulerMobile scheduler(task, &prefs);
  scheduler.Start();
  // We expect it the task to not have triggered.
  EXPECT_EQ(0, executed);
}

TEST(VariationsRequestSchedulerMobileTest, StartRun) {
  TestingPrefServiceSimple prefs;
  // Verify it doesn't take more than a day.
  base::Time old = base::Time::Now() - base::TimeDelta::FromHours(24);
  prefs.registry()->RegisterTimePref(prefs::kVariationsLastFetchTime, old);
  int executed = 0;
  const base::Closure task = base::Bind(&Increment, &executed);
  VariationsRequestSchedulerMobile scheduler(task, &prefs);
  scheduler.Start();
  // We expect the task to have triggered.
  EXPECT_EQ(1, executed);
}

TEST(VariationsRequestSchedulerMobileTest, OnAppEnterForegroundNoRun) {
  base::MessageLoopForUI message_loop_;

  TestingPrefServiceSimple prefs;

  // Initialize to as if it was just fetched. This means it should not run.
  prefs.registry()->RegisterTimePref(prefs::kVariationsLastFetchTime,
                                     base::Time::Now());
  int executed = 0;
  const base::Closure task = base::Bind(&Increment, &executed);
  VariationsRequestSchedulerMobile scheduler(task, &prefs);

  // Verify timer not running.
  EXPECT_FALSE(scheduler.schedule_fetch_timer_.IsRunning());
  scheduler.OnAppEnterForeground();

  // Timer now running.
  EXPECT_TRUE(scheduler.schedule_fetch_timer_.IsRunning());

  // Force execution of the task on this timer to verify that the correct task
  // was added to the timer.
  scheduler.schedule_fetch_timer_.FireNow();

  // The task should not execute because the seed was fetched too recently.
  EXPECT_EQ(0, executed);
}

TEST(VariationsRequestSchedulerMobileTest, OnAppEnterForegroundRun) {
  base::MessageLoopForUI message_loop_;

  TestingPrefServiceSimple prefs;

  base::Time old = base::Time::Now() - base::TimeDelta::FromHours(24);
  prefs.registry()->RegisterTimePref(prefs::kVariationsLastFetchTime, old);
  int executed = 0;
  const base::Closure task = base::Bind(&Increment, &executed);
  VariationsRequestSchedulerMobile scheduler(task, &prefs);

  // Verify timer not running.
  EXPECT_FALSE(scheduler.schedule_fetch_timer_.IsRunning());
  scheduler.OnAppEnterForeground();

  // Timer now running.
  EXPECT_TRUE(scheduler.schedule_fetch_timer_.IsRunning());

  // Force execution of the task on this timer to verify that the correct task
  // was added to the timer - this will verify that the right task is running.
  scheduler.schedule_fetch_timer_.FireNow();

  // We expect the input task to have triggered.
  EXPECT_EQ(1, executed);
}

TEST(VariationsRequestSchedulerMobileTest, OnAppEnterForegroundOnStartup) {
  base::MessageLoopForUI message_loop_;

  TestingPrefServiceSimple prefs;

  base::Time old = base::Time::Now() - base::TimeDelta::FromHours(24);
  prefs.registry()->RegisterTimePref(prefs::kVariationsLastFetchTime, old);
  int executed = 0;
  const base::Closure task = base::Bind(&Increment, &executed);
  VariationsRequestSchedulerMobile scheduler(task, &prefs);

  scheduler.Start();

  // We expect the task to have triggered.
  EXPECT_EQ(1, executed);

  scheduler.OnAppEnterForeground();

  EXPECT_FALSE(scheduler.schedule_fetch_timer_.IsRunning());
  // We expect the input task to not have triggered again, so executed stays
  // at 1.
  EXPECT_EQ(1, executed);

  // Simulate letting time pass.
  const base::Time last_fetch_time =
      prefs.GetTime(prefs::kVariationsLastFetchTime);
  const base::Time one_day_earlier =
      last_fetch_time - base::TimeDelta::FromHours(24);
  prefs.SetTime(prefs::kVariationsLastFetchTime, one_day_earlier);
  scheduler.last_request_time_ -= base::TimeDelta::FromHours(24);

  scheduler.OnAppEnterForeground();
  EXPECT_TRUE(scheduler.schedule_fetch_timer_.IsRunning());
  scheduler.schedule_fetch_timer_.FireNow();
  // This time it should execute the task.
  EXPECT_EQ(2, executed);
}

}  // namespace variations
