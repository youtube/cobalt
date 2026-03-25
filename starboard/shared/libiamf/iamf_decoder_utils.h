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

#ifndef STARBOARD_SHARED_LIBIAMF_IAMF_DECODER_UTILS_H_
#define STARBOARD_SHARED_LIBIAMF_IAMF_DECODER_UTILS_H_

#include <optional>

#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {

// Holds information parsed from an IAMF input buffer. This struct uses
// non-owning pointers to view data within the original InputBuffer to avoid
// unnecessary copies.
struct IamfBufferInfo {
  uint32_t num_samples = 0;
  int sample_rate = 0;
  std::optional<uint32_t> mix_presentation_id;

  const uint8_t* config_obus = nullptr;
  size_t config_obus_size = 0;
  const uint8_t* data = nullptr;
  size_t data_size = 0;

  // Keep a reference to the input buffer to ensure the data pointers remain
  // valid.
  scoped_refptr<InputBuffer> input_buffer;

  bool is_valid() const;
};

bool ParseInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                      IamfBufferInfo* info,
                      const bool prefer_binaural_audio,
                      const bool prefer_surround_audio);

}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBIAMF_IAMF_DECODER_UTILS_H_
