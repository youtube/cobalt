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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_RESOLUTIONS_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_RESOLUTIONS_H_

#include "starboard/common/size.h"

namespace starboard {

// Well-known video resolutions.
struct Resolution {
  static constexpr Size k240p{426, 240};
  static constexpr Size k360p{640, 360};
  static constexpr Size k480p{854, 480};
  static constexpr Size k720p{1280, 720};    // HD
  static constexpr Size k1080p{1920, 1080};  // Full HD (FHD)
  static constexpr Size k1440p{2560, 1440};  // Quad HD (QHD)
  static constexpr Size k4k{3840, 2160};     // 4K Ultra HD (UHD)
  static constexpr Size k8k{7680, 4320};     // 8K Ultra HD
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_RESOLUTIONS_H_
