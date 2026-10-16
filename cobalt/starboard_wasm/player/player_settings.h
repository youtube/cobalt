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

// Settings that the embedder's media pipeline applies to its own players
// (H5vcc `Media.*` settings, viewport). PlayerHost applies the same ones, so
// players created through the `sb_player` imports behave like `<video>`
// players. Depends only on //base so the embedder can provide them without
// depending on the player host.

#ifndef COBALT_STARBOARD_WASM_PLAYER_PLAYER_SETTINGS_H_
#define COBALT_STARBOARD_WASM_PLAYER_PLAYER_SETTINGS_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/functional/callback.h"

namespace cobalt {
namespace starboard_wasm {

struct PlayerSettings {
  PlayerSettings();
  PlayerSettings(const PlayerSettings&);
  PlayerSettings& operator=(const PlayerSettings&);
  ~PlayerSettings();

  // Integer Starboard experimental features, set on the player thread before
  // each SbPlayerCreate().
  std::map<std::string, int64_t> experimental_features;
  // Limits the samples per SbPlayerWriteSamples() call; 0 for the default.
  int max_samples_per_write = 0;
  // Bounds of a surface that has no position; 0 if unknown.
  int viewport_width = 0;
  int viewport_height = 0;
};

// Returns the current settings. Runs on player threads, right before a player
// is created, so it must be thread-safe and may block briefly.
using PlayerSettingsProvider = base::RepeatingCallback<PlayerSettings()>;

// Sets the process-wide provider. Without one, players get Starboard's
// defaults.
void SetPlayerSettingsProvider(PlayerSettingsProvider provider);

PlayerSettings GetPlayerSettings();

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_PLAYER_PLAYER_SETTINGS_H_
