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

#include "third_party/blink/renderer/core/inspector/devtools_agent.h"

#include <memory>

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"

namespace blink {

// Gold builds compile with `enable_devtools_backend = false`, so there is never
// a DevToolsAgent to attach to a new worker. This must still return a valid
// object: callers such as ThreadedMessagingProxyBase::InitializeWorkerThread
// pass the result straight to WorkerThread::Start(), which unconditionally
// reads `devtools_params->devtools_worker_token`. Returning null here segfaults
// the renderer on every `new Worker(...)`. See b/564844345.
//
// This mirrors the early-exit path of the real implementation in
// devtools_agent.cc, which likewise returns a params object with only the
// worker token populated when no agent is attached
std::unique_ptr<WorkerDevToolsParams> DevToolsAgent::WorkerThreadCreated(
    ExecutionContext*,
    WorkerThread*,
    const KURL&,
    const String&,
    const std::optional<const DedicatedWorkerToken>& token) {
  auto result = std::make_unique<WorkerDevToolsParams>();
  result->devtools_worker_token = token.has_value()
                                      ? token.value().value()
                                      : base::UnguessableToken::Create();
  result->wait_for_debugger = false;
  return result;
}
void DevToolsAgent::WorkerThreadTerminated(ExecutionContext*, WorkerThread*) {}

}  // namespace blink
