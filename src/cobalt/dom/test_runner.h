// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_TEST_RUNNER_H_
#define COBALT_DOM_TEST_RUNNER_H_

#if defined(ENABLE_TEST_RUNNER)

#include "base/callback.h"
#include "cobalt/base/clock.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// TestRunner is a utility class for JavaScript to communicate with
// Cobalt-driver applications (eg. layout tests). The name of TestRunner is
// analogous to the object by the same name inside of WebKit.
class TestRunner : public script::Wrappable {
 public:
  typedef script::CallbackFunction<void()> TestRunnerCallback;
  typedef script::ScriptValue<TestRunnerCallback> TestRunnerCallbackArg;

  TestRunner();

  // These methods are used in applications that wait for the onload event to
  // which they react by taking a measurement. The function |WaitUntilDone| can
  // be called to indicate that the system should not take a measurement at the
  // onload event, and instead wait for a call to |NotifyDone| to take the
  // measurement. (eg. CSS background-image and animation related tests use
  // |WaitUntilDone| to block the snapshot taken when the onload event
  // triggered, and wait for a |NotifyDone| call to take the snapshot.)
  void NotifyDone();
  void WaitUntilDone();

  // Trigger a layout that is not measured. Can only be called after
  // |WaitUntilDone|. This method can be used to setup tests for partial
  // layouts.
  void DoNonMeasuredLayout();

  // Increment's the web page's clock by the specified number of milliseconds.
  // This will result in the advance of animations.
  void AdvanceClockByMs(uint64 amount);

  void CollectGarbageAndThenDo(script::EnvironmentSettings* settings,
                               const TestRunnerCallbackArg& callback_arg);

  // Returns the clock controlled by test runner.
  scoped_refptr<base::Clock> GetClock();

  // When this callback is called, a layout should be triggered. Not all
  // triggered layouts should be measured, and the callback function should call
  // should_wait() to ensure that measurements are only taken when should_wait()
  // returns false.
  void set_trigger_layout_callback(
      const base::Closure& trigger_layout_callback) {
    trigger_layout_callback_ = trigger_layout_callback;
  }

  bool should_wait() const { return should_wait_; }

  DEFINE_WRAPPABLE_TYPE(TestRunner);

 private:
  ~TestRunner() {}

  bool should_wait_;
  base::Closure trigger_layout_callback_;
  scoped_refptr<base::ManualAdvanceClock> clock_;

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

}  // namespace dom
}  // namespace cobalt

#endif  // ENABLE_TEST_RUNNER

#endif  // COBALT_DOM_TEST_RUNNER_H_
