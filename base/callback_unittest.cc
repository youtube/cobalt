// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/scoped_ptr.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HelperObject {
 public:
  HelperObject() : next_number_(0) { }
  int GetNextNumber() { return ++next_number_; }
  void GetNextNumberArg(int* number) { *number = GetNextNumber(); }

 private:
  int next_number_;
};

}  // namespace

TEST(Callback, OneArg) {
  HelperObject obj;
  scoped_ptr<Callback1<int*>::Type> callback(
      NewCallback(&obj, &HelperObject::GetNextNumberArg));

  int number = 0;
  callback->Run(&number);
  EXPECT_EQ(number, 1);
}

TEST(Callback, ReturnValue) {
  HelperObject obj;
  scoped_ptr<CallbackWithReturnValue<int>::Type> callback(
      NewCallbackWithReturnValue(&obj, &HelperObject::GetNextNumber));

  EXPECT_EQ(callback->Run(), 1);
}
