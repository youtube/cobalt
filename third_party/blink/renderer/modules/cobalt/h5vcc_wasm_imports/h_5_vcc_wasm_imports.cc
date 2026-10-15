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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_imports.h"

#include <string>

#include "cobalt/starboard_wasm/all_modules.h"
#include "cobalt/starboard_wasm/core/provider_core.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_player.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_surface.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_system.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// Event names are per-thread AtomicStrings because this runs on workers as
// well as the main window thread.
AtomicString WakeEventType() {
  return AtomicString("wake");
}

}  // namespace

// static
const char H5vccWasmImports::kSupplementName[] = "H5vccWasmImports";

// static
H5vccWasmImports* H5vccWasmImports::h5vccWasmImports(LocalDOMWindow& window) {
  return FromContext(window.GetExecutionContext());
}

// static
H5vccWasmImports* H5vccWasmImports::h5vccWasmImports(
    DedicatedWorkerGlobalScope& worker) {
  return FromContext(worker.GetExecutionContext());
}

// static
H5vccWasmImports* H5vccWasmImports::FromContext(ExecutionContext* context) {
  if (!context || context->IsContextDestroyed()) {
    return nullptr;
  }
  H5vccWasmImports* provider =
      Supplement<ExecutionContext>::From<H5vccWasmImports>(*context);
  if (!provider) {
    provider = MakeGarbageCollected<H5vccWasmImports>(*context);
    ProvideTo(*context, provider);
  }
  return provider;
}

H5vccWasmImports::H5vccWasmImports(ExecutionContext& context)
    : Supplement<ExecutionContext>(context),
      ExecutionContextLifecycleObserver(&context),
      core_(std::make_unique<cobalt::starboard_wasm::ProviderCore>(
          context.GetTaskRunner(TaskType::kInternalMedia),
          WTF::BindRepeating(&H5vccWasmImports::DispatchWake,
                             WrapWeakPersistent(this)),
          cobalt::starboard_wasm::AllModules())),
      system_(MakeGarbageCollected<H5vccWasmSystem>(*this)),
      surface_(MakeGarbageCollected<H5vccWasmSurface>(*this)),
      player_(MakeGarbageCollected<H5vccWasmPlayer>(*this)) {}

H5vccWasmImports::~H5vccWasmImports() = default;

const AtomicString& H5vccWasmImports::InterfaceName() const {
  return event_target_names::kH5VccWasmImports;
}

ExecutionContext* H5vccWasmImports::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void H5vccWasmImports::bindMemory(ScriptState* script_state,
                                  const ScriptObject& memory,
                                  ExceptionState& exception_state) {
  if (!core_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The context is gone.");
    return;
  }
  std::string error;
  if (!core_->BindMemory(script_state->GetIsolate(), memory.V8Object(),
                         &error)) {
    exception_state.ThrowTypeError(String::FromUTF8(error));
  }
}

EventListener* H5vccWasmImports::onwake() {
  return GetAttributeEventListener(WakeEventType());
}

void H5vccWasmImports::setOnwake(EventListener* listener) {
  SetAttributeEventListener(WakeEventType(), listener);
}

void H5vccWasmImports::ContextDestroyed() {
  if (core_) {
    core_->Shutdown();
    core_.reset();
  }
}

void H5vccWasmImports::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
  visitor->Trace(system_);
  visitor->Trace(surface_);
  visitor->Trace(player_);
}

void H5vccWasmImports::DispatchWake() {
  if (!core_ || !GetExecutionContext()) {
    return;
  }
  DispatchEvent(*Event::Create(WakeEventType()));
}

}  // namespace blink
