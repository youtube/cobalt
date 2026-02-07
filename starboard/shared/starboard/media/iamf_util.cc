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

#include "starboard/shared/starboard/media/iamf_util.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

#include "starboard/common/string.h"

namespace starboard {
namespace {
// Checks if |input| is a valid IAMF profile value, and stores the converted
// value in |*profile| if so.
bool StringToProfile(const std::string& input, uint32_t* profile) {
  SB_DCHECK(profile);

  if (input.size() != 3) {
    return false;
  }

  if (!std::isdigit(input[0]) || !std::isdigit(input[1]) ||
      !std::isdigit(input[2])) {
    return false;
  }

  uint32_t converted_val = static_cast<uint32_t>(std::atoi(input.c_str()));
  if (converted_val > kIamfProfileMax) {
    return false;
  }
  *profile = converted_val;
  return true;
}
}  // namespace

IamfMimeUtil::IamfMimeUtil(const std::string& mime_type) {
  // Reference: Immersive Audio Model and Formats;
  //            v1.0.0
  //            6.3. Codecs Parameter String
  // (https://aomediacodec.github.io/iamf/v1.0.0-errata.html#codecsparameter)
  if (mime_type.find("iamf") != 0) {
    return;
  }

  // 4   FOURCC string "iamf".
  // +1  delimiting period.
  // +3  primary_profile as 3 digit string.
  // +1  delimiting period.
  // +3  additional_profile as 3 digit string.
  // +1  delimiting period.
  // +9  The remaining string is one of "Opus", "mp4a.40.2", "fLaC", or "ipcm".
  constexpr int kMaxIamfCodecIdLength = 22;
  if (mime_type.size() > kMaxIamfCodecIdLength) {
    return;
  }

  const std::vector<std::string> vec = SplitString(mime_type, '.');
  // The mime type must be in 4 parts for all substreams other than AAC, which
  // is 6 parts.
  if (vec.size() != 4 && vec.size() != 6) {
    return;
  }

  // The primary profile must be between 0 and 255 inclusive.
  uint32_t primary_profile = 0;
  if (!StringToProfile(vec[1], &primary_profile)) {
    return;
  }

  // The additional profile must be between 0 and 255 inclusive.
  uint32_t additional_profile = 0;
  if (!StringToProfile(vec[2], &additional_profile)) {
    return;
  }

  // The codec string should be one of "Opus", "mp4a", "fLaC", or "ipcm".
  std::string codec = vec[3];
  if ((codec != "Opus") && (codec != "mp4a") && (codec != "fLaC") &&
      (codec != "ipcm")) {
    return;
  }

  // Only IAMF codec parameter strings with "mp4a" should be greater than 4
  // elements.
  if (codec == "mp4a") {
    if (vec.size() != 6) {
      return;
    }

    // The fields following "mp4a" should be "40" and "2" to signal AAC-LC.
    if (vec[4] != "40" || vec[5] != "2") {
      return;
    }
    substream_codec_ = kIamfSubstreamCodecMp4a;
  } else {
    if (vec.size() > 4) {
      return;
    }
    if (codec == "Opus") {
      substream_codec_ = kIamfSubstreamCodecOpus;
    } else if (codec == "fLaC") {
      substream_codec_ = kIamfSubstreamCodecFlac;
    } else {
      substream_codec_ = kIamfSubstreamCodecIpcm;
    }
  }

  primary_profile_ = primary_profile;
  additional_profile_ = additional_profile;
}

}  // namespace starboard
