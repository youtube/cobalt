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

#include "cobalt/starboard_wasm/player/player_settings.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace cobalt {
namespace starboard_wasm {

namespace {

base::Lock& ProviderLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

PlayerSettingsProvider& Provider() {
  static base::NoDestructor<PlayerSettingsProvider> provider;
  return *provider;
}

}  // namespace

PlayerSettings::PlayerSettings() = default;
PlayerSettings::PlayerSettings(const PlayerSettings&) = default;
PlayerSettings& PlayerSettings::operator=(const PlayerSettings&) = default;
PlayerSettings::~PlayerSettings() = default;

void SetPlayerSettingsProvider(PlayerSettingsProvider provider) {
  base::AutoLock lock(ProviderLock());
  Provider() = std::move(provider);
}

PlayerSettings GetPlayerSettings() {
  PlayerSettingsProvider provider;
  {
    base::AutoLock lock(ProviderLock());
    provider = Provider();
  }
  return provider ? provider.Run() : PlayerSettings();
}

}  // namespace starboard_wasm
}  // namespace cobalt
