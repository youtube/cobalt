// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/task/task_runner.h"
#include "base/threading/thread.h"
#include "cobalt/base/wrap_main.h"

namespace cobalt {
namespace demos {

class SimpleSandbox {
 public:
  static SimpleSandbox* GetInstance() {
    return base::Singleton<SimpleSandbox>::get();
  }

  static void StartApplication(int argc, char** argv, const char* link,
                               const base::Closure& quit_closure,
                               SbTimeMonotonic timestamp);

  static void StopApplication();

  base::Thread* thread() { return &thread_; }

 private:
  base::Thread thread_{"SimpleSandboxThread"};
  // friend struct base::DefaultSingletonTraits<SimpleSandbox>;
};

// static
void SimpleSandbox::StartApplication(int argc, char** argv, const char* link,
                                     const base::Closure& quit_closure,
                                     SbTimeMonotonic timestamp) {
  LOG(INFO) << "SimpleSandbox::StartApplication";

  SimpleSandbox::GetInstance()->thread()->Start();

  SimpleSandbox::GetInstance()
      ->thread()
      ->message_loop()
      ->task_runner()
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(
                            [](base::MessageLoop* main_loop,
                               const base::Closure& quit_closure) {
                              LOG(INFO) << "Lambda Function in the thread.";

                              // Stop after seconds.
                              main_loop->task_runner()->PostDelayedTask(
                                  FROM_HERE, quit_closure,
                                  base::TimeDelta::FromSeconds(2));
                            },
                            base::MessageLoop::current(), quit_closure),
                        base::TimeDelta(base::TimeDelta::FromSeconds(1)));
}

// static
void SimpleSandbox::StopApplication() {
  LOG(INFO) << "SimpleSandbox::StopApplication";
}

}  // namespace demos
}  // namespace cobalt

COBALT_WRAP_BASE_MAIN(cobalt::demos::SimpleSandbox::StartApplication,
                      cobalt::demos::SimpleSandbox::StopApplication);
