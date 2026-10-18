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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_player.h"

#include "cobalt/starboard_wasm/core/provider_core.h"
#include "cobalt/starboard_wasm/player/player_module.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_wasm_imports/h_5_vcc_wasm_imports.h"

namespace blink {

using cobalt::starboard_wasm::GuestMemory;
using cobalt::starboard_wasm::PlayerModule;
using cobalt::starboard_wasm::ProviderCore;

H5vccWasmPlayer::H5vccWasmPlayer(H5vccWasmImports& imports)
    : imports_(&imports) {}

uint32_t H5vccWasmPlayer::create(v8::Isolate* isolate,
                                 uint32_t audio_config,
                                 uint32_t audio_config_length,
                                 uint32_t video_config,
                                 uint32_t video_config_length,
                                 uint32_t surface,
                                 uint32_t flags) {
  PlayerModule* player = module();
  if (!player) {
    return 0;
  }
  GuestMemory memory = core()->Memory(isolate);
  return player->Create(memory, audio_config, audio_config_length, video_config,
                        video_config_length, surface, flags);
}

void H5vccWasmPlayer::destroy(uint32_t player) {
  if (PlayerModule* module = this->module()) {
    module->Destroy(player);
  }
}

void H5vccWasmPlayer::seek(uint32_t player,
                           double position_us,
                           uint32_t ticket) {
  if (PlayerModule* module = this->module()) {
    module->Seek(player, position_us, ticket);
  }
}

uint32_t H5vccWasmPlayer::writeSamples(v8::Isolate* isolate,
                                       uint32_t player,
                                       uint32_t track,
                                       uint32_t bytes,
                                       uint32_t bytes_length,
                                       uint32_t infos,
                                       uint32_t count,
                                       uint32_t config,
                                       uint32_t config_length) {
  PlayerModule* module = this->module();
  if (!module) {
    return 0;
  }
  GuestMemory memory = core()->Memory(isolate);
  return module->WriteSamples(memory, player, track, bytes, bytes_length, infos,
                              count, config, config_length);
}

void H5vccWasmPlayer::writeEos(uint32_t player, uint32_t track) {
  if (PlayerModule* module = this->module()) {
    module->WriteEos(player, track);
  }
}

void H5vccWasmPlayer::setRate(uint32_t player, int32_t rate_milli) {
  if (PlayerModule* module = this->module()) {
    module->SetRate(player, rate_milli);
  }
}

void H5vccWasmPlayer::setVolume(uint32_t player, int32_t volume_milli) {
  if (PlayerModule* module = this->module()) {
    module->SetVolume(player, volume_milli);
  }
}

uint32_t H5vccWasmPlayer::getInfo(v8::Isolate* isolate,
                                  uint32_t player,
                                  uint32_t info,
                                  uint32_t info_length) {
  PlayerModule* module = this->module();
  if (!module) {
    return 0;
  }
  GuestMemory memory = core()->Memory(isolate);
  return module->GetInfo(memory, player, info, info_length);
}

uint32_t H5vccWasmPlayer::mediaCaps(v8::Isolate* isolate,
                                    uint32_t mime,
                                    uint32_t mime_length,
                                    uint32_t caps,
                                    uint32_t caps_length) {
  PlayerModule* module = this->module();
  if (!module) {
    return 0;
  }
  GuestMemory memory = core()->Memory(isolate);
  return module->MediaCaps(memory, mime, mime_length, caps, caps_length);
}

void H5vccWasmPlayer::Trace(Visitor* visitor) const {
  visitor->Trace(imports_);
  ScriptWrappable::Trace(visitor);
}

ProviderCore* H5vccWasmPlayer::core() const {
  return imports_->core();
}

PlayerModule* H5vccWasmPlayer::module() const {
  ProviderCore* core = this->core();
  return core ? core->GetModule<PlayerModule>() : nullptr;
}

}  // namespace blink
