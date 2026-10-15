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

#ifndef COBALT_STARBOARD_WASM_PLAYER_PLAYER_MODULE_H_
#define COBALT_STARBOARD_WASM_PLAYER_PLAYER_MODULE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "cobalt/starboard_wasm/core/module.h"

namespace cobalt {
namespace starboard_wasm {

class GuestMemory;
class PlayerHost;

// Implements H5vccWasmSurface and H5vccWasmPlayer. Records passed by pointer
// are defined in player_abi.h.
class PlayerModule : public Module {
 public:
  static constexpr char kName[] = "player";

  explicit PlayerModule(const ModuleContext& context);
  PlayerModule(const PlayerModule&) = delete;
  PlayerModule& operator=(const PlayerModule&) = delete;
  ~PlayerModule() override;

  // Module:
  const char* name() const override;

  // H5vccWasmSurface.
  uint32_t SurfaceCreate(uint32_t flags);
  void SurfaceDestroy(uint32_t surface);
  void SurfaceSetBounds(uint32_t surface,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height,
                        int32_t z_index);
  // Returns the token that binds |surface| to a `<video>` element through a
  // `blob:...#sb-surface=<token>` URL, or "" if |surface| is unknown.
  std::string SurfaceToken(uint32_t surface) const;

  // H5vccWasmPlayer.
  uint32_t Create(GuestMemory& memory,
                  uint32_t audio_config_ptr,
                  uint32_t audio_config_len,
                  uint32_t video_config_ptr,
                  uint32_t video_config_len,
                  uint32_t surface,
                  uint32_t flags);
  void Destroy(uint32_t player);
  void Seek(uint32_t player, double position_us, uint32_t ticket);
  uint32_t WriteSamples(GuestMemory& memory,
                        uint32_t player,
                        uint32_t track,
                        uint32_t bytes_ptr,
                        uint32_t bytes_len,
                        uint32_t infos_ptr,
                        uint32_t count,
                        uint32_t config_ptr,
                        uint32_t config_len);
  void WriteEos(uint32_t player, uint32_t track);
  void SetRate(uint32_t player, int32_t rate_milli);
  void SetVolume(uint32_t player, int32_t volume_milli);
  uint32_t GetInfo(GuestMemory& memory,
                   uint32_t player,
                   uint32_t info_ptr,
                   uint32_t info_len);
  uint32_t MediaCaps(GuestMemory& memory,
                     uint32_t mime_ptr,
                     uint32_t mime_len,
                     uint32_t caps_ptr,
                     uint32_t caps_len);

 private:
  std::unique_ptr<PlayerHost> host_;
};

std::unique_ptr<Module> CreatePlayerModule(const ModuleContext& context);

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_PLAYER_PLAYER_MODULE_H_
