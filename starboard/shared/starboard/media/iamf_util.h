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

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

enum IamfSubstreamCodec {
  kIamfSubstreamCodecOpus,
  kIamfSubstreamCodecMp4a,
  kIamfSubstreamCodecFlac,
  kIamfSubstreamCodecIpcm,
  kIamfSubstreamCodecUnknown
};

// These values must match the profile values defined in
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#profiles
inline constexpr int kIamfProfileSimple = 0;
inline constexpr int kIamfProfileBase = 1;
inline constexpr int kIamfProfileMax = 255;

// Parses an IAMF codecs parameter string following the convention defined in
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#codecsparameter.
// Always check is_valid() before calling the getter functions.
class IamfMimeUtil {
 public:
  explicit IamfMimeUtil(const std::string& mime_type);

  bool is_valid() const {
    return profile_ <= kIamfProfileMax &&
           substream_codec_ != kIamfSubstreamCodecUnknown;
  }
  int profile() const { return profile_; }
  IamfSubstreamCodec substream_codec() const { return substream_codec_; }

 private:
  int profile_ = std::numeric_limits<int>::max();
  IamfSubstreamCodec substream_codec_ = kIamfSubstreamCodecUnknown;
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_IAMF_UTIL_H_
