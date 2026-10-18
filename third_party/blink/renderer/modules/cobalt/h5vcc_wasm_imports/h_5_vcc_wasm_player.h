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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_PLAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_PLAYER_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8-forward.h"

namespace cobalt {
namespace starboard_wasm {
class PlayerModule;
class ProviderCore;
}  // namespace starboard_wasm
}  // namespace cobalt

namespace blink {

class H5vccWasmImports;

// Implements h_5_vcc_wasm_player.idl on cobalt::starboard_wasm::PlayerModule.
class H5vccWasmPlayer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccWasmPlayer(H5vccWasmImports& imports);

  uint32_t create(v8::Isolate* isolate,
                  uint32_t audio_config,
                  uint32_t audio_config_length,
                  uint32_t video_config,
                  uint32_t video_config_length,
                  uint32_t surface,
                  uint32_t flags);
  void destroy(uint32_t player);
  void seek(uint32_t player, double position_us, uint32_t ticket);
  uint32_t writeSamples(v8::Isolate* isolate,
                        uint32_t player,
                        uint32_t track,
                        uint32_t bytes,
                        uint32_t bytes_length,
                        uint32_t infos,
                        uint32_t count,
                        uint32_t config,
                        uint32_t config_length);
  void writeEos(uint32_t player, uint32_t track);
  void setRate(uint32_t player, int32_t rate_milli);
  void setVolume(uint32_t player, int32_t volume_milli);
  uint32_t getInfo(v8::Isolate* isolate,
                   uint32_t player,
                   uint32_t info,
                   uint32_t info_length);
  uint32_t mediaCaps(v8::Isolate* isolate,
                     uint32_t mime,
                     uint32_t mime_length,
                     uint32_t caps,
                     uint32_t caps_length);

  void Trace(Visitor* visitor) const override;

 private:
  cobalt::starboard_wasm::ProviderCore* core() const;
  cobalt::starboard_wasm::PlayerModule* module() const;

  Member<H5vccWasmImports> imports_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_H5VCC_WASM_IMPORTS_H_5_VCC_WASM_PLAYER_H_
