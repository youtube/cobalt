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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_SURFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_SURFACE_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace cobalt {
namespace starboard_wasm {
class PlayerModule;
}  // namespace starboard_wasm
}  // namespace cobalt

namespace blink {

class H5vccWasmImports;

// Implements h_5_vcc_wasm_surface.idl on cobalt::starboard_wasm::PlayerModule.
class H5vccWasmSurface final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccWasmSurface(H5vccWasmImports& imports);

  uint32_t create(uint32_t flags);
  void destroy(uint32_t surface);
  void setBounds(uint32_t surface,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height,
                 int32_t z_index);
  String token(uint32_t surface);

  void Trace(Visitor* visitor) const override;

 private:
  cobalt::starboard_wasm::PlayerModule* module() const;

  Member<H5vccWasmImports> imports_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_SURFACE_H_
