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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_IMPORTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_IMPORTS_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace cobalt {
namespace starboard_wasm {
class ProviderCore;
}  // namespace starboard_wasm
}  // namespace cobalt

namespace blink {

class DedicatedWorkerGlobalScope;
class ExceptionState;
class LocalDOMWindow;
class ScriptState;

class MODULES_EXPORT H5vccWasmImports final
    : public EventTarget,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static H5vccWasmImports* h5vccWasmImports(LocalDOMWindow& window);
  static H5vccWasmImports* h5vccWasmImports(DedicatedWorkerGlobalScope& worker);

  explicit H5vccWasmImports(ExecutionContext& context);
  H5vccWasmImports(const H5vccWasmImports&) = delete;
  H5vccWasmImports& operator=(const H5vccWasmImports&) = delete;
  ~H5vccWasmImports() override;

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  ScriptObject importObject(ScriptState* script_state,
                            ExceptionState& exception_state);
  void bindMemory(ScriptState* script_state,
                  const ScriptObject& memory,
                  ExceptionState& exception_state);
  EventListener* onwake();
  void setOnwake(EventListener* listener);
  double nowUs();

  // Null once the context is destroyed.
  cobalt::starboard_wasm::ProviderCore* core() const { return core_.get(); }

  void ContextDestroyed() override;
  void Trace(Visitor* visitor) const override;

 private:
  static H5vccWasmImports* FromContext(ExecutionContext* context);
  void DispatchWake();

  std::unique_ptr<cobalt::starboard_wasm::ProviderCore> core_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_IMPORTS_H_
