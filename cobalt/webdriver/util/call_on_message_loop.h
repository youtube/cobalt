// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_
#define COBALT_WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
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
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const CallbackType& callback)
      : completed_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED),
        success_(false) {
    DCHECK_NE(base::MessageLoop::current()->task_runner(), task_runner);
    std::unique_ptr<DeletionSignaler> dt(
        new DeletionSignaler(&completed_event_));
    // Note that while base::MessageLoopProxy::PostTask returns false
    // after the message loop has gone away, it still can return true
    // even if tasks are posted during shutdown and will never be run,
    // so we ignore this return value.
    task_runner->PostTask(FROM_HERE, base::Bind(&CallOnMessageLoopHelper::Call,
                                                base::Unretained(this),
                                                callback, base::Passed(&dt)));
  }

  // Waits for result, filling |out| with the return value if successful.
  // Returns true on success or false if the message loop went away
  // before the task was executed.
  bool WaitForResult(ReturnValue* out) {
    completed_event_.Wait();
    if (success_) {
      *out = result_;
    }
    return success_;
  }

  ~CallOnMessageLoopHelper() {
    // We must ensure that we've waited for completion otherwise
    // DeletionSignaler will have a use-after-free.
    completed_event_.Wait();
  }

 private:
  // DeletionSignaler signals an event when the destructor is called.
  // This allows us to use the base::Passed mechanism to signal our
  // completed_event_ both when Call() has been invoked and when
  // the message loop has been deleted.
  class DeletionSignaler {
   public:
    base::WaitableEvent* to_signal_;

    explicit DeletionSignaler(base::WaitableEvent* to_signal)
        : to_signal_(to_signal) {}

    ~DeletionSignaler() { to_signal_->Signal(); }

   private:
    DISALLOW_COPY_AND_ASSIGN(DeletionSignaler);
  };

  void Call(const CallbackType& callback,
            std::unique_ptr<DeletionSignaler> dt ALLOW_UNUSED_TYPE) {
    result_ = callback.Run();
    success_ = true;
  }

  base::WaitableEvent completed_event_;
  ReturnValue result_;
  bool success_;
};

// Used with CallWeakOnbase::MessageLoop.
template <typename T, typename ReturnValue>
base::Optional<ReturnValue> RunWeak(const base::Callback<T*()>& get_weak,
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
// complete. Returns true if successful, or false if the underlying
// PostTask failed. This can happen if a WebModule shuts down due to a page
// navigation.
//
// On success, |out| is set to the result.
template <class ReturnValue>
bool TryCallOnMessageLoop(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Callback<ReturnValue(void)>& callback, ReturnValue* out) {
  internal::CallOnMessageLoopHelper<ReturnValue> call_helper(task_runner,
                                                             callback);
  return call_helper.WaitForResult(out);
}

// Tries to call |callback| on base::MessageLoop |task_runner|,
// but returns a CommandResult of |window_disappeared_code| if the
// message loop has shut down. This can happen if a WebModule shuts
// down due to a page navigation.
template <typename ReturnValue>
util::CommandResult<ReturnValue> CallOnMessageLoop(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Callback<util::CommandResult<ReturnValue>(void)>& callback,
    protocol::Response::StatusCode window_disappeared_code) {
  util::CommandResult<ReturnValue> result;
  bool success = TryCallOnMessageLoop(task_runner, callback, &result);

  if (!success) {
    result =
        util::CommandResult<ReturnValue>(window_disappeared_code, "", true);
  }
  return result;
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
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Callback<T*()>& get_weak,
    const base::Callback<ReturnValue(T*)>& cb,
    protocol::Response::StatusCode no_such_object_code) {
  typedef util::CommandResult<ReturnValue> CommandResult;
  typedef base::Optional<ReturnValue> InternalResult;
  InternalResult result;
  bool success = util::TryCallOnMessageLoop(
      task_runner, base::Bind(&internal::RunWeak<T, ReturnValue>, get_weak, cb),
      &result);
  if (success && result) {
    return CommandResult(result.value());
  } else {
    return CommandResult(no_such_object_code);
  }
}

}  // namespace util
}  // namespace webdriver
}  // namespace cobalt

#endif  // COBALT_WEBDRIVER_UTIL_CALL_ON_MESSAGE_LOOP_H_
