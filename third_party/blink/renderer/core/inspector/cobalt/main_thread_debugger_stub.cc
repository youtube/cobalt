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

#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"

namespace blink {

MainThreadDebugger::MainThreadDebugger(v8::Isolate* isolate)
    : ThreadDebuggerCommonImpl(isolate) {}
MainThreadDebugger::~MainThreadDebugger() = default;

MainThreadDebugger* MainThreadDebugger::Instance(v8::Isolate*) {
  return nullptr;
}
void MainThreadDebugger::ContextWillBeDestroyed(ScriptState*) {}
void MainThreadDebugger::ContextCreated(ScriptState*,
                                        LocalFrame*,
                                        const SecurityOrigin*) {}
void MainThreadDebugger::runMessageLoopOnPause(int) {}
void MainThreadDebugger::runMessageLoopOnInstrumentationPause(int) {}
void MainThreadDebugger::quitMessageLoopOnPause() {}
void MainThreadDebugger::runIfWaitingForDebugger(int) {}
void MainThreadDebugger::muteMetrics(int) {}
void MainThreadDebugger::unmuteMetrics(int) {}
v8::Local<v8::Context> MainThreadDebugger::ensureDefaultContextInGroup(int) {
  return {};
}
void MainThreadDebugger::beginEnsureAllContextsInGroup(int) {}
void MainThreadDebugger::endEnsureAllContextsInGroup(int) {}
void MainThreadDebugger::installAdditionalCommandLineAPI(
    v8::Local<v8::Context>,
    v8::Local<v8::Object>) {}
void MainThreadDebugger::consoleAPIMessage(int,
                                           v8::Isolate::MessageErrorLevel,
                                           const v8_inspector::StringView&,
                                           const v8_inspector::StringView&,
                                           unsigned,
                                           unsigned,
                                           v8_inspector::V8StackTrace*) {}
v8::MaybeLocal<v8::Value> MainThreadDebugger::memoryInfo(
    v8::Isolate*,
    v8::Local<v8::Context>) {
  return {};
}
void MainThreadDebugger::consoleClear(int) {}
bool MainThreadDebugger::canExecuteScripts(int) {
  return true;
}
int MainThreadDebugger::ContextGroupId(ExecutionContext*) {
  return 0;
}
int MainThreadDebugger::ContextGroupId(LocalFrame*) {
  return 0;
}
void MainThreadDebugger::ReportConsoleMessage(ExecutionContext*,
                                              mojom::ConsoleMessageSource,
                                              mojom::ConsoleMessageLevel,
                                              const WTF::String&,
                                              SourceLocation*) {}
void MainThreadDebugger::DidClearContextsForFrame(LocalFrame*) {}
void MainThreadDebugger::ExceptionThrown(ExecutionContext*, ErrorEvent*) {}

}  // namespace blink
