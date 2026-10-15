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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_imports_player.h"

#include "cobalt/starboard_wasm/core/provider_core.h"
#include "cobalt/starboard_wasm/player/player_module.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_imports.h"

namespace blink {

// static
String H5vccWasmImportsPlayer::surfaceToken(H5vccWasmImports& imports,
                                            uint32_t surface) {
  using cobalt::starboard_wasm::PlayerModule;
  if (!imports.core()) {
    return String();
  }
  auto* player = static_cast<PlayerModule*>(
      imports.core()->GetModule(PlayerModule::kName));
  return player ? String::FromUTF8(player->SurfaceToken(surface)) : String();
}

}  // namespace blink
