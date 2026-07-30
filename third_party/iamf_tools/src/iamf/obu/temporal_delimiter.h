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
#ifndef OBU_TEMPORAL_DELIMITER_H_
#define OBU_TEMPORAL_DELIMITER_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

class TemporalDelimiterObu : public ObuBase {
 public:
  /*!\brief Creates a `TemporalDelimiterObu`,
   *
   * This is a factory method that creates a `TemporalDelimiterObu` from the
   * given `ObuHeader` and `ReadBitBuffer`. It is simple because
   * `TemporalDelimiterObu` has no payload.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param rb `ReadBitBuffer` where the `TemporalDelimiterObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return a `TemporalDelimiterObu` on success. A specific status on failure.
   */
  static absl::StatusOr<TemporalDelimiterObu> CreateFromBuffer(
      const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb) {
    TemporalDelimiterObu obu(header);
    RETURN_IF_NOT_OK(obu.ReadAndValidatePayload(payload_size, rb));
    return obu;
  }

  /*!\brief Constructor. */
  explicit TemporalDelimiterObu(ObuHeader header)
      : ObuBase(header, kObuIaTemporalDelimiter) {}

  /*!\brief Destructor. */
  ~TemporalDelimiterObu() override = default;

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override {
    // There is nothing to print for a Temporal Delimiter OBU.
  }

  // Temporal delimiter has no payload

 private:
  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` always.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& /*wb*/) const override {
    // There is nothing to write for a Temporal Delimiter OBU payload.
    return absl::OkStatus();
  }

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` always.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t /*payload_size*/,
                                             ReadBitBuffer& rb) override {
    return absl::OkStatus();
  }
};
}  // namespace iamf_tools

#endif  // OBU_TEMPORAL_DELIMITER_H_
