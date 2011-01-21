// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_completion_callback.h"

#include "base/message_loop.h"
#include "net/base/net_errors.h"

TestCompletionCallback::TestCompletionCallback()
    : result_(0),
      have_result_(false),
      waiting_for_result_(false) {
}

TestCompletionCallback::~TestCompletionCallback() {}

int TestCompletionCallback::WaitForResult() {
  DCHECK(!waiting_for_result_);
  while (!have_result_) {
    waiting_for_result_ = true;
    MessageLoop::current()->Run();
    waiting_for_result_ = false;
  }
  have_result_ = false;  // auto-reset for next callback
  return result_;
}

int TestCompletionCallback::GetResult(int result) {
  if (net::ERR_IO_PENDING != result)
    return result;
  return WaitForResult();
}

void TestCompletionCallback::RunWithParams(const Tuple1<int>& params) {
  result_ = params.a;
  have_result_ = true;
  if (waiting_for_result_)
    MessageLoop::current()->Quit();
}
