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

#include "cobalt/starboard_wasm/player/player_module.h"

#include "base/numerics/safe_conversions.h"
#include "cobalt/starboard_wasm/core/guest_memory.h"
#include "cobalt/starboard_wasm/player/player_abi.h"
#include "cobalt/starboard_wasm/player/player_host.h"

namespace cobalt {
namespace starboard_wasm {

PlayerModule::PlayerModule(const ModuleContext& context)
    : host_(std::make_unique<PlayerHost>(context)) {}

PlayerModule::~PlayerModule() = default;

const char* PlayerModule::name() const {
  return kName;
}

uint32_t PlayerModule::SurfaceCreate(uint32_t flags) {
  return host_->SurfaceCreate(flags);
}

void PlayerModule::SurfaceDestroy(uint32_t surface) {
  host_->SurfaceDestroy(surface);
}

void PlayerModule::SurfaceSetBounds(uint32_t surface,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    int32_t z_index) {
  host_->SurfaceSetBounds(surface, x, y, width, height, z_index);
}

std::string PlayerModule::SurfaceToken(uint32_t surface) const {
  return host_->SurfaceToken(surface);
}

uint32_t PlayerModule::Create(GuestMemory& memory,
                              uint32_t audio_config_ptr,
                              uint32_t audio_config_len,
                              uint32_t video_config_ptr,
                              uint32_t video_config_len,
                              uint32_t surface,
                              uint32_t flags) {
  const uint8_t* audio = memory.Bytes(audio_config_ptr, audio_config_len);
  const uint8_t* video = memory.Bytes(video_config_ptr, video_config_len);
  if ((!audio && audio_config_len) || (!video && video_config_len)) {
    return 0;
  }
  return host_->PlayerCreate(audio, audio_config_len, video, video_config_len,
                             surface, flags);
}

void PlayerModule::Destroy(uint32_t player) {
  host_->PlayerDestroy(player);
}

void PlayerModule::Seek(uint32_t player, double position_us, uint32_t ticket) {
  host_->PlayerSeek(player, base::saturated_cast<int64_t>(position_us), ticket);
}

uint32_t PlayerModule::WriteSamples(GuestMemory& memory,
                                    uint32_t player,
                                    uint32_t track,
                                    uint32_t bytes_ptr,
                                    uint32_t bytes_len,
                                    uint32_t infos_ptr,
                                    uint32_t count,
                                    uint32_t config_ptr,
                                    uint32_t config_len) {
  const uint8_t* config = memory.Bytes(config_ptr, config_len);
  if (!config && config_len) {
    return 0;
  }
  return host_->PlayerWriteSamples(
      player, track, memory.Bytes(bytes_ptr, bytes_len), bytes_len,
      memory.Array(infos_ptr, count, sizeof(me_sample_info)), count, config,
      config_len);
}

void PlayerModule::WriteEos(uint32_t player, uint32_t track) {
  host_->PlayerWriteEos(player, track);
}

void PlayerModule::SetRate(uint32_t player, int32_t rate_milli) {
  host_->PlayerSetRate(player, rate_milli);
}

void PlayerModule::SetVolume(uint32_t player, int32_t volume_milli) {
  host_->PlayerSetVolume(player, volume_milli);
}

uint32_t PlayerModule::GetInfo(GuestMemory& memory,
                               uint32_t player,
                               uint32_t info_ptr,
                               uint32_t info_len) {
  return host_->PlayerGetInfo(player, memory.Bytes(info_ptr, info_len),
                              info_len);
}

uint32_t PlayerModule::MediaCaps(GuestMemory& memory,
                                 uint32_t mime_ptr,
                                 uint32_t mime_len,
                                 uint32_t caps_ptr,
                                 uint32_t caps_len) {
  const uint8_t* mime = memory.Bytes(mime_ptr, mime_len);
  if (!mime && mime_len) {
    return 0;
  }
  return host_->MediaCaps(reinterpret_cast<const char*>(mime),
                          mime ? mime_len : 0, memory.Bytes(caps_ptr, caps_len),
                          caps_len);
}

std::unique_ptr<Module> CreatePlayerModule(const ModuleContext& context) {
  return std::make_unique<PlayerModule>(context);
}

}  // namespace starboard_wasm
}  // namespace cobalt
