// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef INTERNAL_STARBOARD_SHARED_UIKIT_PLAYBACK_CAPABILITIES_H_
#define INTERNAL_STARBOARD_SHARED_UIKIT_PLAYBACK_CAPABILITIES_H_

#include "starboard/media.h"

namespace starboard {
namespace shared {
namespace uikit {

class PlaybackCapabilities {
 public:
  // The first query of hardware capabilities may take more than 50 ms. It's
  // better to do that in background at app launching.
  static void InitializeInBackground();
  // Returns true if the device has hardware vp9 decoder.
  static bool IsHwVp9Supported();
  // Returns true if the device is Apple TV HD.
  static bool IsAppleTVHD();
  // Returns true if the device is Apple TV 4K.
  static bool IsAppleTV4K();
  // Return audio output configuration.
  static bool GetAudioConfiguration(int index,
                                    SbMediaAudioConfiguration* configuration);
  // Force to reload audio configurations.
  static void ReloadAudioConfigurations();
};

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

#endif  // INTERNAL_STARBOARD_SHARED_UIKIT_PLAYBACK_CAPABILITIES_H_
