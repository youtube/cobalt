// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_DMP_FILE_SELECTOR_H_
#define STARBOARD_NPLB_DMP_FILE_SELECTOR_H_

#include <vector>

#include "starboard/media.h"

namespace starboard {
namespace nplb {

class DmpFileSelector {
 public:
  typedef enum DeviceType {
    kDeviceTypeFHD,
    kDeviceType4k,
    kDeviceType8k,
  } DeviceType;

  DmpFileSelector();
  ~DmpFileSelector() {}

  std::vector<const char*> GetSupportedVideoDmpFiles(
      std::vector<const char*> video_files,
      const char* key_system);

 private:
  bool IsCodecSupported(SbMediaVideoCodec codec) const;
  bool CheckCodecSatisfied(SbMediaVideoCodec codec) const;
  void SetCodecSatisfied(SbMediaVideoCodec codec);

  bool av1_satisfied_ = false;
  bool avc_satisfied_ = false;
  bool vp9_satisfied_ = false;

  bool supports_av1_ = false;
  bool supports_vp9_ = false;

  DeviceType type_;
  int max_frame_width_ = 0;

  std::vector<const char*> supported_video_files_;
};

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_DMP_FILE_SELECTOR_H_
