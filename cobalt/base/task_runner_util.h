// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_TAKS_RUNNER_UTIL_H_
#define COBALT_BASE_TAKS_RUNNER_UTIL_H_

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"

namespace base {
namespace task_runner_util {

void WaitForFence(base::SequencedTaskRunner *task_runner,
                  const base::Location &from_here);
void PostBlockingTask(base::SequencedTaskRunner *task_runner,
                      const base::Location &from_here, base::OnceClosure task);

}  // namespace task_runner_util
}  // namespace base

#endif  // COBALT_BASE_TAKS_RUNNER_UTIL_H_
