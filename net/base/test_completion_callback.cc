// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_completion_callback.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/net_errors.h"

void TestCompletionCallbackBase::SetResult(int result) {
  result_ = result;
  have_result_ = true;
  if (waiting_for_result_)
    MessageLoop::current()->Quit();
}

int TestCompletionCallbackBase::WaitForResult() {
  DCHECK(!waiting_for_result_);

  while (!have_result_) {
    waiting_for_result_ = true;
    MessageLoop::current()->Run();
    waiting_for_result_ = false;
  }

  have_result_ = false;  // Auto-reset for next callback.
  return result_;
}

int TestCompletionCallbackBase::GetResult(int result) {
  if (net::ERR_IO_PENDING != result)
    return result;

  return WaitForResult();
}

TestCompletionCallbackBase::TestCompletionCallbackBase()
    : result_(0),
      have_result_(false),
      waiting_for_result_(false) {
}

void TestOldCompletionCallback::RunWithParams(const Tuple1<int>& params) {
  SetResult(params.a);
}

namespace net {

TestCompletionCallback::TestCompletionCallback()
    : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
        base::Bind(&TestCompletionCallback::SetResult,
                   base::Unretained(this)))) {
}

TestCompletionCallback::~TestCompletionCallback() {}


}  // namespace net
