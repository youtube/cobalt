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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_surface.h"

#include "cobalt/starboard_wasm/core/provider_core.h"
#include "cobalt/starboard_wasm/player/player_module.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_imports.h"

namespace blink {

using cobalt::starboard_wasm::PlayerModule;

H5vccWasmSurface::H5vccWasmSurface(H5vccWasmImports& imports)
    : imports_(&imports) {}

uint32_t H5vccWasmSurface::create(uint32_t flags) {
  PlayerModule* player = module();
  return player ? player->SurfaceCreate(flags) : 0;
}

void H5vccWasmSurface::destroy(uint32_t surface) {
  if (PlayerModule* player = module()) {
    player->SurfaceDestroy(surface);
  }
}

void H5vccWasmSurface::setBounds(uint32_t surface,
                                 int32_t x,
                                 int32_t y,
                                 int32_t width,
                                 int32_t height,
                                 int32_t z_index) {
  if (PlayerModule* player = module()) {
    player->SurfaceSetBounds(surface, x, y, width, height, z_index);
  }
}

String H5vccWasmSurface::token(uint32_t surface) {
  PlayerModule* player = module();
  return player ? String::FromUTF8(player->SurfaceToken(surface)) : String();
}

void H5vccWasmSurface::Trace(Visitor* visitor) const {
  visitor->Trace(imports_);
  ScriptWrappable::Trace(visitor);
}

PlayerModule* H5vccWasmSurface::module() const {
  cobalt::starboard_wasm::ProviderCore* core = imports_->core();
  return core ? core->GetModule<PlayerModule>() : nullptr;
}

}  // namespace blink
