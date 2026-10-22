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

#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_surface.h"

#include "cobalt/starboard_wasm/wasm_player_host.h"
#include "third_party/blink/renderer/modules/cobalt/wasm/h_5_vcc_wasm_imports.h"

namespace blink {

using cobalt::starboard_wasm::WasmPlayerHost;

H5vccWasmSurface::H5vccWasmSurface(H5vccWasmImports& imports)
    : imports_(&imports) {}

uint32_t H5vccWasmSurface::create() {
  WasmPlayerHost* host = imports_->host();
  return host ? host->CreateSurface() : 0;
}

void H5vccWasmSurface::destroy(uint32_t surface) {
  if (WasmPlayerHost* host = imports_->host()) {
    host->DestroySurface(surface);
  }
}

String H5vccWasmSurface::token(uint32_t surface) {
  WasmPlayerHost* host = imports_->host();
  return host ? String::FromUTF8(host->SurfaceToken(surface)) : String();
}

void H5vccWasmSurface::Trace(Visitor* visitor) const {
  visitor->Trace(imports_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
