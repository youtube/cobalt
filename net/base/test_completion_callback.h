// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_COMPLETION_CALLBACK_H_
#define NET_BASE_TEST_COMPLETION_CALLBACK_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/tuple.h"
#include "net/base/completion_callback.h"

//-----------------------------------------------------------------------------
// completion callback helper

// A helper class for completion callbacks, designed to make it easy to run
// tests involving asynchronous operations.  Just call WaitForResult to wait
// for the asynchronous operation to complete.
//
// NOTE: Since this runs a message loop to wait for the completion callback,
// there could be other side-effects resulting from WaitForResult.  For this
// reason, this class is probably not ideal for a general application.
//

// Base class overridden by custom implementations of TestCompletionCallback.
class TestCompletionCallbackBase {
 public:
  void SetResult(int result);
  int WaitForResult();
  int GetResult(int result);
  bool have_result() const { return have_result_; }

 protected:
  TestCompletionCallbackBase();

  int result_;
  bool have_result_;
  bool waiting_for_result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallbackBase);
};

namespace net {

class TestCompletionCallback : public TestCompletionCallbackBase {
 public:
  TestCompletionCallback();
  ~TestCompletionCallback();

  const CompletionCallback& callback() const { return callback_; }

 private:
  const CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallback);
};

}  // namespace net

#endif  // NET_BASE_TEST_COMPLETION_CALLBACK_H_
