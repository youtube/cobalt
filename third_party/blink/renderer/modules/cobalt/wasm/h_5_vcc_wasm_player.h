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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_WASM_H_5_VCC_WASM_PLAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_WASM_H_5_VCC_WASM_PLAYER_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8-forward.h"

namespace blink {

class H5vccWasmImports;

// Implements h_5_vcc_wasm_player.idl on cobalt::starboard_wasm::WasmPlayerHost.
// Owned by H5vccWasmImports and used only on its ExecutionContext's thread.
// Once the context is destroyed, operations do nothing and return 0 or false.
class H5vccWasmPlayer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit H5vccWasmPlayer(H5vccWasmImports& imports);

  bool setExperimentalFeaturesForCurrentThread(v8::Isolate* isolate,
                                               uint32_t entries,
                                               uint32_t count);
  bool setMaxVideoInputSizeForCurrentThread(int32_t max_video_input_size);
  bool setMaxVideoResolutionForCurrentThread(
      v8::Isolate* isolate,
      uint32_t max_video_resolution,
      uint32_t max_video_resolution_size);
  uint32_t create(v8::Isolate* isolate,
                  uint32_t surface,
                  uint32_t creation_param,
                  uint32_t creation_param_size);
  void destroy(uint32_t player);
  bool seek(uint32_t player, double seek_to_timestamp, int32_t ticket);
  bool writeSamples(v8::Isolate* isolate,
                    uint32_t player,
                    int32_t sample_type,
                    uint32_t sample_infos,
                    uint32_t count);
  bool writeEndOfStream(uint32_t player, int32_t stream_type);
  void setBounds(uint32_t player,
                 int32_t z_index,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height);
  bool setPlaybackRate(uint32_t player, double playback_rate);
  void setVolume(uint32_t player, double volume);
  bool getInfo(v8::Isolate* isolate, uint32_t player, uint32_t info);
  bool getAudioConfiguration(v8::Isolate* isolate,
                             uint32_t player,
                             int32_t index,
                             uint32_t out);
  uint32_t getErrorMessage(v8::Isolate* isolate,
                           uint32_t player,
                           uint32_t out,
                           uint32_t out_size);
  int32_t getMaximumNumberOfSamplesPerWrite(uint32_t player,
                                            int32_t sample_type);
  int32_t getPreferredOutputMode(v8::Isolate* isolate,
                                 uint32_t creation_param,
                                 uint32_t creation_param_size);
  int32_t canPlayMimeAndKeySystem(v8::Isolate* isolate,
                                  uint32_t mime,
                                  uint32_t mime_size,
                                  uint32_t key_system,
                                  uint32_t key_system_size);
  bool canChangeType(v8::Isolate* isolate,
                     uint32_t current_mime,
                     uint32_t current_mime_size,
                     uint32_t new_mime,
                     uint32_t new_mime_size);
  void requestWake();
  uint32_t pollEvents(v8::Isolate* isolate, uint32_t events, uint32_t count);

  void Trace(Visitor* visitor) const override;

 private:
  Member<H5vccWasmImports> imports_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COBALT_WASM_H_5_VCC_WASM_PLAYER_H_
