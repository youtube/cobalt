// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bind_to_loop.h"

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

void BoundBoolSet(bool* var, bool val) {
  *var = val;
}

void BoundBoolSetFromScopedPtr(bool* var, scoped_ptr<bool> val) {
  *var = *val;
}

void BoundBoolSetFromScopedPtrMalloc(bool* var, scoped_ptr_malloc<bool> val) {
  *var = val;
}

void BoundBoolSetFromScopedArray(bool* var, scoped_array<bool> val) {
  *var = val[0];
}

void BoundBoolSetFromConstRef(bool* var, const bool& val) {
  *var = val;
}

void BoundIntegersSet(int* a_var, int* b_var, int a_val, int b_val) {
  *a_var = a_val;
  *b_var = b_val;
}

// Various tests that check that the bound function is only actually executed
// on the message loop, not during the original Run.
class BindToLoopTest : public ::testing::Test {
 public:
  BindToLoopTest() : proxy_(loop_.message_loop_proxy()) {}

 protected:
  MessageLoop loop_;
  scoped_refptr<base::MessageLoopProxy> proxy_;
};

TEST_F(BindToLoopTest, Closure) {
  // Test the closure is run inside the loop, not outside it.
  base::WaitableEvent waiter(false, false);
  base::Closure cb = BindToLoop(proxy_, base::Bind(
      &base::WaitableEvent::Signal, base::Unretained(&waiter)));
  cb.Run();
  EXPECT_FALSE(waiter.IsSignaled());
  loop_.RunUntilIdle();
  EXPECT_TRUE(waiter.IsSignaled());
}

TEST_F(BindToLoopTest, Bool) {
  bool bool_var = false;
  base::Callback<void(bool)> cb = BindToLoop(proxy_, base::Bind(
      &BoundBoolSet, &bool_var));
  cb.Run(true);
  EXPECT_FALSE(bool_var);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_var);
}

TEST_F(BindToLoopTest, BoundScopedPtrBool) {
  bool bool_val = false;
  scoped_ptr<bool> scoped_ptr_bool(new bool(true));
  base::Closure cb = BindToLoop(proxy_, base::Bind(
      &BoundBoolSetFromScopedPtr, &bool_val, base::Passed(&scoped_ptr_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToLoopTest, PassedScopedPtrBool) {
  bool bool_val = false;
  scoped_ptr<bool> scoped_ptr_bool(new bool(true));
  base::Callback<void(scoped_ptr<bool>)> cb = BindToLoop(proxy_, base::Bind(
      &BoundBoolSetFromScopedPtr, &bool_val));
  cb.Run(scoped_ptr_bool.Pass());
  EXPECT_FALSE(bool_val);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToLoopTest, BoundScopedArrayBool) {
  bool bool_val = false;
  scoped_array<bool> scoped_array_bool(new bool[1]);
  scoped_array_bool[0] = true;
  base::Closure cb = BindToLoop(proxy_, base::Bind(
      &BoundBoolSetFromScopedArray, &bool_val,
      base::Passed(&scoped_array_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToLoopTest, PassedScopedArrayBool) {
  bool bool_val = false;
  scoped_array<bool> scoped_array_bool(new bool[1]);
  scoped_array_bool[0] = true;
  base::Callback<void(scoped_array<bool>)> cb = BindToLoop(proxy_, base::Bind(
      &BoundBoolSetFromScopedArray, &bool_val));
  cb.Run(scoped_array_bool.Pass());
  EXPECT_FALSE(bool_val);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToLoopTest, BoundScopedPtrMallocBool) {
  bool bool_val = false;
  scoped_ptr_malloc<bool> scoped_ptr_malloc_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *scoped_ptr_malloc_bool = true;
  base::Closure cb = BindToLoop(proxy_, base::Bind(
      &BoundBoolSetFromScopedPtrMalloc, &bool_val,
      base::Passed(&scoped_ptr_malloc_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToLoopTest, PassedScopedPtrMallocBool) {
  bool bool_val = false;
  scoped_ptr_malloc<bool> scoped_ptr_malloc_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *scoped_ptr_malloc_bool = true;
  base::Callback<void(scoped_ptr_malloc<bool>)> cb = BindToLoop(
      proxy_, base::Bind(&BoundBoolSetFromScopedPtrMalloc, &bool_val));
  cb.Run(scoped_ptr_malloc_bool.Pass());
  EXPECT_FALSE(bool_val);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToLoopTest, BoolConstRef) {
  bool bool_var = false;
  bool true_var = true;
  const bool& true_ref = true_var;
  base::Closure cb = BindToLoop(proxy_, base::Bind(
      &BoundBoolSetFromConstRef, &bool_var, true_ref));
  cb.Run();
  EXPECT_FALSE(bool_var);
  loop_.RunUntilIdle();
  EXPECT_TRUE(bool_var);
}

TEST_F(BindToLoopTest, Integers) {
  int a = 0;
  int b = 0;
  base::Callback<void(int, int)> cb = BindToLoop(proxy_, base::Bind(
      &BoundIntegersSet, &a, &b));
  cb.Run(1, -1);
  EXPECT_EQ(a, 0);
  EXPECT_EQ(b, 0);
  loop_.RunUntilIdle();
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, -1);
}

}  // namespace media
