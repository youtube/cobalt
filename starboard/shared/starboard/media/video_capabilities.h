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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_VIDEO_CAPABILITIES_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_VIDEO_CAPABILITIES_H_

#include <string>
#include <vector>

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// Allows to query video capabilities via simple, preset rules.  With a setup
// like:
//   VideoCapabilities video_capabilities;
//
//   video_capabilities.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1080, 60);
//   video_capabilities.AddSdrRule(kSbMediaVideoCodecVp9, 3840, 2160, 30);
//   video_capabilities.AddSdrRule(kSbMediaVideoCodecVp9, 2560, 1440, 60);
// We can query whether certain playbacks are supported using:
//   video_capabilities.IsSupported(
//      kSbMediaVideoCodecH264, kSbMediaTransferIdBt709, 640, 480, 30);
class VideoCapabilities {
 public:
  void AddSdrRule(SbMediaVideoCodec codec, int width, int height, int fps);
  void AddHdrRule(SbMediaVideoCodec codec, int width, int height, int fps);

  bool IsSupported(SbMediaVideoCodec codec,
                   SbMediaTransferId transfer_id,
                   int width,
                   int height,
                   int fps) const;

 private:
  class Rule {
   public:
    Rule(SbMediaVideoCodec codec, int width, int height, int fps);

    std::string AsString() const;
    bool CanBeInferredBy(const Rule& that) const;

   private:
    const SbMediaVideoCodec codec_;
    const int width_;
    const int height_;
    const int fps_;
  };

  typedef std::vector<Rule> Rules;

  static void AddRule(const Rule& new_rule, Rules* rules);
  static bool IsSupported(const Rule& rule_to_check, const Rules& rules);

  Rules sdr_rules_;
  Rules hdr_rules_;
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_VIDEO_CAPABILITIES_H_
