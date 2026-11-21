/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/common/leb_generator.h"

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

/*!\brief A general struct to represent an LEB128.
 *
 * When `is_signed` is false this represents a ULEB128. Otherwise this
 * represents an SLEB128. An LEB128 can be encoded in up to 8 bytes.
 *
 * A ULEB128 may have values from 0 to (1 << 32) - 1.
 * An SLEB128 may have values from -(1 << 31) to (1 << 31) - 1.
 * These bounds imply the decoded value can be stored in 32 bits.
 */
struct Leb128 {
  std::variant<DecodedUleb128, DecodedSleb128> decoded_val;
  int8_t coded_size;
  bool is_signed;
};

/*!\brief Serializes the LEB128 and returns a status based on the result.
 *
 * Serializes a leb128 and returns a status based on the result. Each byte of a
 * leb128 encodes 7 bits, the upper bit encodes whether it is the last byte in
 * the sequence. Writes the number of written to the output argument.
 *
 * \param val Leb128 to serialize.
 * \param min_size_encoding Controls whether the function writes the
 *        smallest possible representation of a LEB128. When false codes the
 *        LEB128 in `coded_size` bytes.
 * \param buffer Buffer to serialize to.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
 *         if the initial `coded_size` was invalid. `absl::UnknownError()` if
 *         the `coded_size` was insufficient to encode the value.
 */
absl::Status Leb128ToUint8Vector(const Leb128& val, bool min_size_encoding,
                                 std::vector<uint8_t>& buffer) {
  // Reject LEB128s with invalid size.
  if (val.coded_size < 1 || kMaxLeb128Size < val.coded_size) {
    return absl::InvalidArgumentError("Invalid `coded_size`");
  }

  buffer.clear();
  buffer.reserve(val.coded_size);

  const bool decoded_is_negative =
      val.is_signed && std::get<DecodedSleb128>(val.decoded_val) < 0;

  uint32_t temp_val = val.is_signed
                          ? static_cast<DecodedUleb128>(
                                std::get<DecodedSleb128>(val.decoded_val))
                          : std::get<DecodedUleb128>(val.decoded_val);
  absl::Status status = absl::UnknownError("Unknown error");

  for (int i = 0; i < val.coded_size; i++) {
    // Encode the next 7 bits.
    buffer.push_back(0x80 | (temp_val & 0x7f));
    temp_val >>= 7;  // Logical shift clears the upper 7 bits.

    if (decoded_is_negative) {
      // For a negative SLEB128 set upper 7 bits (arithmetic shift).
      temp_val |= 0xfe000000;
    }

    // The encoding could end when it is negative and is all 1s (-1) or positive
    // and all 0s.
    uint32_t end_value = 0;
    if (val.is_signed && (buffer.back() & 0x40))
      end_value = static_cast<uint32_t>(-1);

    if (temp_val == end_value) {
      status = absl::OkStatus();

      // Exit early if the `min_size_encoding` flag is set. Otherwise continue
      // until the encoding is `val->coded_size` bytes long.
      if (min_size_encoding) {
        break;
      }
    }
  }

  // Clear the final MSB to 0 to signal the end of the encoding.
  buffer.back() &= 0x7f;

  if (!status.ok() && !min_size_encoding) {
    auto error_message = absl::StrCat(
        (val.is_signed ? std::get<DecodedSleb128>(val.decoded_val)
                       : std::get<DecodedUleb128>(val.decoded_val)),
        " requires at least ", buffer.size(), " bytes. The caller requested it",
        " have a fixed size of ", val.coded_size);
    status = absl::InvalidArgumentError(error_message);
  }

  return status;
}

}  // namespace

// A trusted private constructor. The `Create()` functions ensure it is only
// called with expected arguments.
LebGenerator::LebGenerator(GenerationMode generation_mode, int8_t fixed_size)
    : generation_mode_(generation_mode), fixed_size_(fixed_size) {}

std::unique_ptr<LebGenerator> LebGenerator::Create(
    GenerationMode generation_mode, int8_t fixed_size) {
  switch (generation_mode) {
    case GenerationMode::kMinimum:
      return absl::WrapUnique(new LebGenerator(generation_mode, 0));
    case GenerationMode::kFixedSize:
      if (1 <= fixed_size && fixed_size <= kMaxLeb128Size) {
        return absl::WrapUnique(new LebGenerator(generation_mode, fixed_size));
      } else {
        ABSL_LOG(ERROR) << "Invalid fixed size: " << fixed_size;
        return nullptr;
      }
    default:
      ABSL_LOG(ERROR) << "Invalid generation mode: "
                      << absl::StrCat(generation_mode);
  }
  return nullptr;
}

absl::Status LebGenerator::Uleb128ToUint8Vector(
    DecodedUleb128 input, std::vector<uint8_t>& buffer) const {
  switch (generation_mode_) {
    case GenerationMode::kMinimum:
      return Leb128ToUint8Vector({input, kMaxLeb128Size, false},
                                 /*min_size_encoding=*/true, buffer);
    case GenerationMode::kFixedSize:
      return Leb128ToUint8Vector({input, fixed_size_, false},
                                 /*min_size_encoding=*/false, buffer);
    default:
      return absl::UnknownError("Unknown `generation_mode_`.");
  }
}

absl::Status LebGenerator::Sleb128ToUint8Vector(
    DecodedSleb128 input, std::vector<uint8_t>& buffer) const {
  switch (generation_mode_) {
    case GenerationMode::kMinimum:
      return Leb128ToUint8Vector({input, kMaxLeb128Size, true},
                                 /*min_size_encoding=*/true, buffer);
    case GenerationMode::kFixedSize:
      return Leb128ToUint8Vector({input, fixed_size_, true},
                                 /*min_size_encoding=*/false, buffer);
    default:
      return absl::UnknownError("Unknown `generation_mode_`.");
  }
}

}  // namespace iamf_tools
