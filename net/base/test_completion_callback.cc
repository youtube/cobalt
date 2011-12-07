// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_completion_callback.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/net_errors.h"

TestOldCompletionCallback::TestOldCompletionCallback()
    : result_(0),
      have_result_(false),
      waiting_for_result_(false) {
}

TestOldCompletionCallback::~TestOldCompletionCallback() {}

int TestOldCompletionCallback::WaitForResult() {
  DCHECK(!waiting_for_result_);
  while (!have_result_) {
    waiting_for_result_ = true;
    MessageLoop::current()->Run();
    waiting_for_result_ = false;
  }
  have_result_ = false;  // auto-reset for next callback
  return result_;
}

int TestOldCompletionCallback::GetResult(int result) {
  if (net::ERR_IO_PENDING != result)
    return result;
  return WaitForResult();
}

void TestOldCompletionCallback::RunWithParams(const Tuple1<int>& params) {
  result_ = params.a;
  have_result_ = true;
  if (waiting_for_result_)
    MessageLoop::current()->Quit();
}

namespace net {

TestCompletionCallback::TestCompletionCallback()
    : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
        base::Bind(&TestCompletionCallback::OnComplete,
                   base::Unretained(this)))) {
}

TestCompletionCallback::~TestCompletionCallback() {}

void TestCompletionCallback::OnComplete(int result) {
  old_callback_impl_.RunWithParams(Tuple1<int>(result));
}

}  // namespace net
