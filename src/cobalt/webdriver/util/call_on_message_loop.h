/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_
#define COBALT_WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"

namespace cobalt {
namespace webdriver {
namespace util {
namespace internal {
template <class ReturnValue>
class CallOnMessageLoopHelper {
 public:
  typedef base::Callback<ReturnValue(void)> CallbackType;
  CallOnMessageLoopHelper(
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      const CallbackType& callback)
      : completed_event_(false, false) {
    DCHECK_NE(base::MessageLoopProxy::current(), message_loop_proxy);
    message_loop_proxy->PostTask(
        FROM_HERE, base::Bind(&CallOnMessageLoopHelper::CallAndSignal,
                              base::Unretained(this), callback));
  }

  ReturnValue WaitForResult() {
    completed_event_.Wait();
    return result_;
  }

 private:
  void CallAndSignal(const CallbackType& callback) {
    result_ = callback.Run();
    completed_event_.Signal();
  }

  base::WaitableEvent completed_event_;
  ReturnValue result_;
};

// Used with CallWeakOnMessageLoop.
template <typename T, typename ReturnValue>
base::optional<ReturnValue> RunWeak(const base::Callback<T*()>& get_weak,
                                    const base::Callback<ReturnValue(T*)>& cb) {
  T* weak_object = get_weak.Run();
  if (weak_object) {
    return cb.Run(weak_object);
  } else {
    return base::nullopt;
  }
}
}  // namespace internal

// Call the base::Callback on the specified message loop and wait for it to
// complete, then return the result of the callback.
template <class ReturnValue>
ReturnValue CallOnMessageLoop(
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    const base::Callback<ReturnValue(void)>& callback) {
  internal::CallOnMessageLoopHelper<ReturnValue> call_helper(message_loop_proxy,
                                                             callback);
  return call_helper.WaitForResult();
}

// Supports a common pattern in the various XXXDriver classes.
// On the provided message loop, calls RunWeak which will run the callback |cb|
// if |get_weak| returns a non-NULL pointer. RunWeak will return the result of
// the callback, or base::nullopt if |get_weak| returned NULL and the callback
// wasn't run.
// If the return value from RunWeak is valid, return a CommandResult that wraps
// the value. Otherwise return |no_such_object_code| to indicate the correct
// error.
template <typename T, typename ReturnValue>
util::CommandResult<ReturnValue> CallWeakOnMessageLoopAndReturnResult(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const base::Callback<T*()>& get_weak,
    const base::Callback<ReturnValue(T*)>& cb,
    protocol::Response::StatusCode no_such_object_code) {
  typedef util::CommandResult<ReturnValue> CommandResult;
  typedef base::optional<ReturnValue> InternalResult;
  InternalResult result = util::CallOnMessageLoop(
      message_loop,
      base::Bind(&internal::RunWeak<T, ReturnValue>, get_weak, cb));
  if (result) {
    return CommandResult(result.value());
  } else {
    return CommandResult(no_such_object_code);
  }
}

}  // namespace util
}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_
