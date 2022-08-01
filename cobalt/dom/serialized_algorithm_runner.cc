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

#include "cobalt/dom/serialized_algorithm_runner.h"

#include "base/message_loop/message_loop.h"

namespace cobalt {
namespace dom {

void DefaultAlgorithmRunner::Start(const scoped_refptr<HandleBase>& handle) {
  DCHECK(handle);
  TRACE_EVENT0("cobalt::dom", "DefaultAlgorithmRunner::Start()");

  if (asynchronous_reduction_enabled_) {
    Process(handle);
    return;
  }

  auto task_runner = base::MessageLoop::current()->task_runner();
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DefaultAlgorithmRunner::Process,
                                       base::Unretained(this), handle));
}

void DefaultAlgorithmRunner::Process(const scoped_refptr<HandleBase>& handle) {
  DCHECK(handle);
  TRACE_EVENT0("cobalt::dom", "DefaultAlgorithmRunner::Process()");

  auto task_runner = base::MessageLoop::current()->task_runner();

  bool finished = false;
  handle->Process(&finished);

  if (finished) {
    handle->Finalize();
    return;
  }
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DefaultAlgorithmRunner::Process,
                                       base::Unretained(this), handle));
}

OffloadAlgorithmRunner::OffloadAlgorithmRunner(
    const scoped_refptr<TaskRunner>& process_task_runner,
    const scoped_refptr<TaskRunner>& finalize_task_runner)
    : process_task_runner_(process_task_runner),
      finalize_task_runner_(finalize_task_runner) {
  DCHECK(process_task_runner_);
  DCHECK(finalize_task_runner_);
  DCHECK_NE(process_task_runner_, finalize_task_runner_);
}

void OffloadAlgorithmRunner::Start(const scoped_refptr<HandleBase>& handle) {
  DCHECK(handle);

  TRACE_EVENT0("cobalt::dom", "OffloadAlgorithmRunner::Start()");

  process_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OffloadAlgorithmRunner::Process,
                                base::Unretained(this), handle));
}

void OffloadAlgorithmRunner::Process(const scoped_refptr<HandleBase>& handle) {
  DCHECK(handle);
  DCHECK(process_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT0("cobalt::dom", "OffloadAlgorithmRunner::Process()");

  bool finished = false;
  handle->Process(&finished);

  if (finished) {
    finalize_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HandleBase::Finalize, handle));
    return;
  }
  process_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&OffloadAlgorithmRunner::Process,
                                base::Unretained(this), handle));
}

}  // namespace dom
}  // namespace cobalt
