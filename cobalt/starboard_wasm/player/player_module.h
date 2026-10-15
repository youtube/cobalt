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

class PlayerHost;

// The `sb_player` and `sb_surface` imports.
class PlayerModule : public Module {
 public:
  static constexpr char kName[] = "player";

  explicit PlayerModule(const ModuleContext& context);
  PlayerModule(const PlayerModule&) = delete;
  PlayerModule& operator=(const PlayerModule&) = delete;
  ~PlayerModule() override;

  // Module:
  const char* name() const override;
  void Bind(ImportTable& table) override;

  // Returns the token that binds |surface| to a `<video>` element through a
  // `blob:...#sb-surface=<token>` URL, or "" if |surface| is unknown.
  std::string SurfaceToken(uint32_t surface) const;

  PlayerHost& host() { return *host_; }

 private:
  std::unique_ptr<PlayerHost> host_;
};

std::unique_ptr<Module> CreatePlayerModule(const ModuleContext& context);

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_PLAYER_PLAYER_MODULE_H_
