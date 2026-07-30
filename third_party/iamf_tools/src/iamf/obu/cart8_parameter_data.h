/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_CART8_PARAMETER_DATA_H_
#define OBU_CART8_PARAMETER_DATA_H_

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct Cart8ParameterData : public ParameterData {
  Cart8ParameterData() = default;

  /*!\brief Overridden destructor.
   */
  ~Cart8ParameterData() override = default;

  bool friend operator==(const Cart8ParameterData& lhs,
                         const Cart8ParameterData& rhs) = default;

  /*!\brief Reads and validates a `Cart8ParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the cart8 parameter data.
   */
  void Print() const override;
};
}  // namespace iamf_tools

#endif  // OBU_CART8_PARAMETER_DATA_H_