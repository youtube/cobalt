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

#ifndef WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_
#define WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_

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

}  // namespace util
}  // namespace webdriver
}  // namespace cobalt

#endif  // WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_
