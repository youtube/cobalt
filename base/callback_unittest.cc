// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/scoped_ptr.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class HelperObject {
 public:
  HelperObject() : next_number_(0) { }
  int GetNextNumber() { return ++next_number_; }
  void GetNextNumberArg(int* number) { *number = GetNextNumber(); }

 private:
  int next_number_;
};

struct FakeTraits {
  static void DoInvoke(internal::InvokerStorageBase*) {
  }
};

// White-box testpoints to inject into a Callback<> object for checking
// comparators and emptiness APIs.
class FakeInvokerStorage1 : public internal::InvokerStorageBase {
 public:
  typedef FakeTraits FunctionTraits;
};

class FakeInvokerStorage2 : public internal::InvokerStorageBase {
 public:
  typedef FakeTraits FunctionTraits;
};

TEST(CallbackOld, OneArg) {
  HelperObject obj;
  scoped_ptr<Callback1<int*>::Type> callback(
      NewCallback(&obj, &HelperObject::GetNextNumberArg));

  int number = 0;
  callback->Run(&number);
  EXPECT_EQ(number, 1);
}

TEST(CallbackOld, ReturnValue) {
  HelperObject obj;
  scoped_ptr<CallbackWithReturnValue<int>::Type> callback(
      NewCallbackWithReturnValue(&obj, &HelperObject::GetNextNumber));

  EXPECT_EQ(callback->Run(), 1);
}

class CallbackTest : public ::testing::Test {
 public:
  CallbackTest()
      : callback_a_(MakeInvokerStorageHolder(new FakeInvokerStorage1())),
        callback_b_(MakeInvokerStorageHolder(new FakeInvokerStorage2())) {
  }

  virtual ~CallbackTest() {
  }

 protected:
  Callback<void(void)> callback_a_;
  const Callback<void(void)> callback_b_;  // Ensure APIs work with const.
  Callback<void(void)> null_callback_;
};

// Ensure we can create unbound callbacks. We need this to be able to store
// them in class members that can be initialized later.
TEST_F(CallbackTest, DefaultConstruction) {
  Callback<void(void)> c0;
  Callback<void(int)> c1;
  Callback<void(int,int)> c2;
  Callback<void(int,int,int)> c3;
  Callback<void(int,int,int,int)> c4;
  Callback<void(int,int,int,int,int)> c5;
  Callback<void(int,int,int,int,int,int)> c6;

  EXPECT_TRUE(c0.is_null());
  EXPECT_TRUE(c1.is_null());
  EXPECT_TRUE(c2.is_null());
  EXPECT_TRUE(c3.is_null());
  EXPECT_TRUE(c4.is_null());
  EXPECT_TRUE(c5.is_null());
  EXPECT_TRUE(c6.is_null());
}

TEST_F(CallbackTest, IsNull) {
  EXPECT_TRUE(null_callback_.is_null());
  EXPECT_FALSE(callback_a_.is_null());
  EXPECT_FALSE(callback_b_.is_null());
}

TEST_F(CallbackTest, Equals) {
  EXPECT_TRUE(callback_a_.Equals(callback_a_));
  EXPECT_FALSE(callback_a_.Equals(callback_b_));
  EXPECT_FALSE(callback_b_.Equals(callback_a_));

  // We should compare based on instance, not type.
  Callback<void(void)> callback_c(
      MakeInvokerStorageHolder(new FakeInvokerStorage1()));
  Callback<void(void)> callback_a2 = callback_a_;
  EXPECT_TRUE(callback_a_.Equals(callback_a2));
  EXPECT_FALSE(callback_a_.Equals(callback_c));

  // Empty, however, is always equal to empty.
  Callback<void(void)> empty2;
  EXPECT_TRUE(null_callback_.Equals(empty2));
}

TEST_F(CallbackTest, Reset) {
  // Resetting should bring us back to empty.
  ASSERT_FALSE(callback_a_.is_null());
  ASSERT_FALSE(callback_a_.Equals(null_callback_));

  callback_a_.Reset();

  EXPECT_TRUE(callback_a_.is_null());
  EXPECT_TRUE(callback_a_.Equals(null_callback_));
}

}  // namespace
}  // namespace base
