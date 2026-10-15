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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_SYSTEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_SYSTEM_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8-forward.h"

namespace blink {

class H5vccWasmImports;

// Implements h_5_vcc_wasm_system.idl on cobalt::starboard_wasm::SystemModule.
class H5vccWasmSystem final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccWasmSystem(H5vccWasmImports& imports);

  double nowUs();
  uint32_t pollEvents(v8::Isolate* isolate, uint32_t events, uint32_t count);
  void log(v8::Isolate* isolate,
           uint32_t level,
           uint32_t message,
           uint32_t message_length);

  void Trace(Visitor* visitor) const override;

 private:
  Member<H5vccWasmImports> imports_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_SYSTEM_H_
