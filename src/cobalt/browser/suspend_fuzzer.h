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

#ifndef COBALT_BROWSER_SUSPEND_FUZZER_H_
#define COBALT_BROWSER_SUSPEND_FUZZER_H_

#include "base/bind.h"
#include "base/threading/thread.h"

namespace cobalt {
namespace browser {

// Repeatedly switch off between calling |SbSystemRequestSuspend| and
// |SbSystemRequestUnpause|.
class SuspendFuzzer {
 public:
  SuspendFuzzer();
  ~SuspendFuzzer();

 private:
  void DoStep();

  // Suspending the application could possibly freeze our message loop or our
  // web modules' message loops.  We thus create a separate thread to run the
  // suspend fuzzer on.
  base::Thread thread_;

  enum StepType {
#if SB_API_VERSION >= 13
    kShouldRequestFreeze,
    kShouldRequestFocus,
#else
    kShouldRequestSuspend,
    kShouldRequestUnpause,
#endif  // SB_API_VERSION >= 13
  } step_type_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SUSPEND_FUZZER_H_
