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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_AVC_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_AVC_UTIL_H_

#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// Parse avc nalus produced by the Cobalt demuxer.
// It makes the following assumptions:
// 1. It won't be used frequently.  So it aimed to be easy to use, instead of
//    being efficient.
// 2. The data it processes is produced by Cobalt, and should be valid most of
//    of the time.  The class won't crash on invalid input, but besides checking
//    if the input starts with a nalu header, it doesn't do any further
//    validations.  It is up to the decoder to detect invalid input.
// 3. It assumes that multiple sps/pps nalus in different orders are different
//    parameter sets.
class AvcParameterSets {
 public:
  enum Format {
    kAnnexB,
    kHeadless,
  };

  static const size_t kAnnexBHeaderSizeInBytes = 4;
  static const uint8_t kIdrStartCode = 0x65;
  static const uint8_t kSpsStartCode = 0x67;
  static const uint8_t kPpsStartCode = 0x68;

  // Only |format == kAnnexB| is supported, which is checked in the ctor.
  AvcParameterSets(Format format, const uint8_t* data, size_t size);

  Format format() const { return format_; }
  bool is_valid() const { return is_valid_; }
  bool has_sps_and_pps() const {
    return first_sps_index_ != -1 && first_pps_index_ != -1;
  }

  const std::vector<uint8_t>& first_sps() const {
    SB_DCHECK(first_sps_index_ != -1);
    return parameter_sets_[first_sps_index_];
  }
  const std::vector<uint8_t>& first_pps() const {
    SB_DCHECK(first_pps_index_ != -1);
    return parameter_sets_[first_pps_index_];
  }

  std::vector<const uint8_t*> GetAddresses() const {
    std::vector<const uint8_t*> addresses;
    for (auto& parameter_set : parameter_sets_) {
      addresses.push_back(parameter_set.data());
    }
    return addresses;
  }
  std::vector<size_t> GetSizesInBytes() const {
    std::vector<size_t> sizes_in_bytes;
    for (auto& parameter_set : parameter_sets_) {
      sizes_in_bytes.push_back(parameter_set.size());
    }
    return sizes_in_bytes;
  }
  size_t combined_size_in_bytes() const { return combined_size_in_bytes_; }

  AvcParameterSets ConvertTo(Format new_format) const;

  bool operator==(const AvcParameterSets& that) const;
  bool operator!=(const AvcParameterSets& that) const;

 private:
  Format format_ = kAnnexB;
  bool is_valid_ = false;
  int first_sps_index_ = -1;
  int first_pps_index_ = -1;
  std::vector<std::vector<uint8_t>> parameter_sets_;
  size_t combined_size_in_bytes_ = 0;
};

// The function will fail only when the input doesn't start with an Annex B
// nalu header.
bool ConvertAnnexBToAvcc(const uint8_t* annex_b_source,
                         size_t size,
                         uint8_t* avcc_destination);

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_AVC_UTIL_H_
