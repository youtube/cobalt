// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_runner_util.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

int ReturnFourtyTwo() {
  return 42;
}

void StoreValue(int* destination, int value) {
  *destination = value;
}

}  // namespace

TEST(TaskRunnerHelpersTest, PostAndReplyWithStatus) {
  MessageLoop message_loop;
  int result = 0;

  PostTaskAndReplyWithResult(
      message_loop.message_loop_proxy(),
      FROM_HERE,
      Bind(&ReturnFourtyTwo),
      Bind(&StoreValue, &result));

  message_loop.RunAllPending();

  EXPECT_EQ(42, result);
}

}  // namespace base
