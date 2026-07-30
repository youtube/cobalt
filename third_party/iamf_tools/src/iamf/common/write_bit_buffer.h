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
#ifndef COMMON_WRITE_BIT_BUFFER_H_
#define COMMON_WRITE_BIT_BUFFER_H_

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/common/leb_generator.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Holds a buffer and tracks the next bit to be written to. */
class WriteBitBuffer {
 public:
  /*!\brief Constructor.
   *
   * \param initial_capacity Initial capacity of the internal buffer in bytes.
   * \param leb_generator `LebGenerator` to use.
   */
  WriteBitBuffer(int64_t initial_capacity,
                 const LebGenerator& leb_generator = *LebGenerator::Create());

  /*!\brief Destructor.*/
  ~WriteBitBuffer() = default;

  /*!\brief Writes the lower `num_bits` of data to the write buffer.
   *
   * \param data Data to write.
   * \param num_bits Number of lower bits of the data to write. Maximum value of
   *        32.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 32` or if `data >= 2^(num_bits)`.
   *         `absl::UnknownError()` if the `wb->bit_offset` is negative.
   */
  absl::Status WriteUnsignedLiteral(uint32_t data, int num_bits);

  /*!\brief Writes the specified number of lower bits of data to the buffer.
   *
   * \param data Data to write.
   * \param num_bits Number of lower bits of the data to write. Maximum value of
   *        64.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 64` or if `data >= 2^(num_bits)`.
   *         `absl::UnknownError()` if the `wb->bit_offset` is
   *         negative.
   */
  absl::Status WriteUnsignedLiteral64(uint64_t data, int num_bits);

  /*!\brief Writes the signed 16 bit integer to the write buffer.
   *
   * \param data Data to write in standard two's complement form.
   * \return `absl::OkStatus()` on success. `absl::UnknownError()` if the
   *         `wb->bit_offset` is negative.
   */
  absl::Status WriteSigned16(int16_t data);

  /*!\brief Writes the signed 9 bit integer to the write buffer.
   *
   * \param data Data to write in standard two's complement form.
   * \return `absl::OkStatus()` on success. `absl::UnknownError()` if the
   *         `wb->bit_offset` is negative.
   */
  absl::Status WriteSigned9(int16_t data);

  /*!\brief Writes the signed 8 bit integer to the write buffer.
   *
   * \param data Data to write in standard two's complement form.
   * \return `absl::OkStatus()` on success. `absl::UnknownError()` if the
   *         `wb->bit_offset` is negative.
   */
  absl::Status WriteSigned8(int8_t data);

  /*!\brief Writes the boolean to the write buffer.
   *
   * \param data Data to write.
   * \return `absl::OkStatus()` on success. `absl::UnknownError()` if the
   *         `wb->bit_offset` is negative.
   */
  absl::Status WriteBoolean(bool data);

  /*!\brief Writes a null terminated string to the write buffer.
   *
   * \param data Data to write.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the string is not terminated within `kIamfMaxStringSize` bytes.
   *         Other specific statuses on failure.
   */
  absl::Status WriteString(const std::string& data);

  /*!\brief Writes a `absl::Span<const uint8_t>` to the write buffer.
   *
   * \param data Data to write.
   * \return `absl::OkStatus()` on success. `absl::UnknownError()` if the
   *         `wb->bit_offset` is negative.
   */
  absl::Status WriteUint8Span(absl::Span<const uint8_t> data);

  /*!\brief Writes a ULEB128 to the buffer using an implicit generator.
   *
   * \param data Data to write using the member `leb_generator_`.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the generation fails. Other specific statuses on failure.
   */
  absl::Status WriteUleb128(DecodedUleb128 data);

  /*!\brief Writes the expandable size according to ISO 14496-1.
   *
   * \param size_of_instance Size of the instance.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the generation fails. Other specific statuses on failure.
   */
  absl::Status WriteIso14496_1Expanded(uint32_t size_of_instance);

  /*!\brief Flushes and writes a byte-aligned buffer to a file.
   *
   * \param output_file File to write to. Or `std::nullopt` to omit writing.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the buffer is not byte-aligned. `absl::UnknownError()` if the write
   *         failed.
   */
  absl::Status FlushAndWriteToFile(std::optional<std::fstream>& output_file);

  /*!\brief Gets the offset in bits of the buffer.
   * \return Offset in bits of the write buffer.
   */
  int64_t bit_offset() const { return bit_offset_; }

  /*!\brief Returns a `const` pointer to the underlying buffer.
   *
   * If the buffer is not byte-aligned the last byte will be padded with zeroes.
   *
   * \return A `const` pointer to the underlying buffer.
   */
  const std::vector<uint8_t>& bit_buffer() const { return bit_buffer_; }

  /*!\brief Checks whether the current data in the buffer is byte-aligned.
   *
   * \return `true` when the current data in the buffer is byte-aligned.
   */
  bool IsByteAligned() const { return bit_offset_ % 8 == 0; }

  /*!\brief Resets the underlying buffer. */
  void Reset();

  LebGenerator leb_generator_;

 private:
  std::vector<uint8_t> bit_buffer_;
  int64_t bit_offset_;
};

}  // namespace iamf_tools

#endif  // COMMON_WRITE_BIT_BUFFER_H_
