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

#if defined(ENABLE_TEST_RUNNER)

#include "cobalt/dom/test_runner.h"

#include "base/message_loop.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/javascript_engine.h"

namespace cobalt {
namespace dom {

TestRunner::TestRunner() :
    should_wait_(false), clock_(new base::ManualAdvanceClock()) {}

void TestRunner::NotifyDone() {
  if (should_wait_) {
    should_wait_ = false;
    if (!trigger_layout_callback_.is_null()) {
      trigger_layout_callback_.Run();
    }
    // Reset |should_wait_| to prevent a second layout if the document onLoad
    // event occurs after the layout trigger.
    should_wait_ = true;
  }
}

void TestRunner::WaitUntilDone() { should_wait_ = true; }

void TestRunner::DoNonMeasuredLayout() {
  if (should_wait_ && !trigger_layout_callback_.is_null()) {
    trigger_layout_callback_.Run();
  }
}

void TestRunner::AdvanceClockByMs(uint64 amount) {
  clock_->Advance(base::TimeDelta::FromMilliseconds(amount));
}

void TestRunner::CollectGarbageAndThenDo(
    script::EnvironmentSettings* settings,
    const TestRunnerCallbackArg& callback_arg) {
  DCHECK(settings);
  TestRunnerCallbackArg::Reference reference(this, callback_arg);
  DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  dom_settings->javascript_engine()->CollectGarbage();
  reference.value().Run();
}

scoped_refptr<base::Clock> TestRunner::GetClock() {
  return clock_;
}

}  // namespace dom
}  // namespace cobalt

#endif  // ENABLE_TEST_RUNNER
