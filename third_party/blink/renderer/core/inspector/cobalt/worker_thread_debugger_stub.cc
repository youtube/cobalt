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

#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"

namespace blink {

WorkerThreadDebugger::WorkerThreadDebugger(v8::Isolate* isolate)
    : ThreadDebuggerCommonImpl(isolate) {}
WorkerThreadDebugger::~WorkerThreadDebugger() = default;

WorkerThreadDebugger* WorkerThreadDebugger::From(v8::Isolate*) {
  return nullptr;
}
void WorkerThreadDebugger::ExceptionThrown(WorkerThread*, ErrorEvent*) {}
void WorkerThreadDebugger::WorkerThreadCreated(WorkerThread*) {}
void WorkerThreadDebugger::WorkerThreadDestroyed(WorkerThread*) {}
void WorkerThreadDebugger::ContextCreated(WorkerThread*,
                                          const KURL&,
                                          v8::Local<v8::Context>) {}
void WorkerThreadDebugger::ContextWillBeDestroyed(WorkerThread*,
                                                  v8::Local<v8::Context>) {}
int WorkerThreadDebugger::ContextGroupId(ExecutionContext*) {
  return 0;
}
void WorkerThreadDebugger::ReportConsoleMessage(ExecutionContext*,
                                                mojom::ConsoleMessageSource,
                                                mojom::ConsoleMessageLevel,
                                                const WTF::String&,
                                                SourceLocation*) {}
void WorkerThreadDebugger::runMessageLoopOnPause(int) {}
void WorkerThreadDebugger::quitMessageLoopOnPause() {}
void WorkerThreadDebugger::muteMetrics(int) {}
void WorkerThreadDebugger::unmuteMetrics(int) {}
v8::Local<v8::Context> WorkerThreadDebugger::ensureDefaultContextInGroup(int) {
  return {};
}
void WorkerThreadDebugger::beginEnsureAllContextsInGroup(int) {}
void WorkerThreadDebugger::endEnsureAllContextsInGroup(int) {}
bool WorkerThreadDebugger::canExecuteScripts(int) {
  return true;
}
void WorkerThreadDebugger::runIfWaitingForDebugger(int) {}
v8::MaybeLocal<v8::Value> WorkerThreadDebugger::memoryInfo(
    v8::Isolate*,
    v8::Local<v8::Context>) {
  return {};
}
void WorkerThreadDebugger::consoleAPIMessage(int,
                                             v8::Isolate::MessageErrorLevel,
                                             const v8_inspector::StringView&,
                                             const v8_inspector::StringView&,
                                             unsigned,
                                             unsigned,
                                             v8_inspector::V8StackTrace*) {}
void WorkerThreadDebugger::consoleClear(int) {}

}  // namespace blink
