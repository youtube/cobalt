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

#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"



namespace blink {

// WorkerInspectorController stubs
WorkerInspectorController* WorkerInspectorController::Create(
    WorkerThread*, const KURL&, scoped_refptr<InspectorTaskRunner>,
    std::unique_ptr<WorkerDevToolsParams>) {
  return nullptr;
}
void WorkerInspectorController::WaitForDebuggerIfNeeded() {}
void WorkerInspectorController::Dispose() {}
void WorkerInspectorController::Trace(Visitor*) const {}

} // namespace blink
