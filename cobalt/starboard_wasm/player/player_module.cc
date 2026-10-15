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

#include "cobalt/starboard_wasm/core/import_table.h"
#include "cobalt/starboard_wasm/player/player_abi.h"
#include "cobalt/starboard_wasm/player/player_host.h"

namespace cobalt {
namespace starboard_wasm {

namespace {

uint32_t SurfaceCreate(PlayerModule& m, Call& /*call*/, uint32_t flags) {
  return m.host().SurfaceCreate(flags);
}

void SurfaceDestroy(PlayerModule& m, Call& /*call*/, uint32_t surface) {
  m.host().SurfaceDestroy(surface);
}

void SurfaceSetBounds(PlayerModule& m,
                      Call& /*call*/,
                      uint32_t surface,
                      int32_t x,
                      int32_t y,
                      int32_t width,
                      int32_t height,
                      int32_t z_index) {
  m.host().SurfaceSetBounds(surface, x, y, width, height, z_index);
}

uint32_t Create(PlayerModule& m,
                Call& call,
                uint32_t audio_ptr,
                uint32_t audio_len,
                uint32_t video_ptr,
                uint32_t video_len,
                uint32_t surface,
                uint32_t flags) {
  const uint8_t* audio = call.Bytes(audio_ptr, audio_len);
  const uint8_t* video = call.Bytes(video_ptr, video_len);
  if ((!audio && audio_len) || (!video && video_len)) {
    return 0;
  }
  return m.host().PlayerCreate(audio, audio_len, video, video_len, surface,
                               flags);
}

void Destroy(PlayerModule& m, Call& /*call*/, uint32_t player) {
  m.host().PlayerDestroy(player);
}

void Seek(PlayerModule& m,
          Call& /*call*/,
          uint32_t player,
          int64_t position_us,
          uint32_t ticket) {
  m.host().PlayerSeek(player, position_us, ticket);
}

uint32_t WriteSamples(PlayerModule& m,
                      Call& call,
                      uint32_t player,
                      uint32_t track,
                      uint32_t bytes_ptr,
                      uint32_t bytes_len,
                      uint32_t infos_ptr,
                      uint32_t count,
                      uint32_t config_ptr,
                      uint32_t config_len) {
  const uint8_t* config = call.Bytes(config_ptr, config_len);
  if (!config && config_len) {
    return 0;
  }
  return m.host().PlayerWriteSamples(
      player, track, call.Bytes(bytes_ptr, bytes_len), bytes_len,
      call.Array(infos_ptr, count, sizeof(me_sample_info)), count, config,
      config_len);
}

void WriteEos(PlayerModule& m,
              Call& /*call*/,
              uint32_t player,
              uint32_t track) {
  m.host().PlayerWriteEos(player, track);
}

void SetRate(PlayerModule& m,
             Call& /*call*/,
             uint32_t player,
             int32_t rate_milli) {
  m.host().PlayerSetRate(player, rate_milli);
}

void SetVolume(PlayerModule& m,
               Call& /*call*/,
               uint32_t player,
               int32_t volume_milli) {
  m.host().PlayerSetVolume(player, volume_milli);
}

uint32_t GetInfo(PlayerModule& m,
                 Call& call,
                 uint32_t player,
                 uint32_t out_ptr,
                 uint32_t out_len) {
  return m.host().PlayerGetInfo(player, call.Bytes(out_ptr, out_len), out_len);
}

uint32_t MediaCaps(PlayerModule& m,
                   Call& call,
                   uint32_t mime_ptr,
                   uint32_t mime_len,
                   uint32_t out_ptr,
                   uint32_t out_len) {
  const uint8_t* mime = call.Bytes(mime_ptr, mime_len);
  return m.host().MediaCaps(reinterpret_cast<const char*>(mime),
                            mime ? mime_len : 0, call.Bytes(out_ptr, out_len),
                            out_len);
}

}  // namespace

PlayerModule::PlayerModule(const ModuleContext& context)
    : host_(std::make_unique<PlayerHost>(context)) {}

PlayerModule::~PlayerModule() = default;

const char* PlayerModule::name() const {
  return kName;
}

void PlayerModule::Bind(ImportTable& table) {
  table.Add<&SurfaceCreate>("sb_surface", "create");
  table.Add<&SurfaceDestroy>("sb_surface", "destroy");
  table.Add<&SurfaceSetBounds>("sb_surface", "set_bounds");

  table.Add<&Create>("sb_player", "create");
  table.Add<&Destroy>("sb_player", "destroy");
  table.Add<&Seek>("sb_player", "seek");
  table.Add<&WriteSamples, /*kHot=*/true>("sb_player", "write_samples");
  table.Add<&WriteEos>("sb_player", "write_eos");
  table.Add<&SetRate>("sb_player", "set_rate");
  table.Add<&SetVolume>("sb_player", "set_volume");
  table.Add<&GetInfo, /*kHot=*/true>("sb_player", "get_info");
  table.Add<&MediaCaps>("sb_player", "media_caps");
}

std::string PlayerModule::SurfaceToken(uint32_t surface) const {
  return host_->SurfaceToken(surface);
}

std::unique_ptr<Module> CreatePlayerModule(const ModuleContext& context) {
  return std::make_unique<PlayerModule>(context);
}

}  // namespace starboard_wasm
}  // namespace cobalt
