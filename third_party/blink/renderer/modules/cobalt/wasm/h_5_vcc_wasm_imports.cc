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

#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_imports.h"

#include "cobalt/starboard_wasm/event_queue.h"
#include "cobalt/starboard_wasm/guest_memory.h"
#include "cobalt/starboard_wasm/wasm_player_host.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_player.h"
#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_surface.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8-value.h"
#include "v8/include/v8-wasm.h"

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
  // Only the top-level document controls the platform; embedded content
  // (ads, third-party widgets) must not.
  LocalFrame* frame = window.GetFrame();
  if (!frame || !frame->IsOutermostMainFrame()) {
    return nullptr;
  }
  return FromContext(window.GetExecutionContext());
}

// static
H5vccWasmImports* H5vccWasmImports::h5vccWasmImports(
    DedicatedWorkerGlobalScope& worker) {
  // A worker can't tell which frame created it, so require the top-level
  // frame's origin: that admits workers of the main frame and excludes those
  // of cross-origin iframes. Same-origin iframes can already script the main
  // frame, so admitting their workers grants nothing new.
  const SecurityOrigin* top_origin = worker.top_level_frame_security_origin();
  if (!top_origin ||
      !top_origin->IsSameOriginWith(worker.GetSecurityOrigin())) {
    return nullptr;
  }
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
      events_(base::MakeRefCounted<cobalt::starboard_wasm::EventQueue>(
          context.GetTaskRunner(TaskType::kInternalMedia),
          WTF::BindRepeating(&H5vccWasmImports::DispatchWake,
                             WrapWeakPersistent(this)))),
      host_(std::make_unique<cobalt::starboard_wasm::WasmPlayerHost>(
          events_,
          cobalt::starboard_wasm::WasmPlayerHost::DefaultSbPlayerInterface())),
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
  if (!host_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The context is gone.");
    return;
  }
  v8::Local<v8::Value> value = memory.V8Object();
  if (value.IsEmpty() || !value->IsWasmMemoryObject()) {
    exception_state.ThrowTypeError("bindMemory() expects a WebAssembly.Memory");
    return;
  }
  memory_.Reset(script_state->GetIsolate(), value.As<v8::WasmMemoryObject>());
}

cobalt::starboard_wasm::GuestMemory H5vccWasmImports::Memory(
    v8::Isolate* isolate) const {
  return cobalt::starboard_wasm::GuestMemory(isolate, memory_);
}

EventListener* H5vccWasmImports::onwake() {
  return GetAttributeEventListener(WakeEventType());
}

void H5vccWasmImports::setOnwake(EventListener* listener) {
  SetAttributeEventListener(WakeEventType(), listener);
}

void H5vccWasmImports::ContextDestroyed() {
  host_.reset();
  cobalt::starboard_wasm::WasmPlayerHost::ResetForCurrentThread();
  // Players being destroyed keep the queue alive; stop it calling back here.
  events_->Shutdown();
  events_.reset();
  memory_.Reset();
}

void H5vccWasmImports::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
  visitor->Trace(surface_);
  visitor->Trace(player_);
}

void H5vccWasmImports::DispatchWake() {
  if (!events_ || !GetExecutionContext()) {
    return;
  }
  DispatchEvent(*Event::Create(WakeEventType()));
}

}  // namespace blink
