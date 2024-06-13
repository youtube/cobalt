// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/player_components_factory.h"

#include "starboard/android/shared/drm_system.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// static
std::unique_ptr<PlayerComponents::Factory> PlayerComponents::Factory::Create() {
  return std::unique_ptr<PlayerComponents::Factory>(
      new android::shared::PlayerComponentsFactory);
}

// static
bool PlayerComponents::Factory::OutputModeSupported(
    SbPlayerOutputMode output_mode,
    SbMediaVideoCodec codec,
    SbDrmSystem drm_system) {
  using ::starboard::android::shared::DrmSystem;

  if (output_mode == kSbPlayerOutputModePunchOut) {
    return true;
  }

  if (output_mode == kSbPlayerOutputModeDecodeToTexture) {
    if (!SbDrmSystemIsValid(drm_system)) {
      return true;
    }
    DrmSystem* android_drm_system = static_cast<DrmSystem*>(drm_system);
    bool require_secure_decoder = android_drm_system->require_secured_decoder();
    SB_LOG_IF(INFO, require_secure_decoder)
        << "Output mode under decode-to-texture is not supported due to secure "
           "decoder is required.";
    return !require_secure_decoder;
  }

  return false;
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
