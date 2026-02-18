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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_IAMF_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_IAMF_UTIL_H_

#include <limits>
#include <string>

#include "starboard/common/log.h"
#include "starboard/shared/internal_only.h"

namespace starboard {

enum IamfSubstreamCodec {
  kIamfSubstreamCodecUnknown,
  kIamfSubstreamCodecOpus,
  kIamfSubstreamCodecMp4a,
  kIamfSubstreamCodecFlac,
  kIamfSubstreamCodecIpcm
};

// These values must match the profile values defined in
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#profiles
constexpr uint32_t kIamfProfileSimple = 0;
constexpr uint32_t kIamfProfileBase = 1;
constexpr uint32_t kIamfProfileMax = 255;

// Parses an IAMF codecs parameter string following the convention defined in
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#codecsparameter.
// Always check is_valid() before calling the getter functions.
class IamfMimeUtil {
 public:
  explicit IamfMimeUtil(const std::string& mime_type);

  // Parses IAMF Config OBUs for the primary and additional profiles,
  // based on IAMF specification v1.0.0-errata.
  // https://aomediacodec.github.io/iamf/v1.1.0.html#codecsparameter.
  static void ParseIamfConfigOBU(const uint8_t* data,
                                 size_t size,
                                 uint8_t* primary_profile,
                                 uint8_t* additional_profile);

  bool is_valid() const {
    return primary_profile_ <= kIamfProfileMax &&
           additional_profile_ <= kIamfProfileMax &&
           substream_codec_ != kIamfSubstreamCodecUnknown;
  }
  uint32_t primary_profile() const {
    SB_DCHECK(is_valid());
    return primary_profile_;
  }
  uint32_t additional_profile() const {
    SB_DCHECK(is_valid());
    return additional_profile_;
  }
  IamfSubstreamCodec substream_codec() const {
    SB_DCHECK(is_valid());
    return substream_codec_;
  }

 private:
  uint32_t primary_profile_ = std::numeric_limits<uint32_t>::max();
  uint32_t additional_profile_ = std::numeric_limits<uint32_t>::max();
  IamfSubstreamCodec substream_codec_ = kIamfSubstreamCodecUnknown;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_IAMF_UTIL_H_
