// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CancelableCallback is a wrapper around base::Callback that allows
// cancellation of a callback. CancelableCallback takes a reference on the
// wrapped callback until this object is destroyed or Reset()/Cancel() are
// called.
//
// Thread-safety notes:
//
// CancelableCallback objects must be created on, posted to, cancelled on, and
// destroyed on the same thread.
//
//
// EXAMPLE USAGE:
//
// In the following example, the test is verifying that RunIntensiveTest()
// Quit()s the message loop within 4 seconds. The cancelable callback is posted
// to the message loop, the intensive test runs, the message loop is run,
// then the callback is cancelled.
//
// void TimeoutCallback(const std::string& timeout_message) {
//   FAIL() << timeout_message;
//   MessageLoop::current()->Quit();
// }
//
// CancelableCallback timeout(base::Bind(TimeoutCallback));
// MessageLoop::current()->PostDelayedTask(FROM_HERE, timeout.callback(),
//                                         4000)  // 4 seconds to run.
// RunIntensiveTest();
// MessageLoop::current()->Run();
// timeout.Cancel();  // Hopefully this is hit before the timeout callback runs.
//

#ifndef BASE_CANCELABLE_CALLBACK_H_
#define BASE_CANCELABLE_CALLBACK_H_
#pragma once

#include "base/callback.h"
#include "base/base_export.h"
#include "base/memory/weak_ptr.h"

namespace base {

class BASE_EXPORT CancelableCallback {
 public:
  CancelableCallback();

  // |callback| must not be null.
  explicit CancelableCallback(const base::Closure& callback);

  ~CancelableCallback();

  // Cancels and drops the reference to the wrapped callback.
  void Cancel();

  // Returns true if the wrapped callback has been cancelled.
  bool IsCancelled() const;

  // Sets |callback| as the closure that may be cancelled. |callback| may not
  // be null. Outstanding and any previously wrapped callbacks are cancelled.
  void Reset(const base::Closure& callback);

  // Returns a callback that can be disabled by calling Cancel().
  const base::Closure& callback() const;

 private:
  void RunCallback();

  // Helper method to bind |forwarder_| using a weak pointer from
  // |weak_factory_|.
  void InitializeForwarder();

  // Used to ensure RunCallback() is not run when this object is destroyed.
  base::WeakPtrFactory<CancelableCallback> weak_factory_;

  // The wrapper closure.
  base::Closure forwarder_;

  // The stored closure that may be cancelled.
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(CancelableCallback);
};

}  // namespace base

#endif  // BASE_CANCELABLE_CALLBACK_H_
