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

#ifndef OBU_OBU_BASE_H_
#define OBU_OBU_BASE_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

class ObuBase {
 public:
  /*!\brief Constructor.
   *
   * \param header `ObuHeader` of the OBU.
   * \param obu_type `obu_type` of the OBU.
   */
  ObuBase(const ObuHeader& header, ObuType obu_type) : header_(header) {
    header_.obu_type = obu_type;
  }

  /*!\brief Constructor.
   *
   * \param obu_type `obu_type` of the OBU.
   */
  ObuBase(ObuType obu_type) : ObuBase(ObuHeader(), obu_type) {}
  ObuBase() = default;

  /*!\brief Destructor.*/
  virtual ~ObuBase() = 0;

  friend bool operator==(const ObuBase& lhs, const ObuBase& rhs) = default;

  /*!\brief Validates and writes an entire OBU to the buffer.
   *
   * \param final_wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific status on
   *         failure.
   */
  absl::Status ValidateAndWriteObu(WriteBitBuffer& final_wb) const;

  /*!\brief Prints logging information about the OBU.*/
  virtual void PrintObu() const = 0;

  ObuHeader header_;

  /*!\brief Bytes at the end of the OBU.
   *
   * Store any bytes signalled by `obu_size` but not read by
   * `ReadAndValidatePayloadDerived`. This helps comply with section 3.2
   * (https://aomediacodec.github.io/iamf/#obu-header-syntax):
   *
   * "The obu_size MAY be greater than the size needed to represent the OBU
   * syntax. Parsers SHOULD ignore bytes past the OBU syntax that they
   * recognize."
   *
   * Store the extra bytes so any components can forward them as needed.
   */
  std::vector<uint8_t> footer_;

 protected:
  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  virtual absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const = 0;

  /*!\brief Reads the entire OBU payload from the buffer.
   *
   * This includes reading in any extra bytes signalled by `obu_size`, but not
   * known to the `ReadAndValidatePayloadDerived` implementation.
   *
   * \param payload_size Size of the remaining payload to read.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid and at least as large
   *         as the claimed size. A specific status on failure.
   */
  absl::Status ReadAndValidatePayload(int64_t payload_size, ReadBitBuffer& rb);

  /*!\brief Prints logging information about the OBU Header.
   *
   * \param payload_size Payload size of the header.
   */
  void PrintHeader(int64_t payload_size) const;

 private:
  /*!\brief Reads the known OBU payload from the buffer.
   *
   * Implementations of this function MAY omit reading any bytes not known -
   * even if their presence is implied by `payload_size`. Implementations are
   * expected to read an integral number of bytes.
   *
   * \param payload_size Size of the obu payload in bytes. Useful to determine
   *        some fields whose presence or size are not directly signalled
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  virtual absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                                     ReadBitBuffer& rb) = 0;
};

}  // namespace iamf_tools

#endif  // OBU_OBU_BASE_H_
