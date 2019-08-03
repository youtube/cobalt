// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_completion_callback.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"

namespace net {

namespace internal {

void TestCompletionCallbackBaseInternal::DidSetResult() {
  have_result_ = true;
  if (waiting_for_result_)
    MessageLoop::current()->Quit();
}

void TestCompletionCallbackBaseInternal::WaitForResult() {
  DCHECK(!waiting_for_result_);
  while (!have_result_) {
    waiting_for_result_ = true;
    MessageLoop::current()->Run();
    waiting_for_result_ = false;
  }
  have_result_ = false;  // Auto-reset for next callback.
}

TestCompletionCallbackBaseInternal::TestCompletionCallbackBaseInternal()
    : have_result_(false),
      waiting_for_result_(false) {
}

}  // namespace internal

TestCompletionCallback::TestCompletionCallback()
    : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
        base::Bind(&TestCompletionCallback::SetResult,
                   base::Unretained(this)))) {
}

TestCompletionCallback::~TestCompletionCallback() {}

TestInt64CompletionCallback::TestInt64CompletionCallback()
    : ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
        base::Bind(&TestInt64CompletionCallback::SetResult,
                   base::Unretained(this)))) {
}

TestInt64CompletionCallback::~TestInt64CompletionCallback() {}

}  // namespace net
