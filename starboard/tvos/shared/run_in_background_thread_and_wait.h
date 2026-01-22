// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_RUN_IN_BACKGROUND_THREAD_AND_WAIT_H_
#define STARBOARD_TVOS_SHARED_RUN_IN_BACKGROUND_THREAD_AND_WAIT_H_

#include <functional>

// Spins the main loop until `function` is invoked and finishes running.
//
// `function` is invoked from a GCD global queue with priority
// QOS_CLASS_USER_INTERACTIVE.
//
// This is useful when performing an operation that would otherwise block the
// main thread and make it not run the main loop (e.g. a call to pthread_join()
// or std::condition_variable::wait()).
void RunInBackgroundThreadAndWait(const std::function<void()>& function);

#endif  // STARBOARD_TVOS_SHARED_RUN_IN_BACKGROUND_THREAD_AND_WAIT_H_
