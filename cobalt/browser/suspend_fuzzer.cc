// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/suspend_fuzzer.h"

namespace cobalt {
namespace browser {

namespace {

// How long to wait before starting the suspend fuzzer, if it is enabled.  It
// is important that this value is large enough to let the hosted application
// get into its initialized state, since otherwise, we would likely not be
// fuzzing the main state of the application.
const base::TimeDelta kBeginTimeout = base::TimeDelta::FromSeconds(30);

// How long to wait in between suspend fuzzer suspends and resumes, if it is
// enabled.
const base::TimeDelta kInterval = base::TimeDelta::FromSeconds(10);

}  // namespace

#if SB_API_VERSION >= 13
SuspendFuzzer::SuspendFuzzer()
    : thread_("suspend_fuzzer"), step_type_(kShouldRequestFreeze) {
#else
SuspendFuzzer::SuspendFuzzer()
    : thread_("suspend_fuzzer"), step_type_(kShouldRequestSuspend) {
#endif  // SB_API_VERSION >= 13
  thread_.Start();
  thread_.message_loop()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&SuspendFuzzer::DoStep, base::Unretained(this)),
      kBeginTimeout);
}

SuspendFuzzer::~SuspendFuzzer() { thread_.Stop(); }

void SuspendFuzzer::DoStep() {
  DCHECK(base::MessageLoop::current() == thread_.message_loop());
#if SB_API_VERSION >= 13
  if (step_type_ == kShouldRequestFreeze) {
    SB_DLOG(INFO) << "suspend_fuzzer: Requesting freeze.";
    SbSystemRequestFreeze();
    step_type_ = kShouldRequestFocus;
  } else if (step_type_ == kShouldRequestFocus) {
    SB_DLOG(INFO) << "suspend_fuzzer: Requesting focus.";
    SbSystemRequestFocus();
    step_type_ = kShouldRequestFocus;
  } else {
    NOTREACHED();
  }
#else
  if (step_type_ == kShouldRequestSuspend) {
    SB_DLOG(INFO) << "suspend_fuzzer: Requesting suspend.";
    SbSystemRequestSuspend();
    step_type_ = kShouldRequestSuspend;
  } else if (step_type_ == kShouldRequestUnpause) {
    SB_DLOG(INFO) << "suspend_fuzzer: Requesting unpause.";
    SbSystemRequestUnpause();
    step_type_ = kShouldRequestUnpause;
  } else {
    NOTREACHED();
  }
#endif  // SB_API_VERSION >= 13

  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&SuspendFuzzer::DoStep, base::Unretained(this)),
      kInterval);
}

}  // namespace browser
}  // namespace cobalt
