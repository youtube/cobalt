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

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "starboard/common/string.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {
// Helper function to convert an input string to a uint32_t. On success, it
// returns true and sets |*output| to the converted number. It returns false if
// the input contains non-digit characters, if the converted number is too
// large to fit in uint32_t, or if the input is empty.
bool StringToUInt(const std::string& input, uint32_t* output) {
  SB_DCHECK(output);
  if (input.empty()) {
    return false;
  }
  uint64_t output_l = 0;
  std::string reversed_input = std::string(input.rbegin(), input.rend());
  for (int i = 0; i < reversed_input.size(); ++i) {
    uint32_t digit = reversed_input[i] - '0';
    if (digit > 9) {
      // The read character is not a digit.
      return false;
    }
    output_l += (digit * std::pow(10, i));
    if (output_l > std::numeric_limits<uint32_t>::max()) {
      // Number is too large to fit in uint32_t.
      return false;
    }
  }
  *output = static_cast<uint32_t>(output_l);
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

  // The length of the primary and additional profile strings must be 3.
  if (vec[1].size() != 3 || vec[2].size() != 3) {
    return;
  }

  // The primary profile must be between 0 and 255 inclusive.
  uint32_t primary_profile = 0;
  if (!StringToUInt(vec[1], &primary_profile) ||
      primary_profile > kIamfProfileMax) {
    return;
  }

  // The additional profile must be between 0 and 255 inclusive.
  uint32_t additional_profile = 0;
  if (!StringToUInt(vec[2], &additional_profile) ||
      additional_profile > kIamfProfileMax) {
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

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
