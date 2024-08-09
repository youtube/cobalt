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

#ifndef STARBOARD_SHARED_LIBIAMF_IAMF_CONFIG_READER_H_
#define STARBOARD_SHARED_LIBIAMF_IAMF_CONFIG_READER_H_

#include <optional>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace shared {
namespace libiamf {

// TODO: Add handling for non-redundant OBUS
// TODO: Implement or depend on a buffer reader to simplify the implementation.
class IamfConfigReader {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;

  IamfConfigReader() = default;

  bool ResetAndRead(scoped_refptr<InputBuffer> input_buffer);

  void Reset();

  bool is_valid() {
    return data_size_ > 0 && config_size_ > 0 && has_mix_presentation_id() &&
           sample_rate_ > 0 && samples_per_buffer_ > 0;
  }
  int sample_rate() { return sample_rate_; }
  uint32_t samples_per_buffer() { return samples_per_buffer_; }
  uint32_t config_size() { return config_size_; }
  // TODO: Allow for selection of multiple mix presentation IDs. Currently,
  // only the first mix presentation parsed is selected.
  bool has_mix_presentation_id() { return mix_presentation_id_.has_value(); }
  uint32_t mix_presentation_id() {
    SB_DCHECK(mix_presentation_id_.has_value());
    return mix_presentation_id_.value();
  }

  std::vector<uint8_t> config_obus() { return config_obus_; }
  std::vector<uint8_t> data() { return data_; }

 private:
  bool Read(scoped_refptr<InputBuffer> input_buffer);
  bool ReadOBU(const uint8_t* buf, bool& completed_parsing);
  bool ReadOBUHeader(const uint8_t* buf,
                     uint8_t* obu_type,
                     uint32_t* obu_size,
                     uint32_t* header_size);

  int buffer_head_ = 0;
  int sample_rate_ = 0;
  int sample_size_ = 0;
  uint32_t samples_per_buffer_ = 0;
  uint32_t config_size_ = 0;
  uint32_t data_size_ = 0;

  std::optional<uint32_t> mix_presentation_id_;

  bool has_valid_config_ = false;
  bool binaural_mix_presentation_id_ = -1;
  std::vector<uint8_t> config_obus_;
  std::vector<uint8_t> data_;
};

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBIAMF_IAMF_CONFIG_READER_H_
