// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/media/player_configuration.h"

#include <atomic>

#include "starboard/common/once.h"
#include "starboard/extension/media/player_configuration.h"

namespace starboard {
namespace shared {
namespace uikit {

namespace {

class PlayerConfigurations {
 public:
  bool IsDecodeToTexturePreferred() { return decode_to_texture_preferred_; }

  void SetDecodeToTexturePreferred(bool enforced) {
    decode_to_texture_preferred_ = enforced;
  }

 private:
  std::atomic_bool decode_to_texture_preferred_{false};
};

// static
SB_ONCE_INITIALIZE_FUNCTION(PlayerConfigurations, GetInstance);

void SetDecodeToTexturePreferred(bool enforced) {
  GetInstance()->SetDecodeToTexturePreferred(enforced);
}

const StarboardExtensionPlayerConfigurationApi kPlayerConfigurationApi = {
    kStarboardExtensionPlayerConfigurationName,
    1,  // API version that's implemented.
    &SetDecodeToTexturePreferred, nullptr};

}  // namespace

bool IsDecodeToTexturePreferred() {
  return GetInstance()->IsDecodeToTexturePreferred();
}

const void* GetPlayerConfigurationApi() {
  return &kPlayerConfigurationApi;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
