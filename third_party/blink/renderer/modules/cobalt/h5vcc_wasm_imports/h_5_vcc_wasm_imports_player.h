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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_IMPORTS_PLAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_IMPORTS_PLAYER_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class H5vccWasmImports;

// Implements h_5_vcc_wasm_imports_player.idl.
class H5vccWasmImportsPlayer {
  STATIC_ONLY(H5vccWasmImportsPlayer);

 public:
  static String surfaceToken(H5vccWasmImports& imports, uint32_t surface);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_IMPORTS_PLAYER_H_
