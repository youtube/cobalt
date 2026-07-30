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
#include "iamf/common/utils/numeric_utils.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace iamf_tools {

absl::Status AddUint32CheckOverflow(uint32_t x_1, uint32_t x_2,
                                    uint32_t& result) {
  // Add in the payload size.
  const uint64_t sum = static_cast<uint64_t>(x_1) + static_cast<uint64_t>(x_2);
  // Check if this would overflow as a `uint32_t`.
  if (sum > std::numeric_limits<uint32_t>::max()) {
    return absl::InvalidArgumentError(
        "Result of AddUint32CheckOverflow would overflow a uint32_t.");
  }
  result = static_cast<uint32_t>(sum);
  return absl::OkStatus();
}

absl::Status FloatToQ7_8(float value, int16_t& result) {
  // Q7.8 format can represent values in the range [-2^7, 2^7 - 2^-8].
  if (std::isnan(value) || value < -128 || (128.0 - 1.0 / 256.0) < value) {
    return absl::UnknownError(absl::StrCat(
        "Value, ", value, " cannot be represented in Q7.8 format."));
  }
  result = static_cast<int16_t>(value * (1 << 8));
  return absl::OkStatus();
}

float Q7_8ToFloat(int16_t value) {
  return static_cast<float>(value) * 1.0f / 256.0f;
}

absl::Status FloatToQ0_8(float value, uint8_t& result) {
  // Q0.8 format can represent values in the range [0, 1 - 2^-8].
  if (std::isnan(value) || value < 0 || 1 <= value) {
    return absl::UnknownError(absl::StrCat(
        "Value, ", value, " cannot be represented in Q0.8 format."));
  }

  result = static_cast<uint8_t>(value * (1 << 8));
  return absl::OkStatus();
}

float Q0_8ToFloat(uint8_t value) {
  return static_cast<float>(value) * 1.0f / 256.0f;
}

absl::Status LittleEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                      int32_t& output) {
  // If we have bytes A, B, C, D, then we need to read them as:
  //   (D << 24) | (C << 16) | (B << 8) | A
  // If we have less than four bytes, e.g. two bytes, we would read them as:
  //   (B << 8) | A
  // with the upper bits filled with the sign bit.
  const size_t num_bytes = bytes.size();
  if (num_bytes > 4 || num_bytes < 1) {
    return absl::InvalidArgumentError("Need [1, 4] bytes to make an int32_t");
  }
  int32_t result = 0;
  for (size_t i = 0; i < bytes.size(); ++i) {
    const auto shift = 8 * ((4 - num_bytes) + i);
    result |= static_cast<int32_t>(bytes[i]) << shift;
  }
  output = result;
  return absl::OkStatus();
}

absl::Status BigEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                   int32_t& output) {
  // If we have bytes A, B, C, D, then we need to read them as:
  //   (A << 24) | (B << 16) | (C << 8) | D
  // If we have less than four bytes, e.g. two bytes, we would read them as:
  //   (A << 8) | B
  // with the upper bits filled with the sign bit.
  auto reversed_bytes = std::vector<uint8_t>(bytes.rbegin(), bytes.rend());
  return LittleEndianBytesToInt32(reversed_bytes, output);
}

absl::Status ClipDoubleToInt32(double input, int32_t& output) {
  if (std::isnan(input)) {
    return absl::InvalidArgumentError("Input is NaN.");
  }

  if (input > std::numeric_limits<int32_t>::max()) {
    output = std::numeric_limits<int32_t>::max();
  } else if (input < std::numeric_limits<int32_t>::min()) {
    output = std::numeric_limits<int32_t>::min();
  } else {
    output = static_cast<int32_t>(input);
  }

  return absl::OkStatus();
}

bool IsNativeBigEndian() {
  if constexpr (std::endian::native == std::endian::big) {
    return true;
  } else {
    return false;
  }
}

}  // namespace iamf_tools
