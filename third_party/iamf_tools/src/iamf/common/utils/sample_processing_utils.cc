/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/common/utils/sample_processing_utils.h"

#include <cstddef>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace iamf_tools {

absl::Status WritePcmSample(uint32_t sample, uint8_t sample_size,
                            bool big_endian, uint8_t* const buffer,
                            size_t& write_position) {
  // Validate assumptions of the logic in the `for` loop below.
  if (sample_size % 8 != 0 || sample_size > 32) [[unlikely]] {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid sample size: ", sample_size));
  }

  for (int shift = 32 - sample_size; shift < 32; shift += 8) {
    uint8_t byte = 0;
    if (big_endian) [[unlikely]] {
      byte = (sample >> ((32 - sample_size) + (32 - (shift + 8)))) & 0xff;
    } else {
      byte = (sample >> shift) & 0xff;
    }
    buffer[write_position++] = byte;
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
