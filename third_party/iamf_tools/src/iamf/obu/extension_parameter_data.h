/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_EXTNESION_PARAMETER_DATA_H_
#define OBU_EXTNESION_PARAMETER_DATA_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct ExtensionParameterData : public ParameterData {
  /*!\brief Constructor.
   *
   * \param input_parameter_data_bytes Input bytes of the parameter data.
   */
  explicit ExtensionParameterData(
      absl::Span<const uint8_t> input_parameter_data_bytes)
      : ParameterData(),
        parameter_data_bytes(input_parameter_data_bytes.begin(),
                             input_parameter_data_bytes.end()) {}
  ExtensionParameterData() = default;

  /*!\brief Overridden destructor.*/
  ~ExtensionParameterData() override = default;

  /*!\brief Reads and validates the `ExtensionParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()`. A specific error code on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the extension parameter data.
   */
  void Print() const override;

  // `parameter_data_size` is inferred from the size of this vector.
  std::vector<uint8_t> parameter_data_bytes;
};

}  // namespace iamf_tools

#endif  // OBU_EXTNESION_PARAMETER_DATA_H_
