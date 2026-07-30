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

#include "iamf/obu/obu_base.h"

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

ObuBase::~ObuBase() {}

absl::Status ObuBase::ValidateAndWriteObu(WriteBitBuffer& final_wb) const {
  // Allocate a temporary buffer big enough for most OBUs to assist writing, but
  // make it resizable so it can be expanded for large OBUs.
  static const int64_t kBufferSize = 1024;
  WriteBitBuffer temp_wb(kBufferSize, final_wb.leb_generator_);

  // Write the payload to a temporary buffer using the virtual function.
  RETURN_IF_NOT_OK(ValidateAndWritePayload(temp_wb));
  // Write the footer to the temporary buffer.
  RETURN_IF_NOT_OK(temp_wb.WriteUint8Span(absl::MakeConstSpan(footer_)));
  if (!temp_wb.IsByteAligned()) {
    // The header stores the size of the OBU in bytes.
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected the OBU payload to be byte-aligned: ", temp_wb.bit_offset()));
  }

  // Write the header now that the payload size is known.
  const int64_t payload_size_bytes = temp_wb.bit_buffer().size();

  RETURN_IF_NOT_OK(header_.ValidateAndWrite(payload_size_bytes, final_wb));

  const int64_t expected_end_payload =
      final_wb.bit_offset() + payload_size_bytes * 8;

  // Copy over the payload into the final write buffer.
  RETURN_IF_NOT_OK(
      final_wb.WriteUint8Span(absl::MakeConstSpan(temp_wb.bit_buffer())));

  // Validate the write buffer is at the expected location expected after
  // writing the payload.
  if (expected_end_payload != final_wb.bit_offset()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected end_payload: ", expected_end_payload,
        " to be equal to write buffer bit offset: ", final_wb.bit_offset()));
  }

  return absl::OkStatus();
}

absl::Status ObuBase::ReadAndValidatePayload(int64_t payload_size_bytes,
                                             ReadBitBuffer& rb) {
  const int64_t expected_final_position = rb.Tell() + (payload_size_bytes * 8);

  // Read the known portion of the payload.
  RETURN_IF_NOT_OK(ReadAndValidatePayloadDerived(payload_size_bytes, rb));
  const int64_t final_position = rb.Tell();

  // Read the remaining of the payload (if any) into the footer.
  if (expected_final_position == final_position) {
    return absl::OkStatus();
  } else if (expected_final_position > final_position) {
    if ((expected_final_position - final_position) % 8 != 0) {
      return absl::InvalidArgumentError(
          absl::StrCat("Expected `ReadAndValidatePayloadDerived` to write a "
                       "multiple of 8 bits for obu_type=",
                       header_.obu_type));
    }
    const int64_t num_bytes_to_read =
        (expected_final_position - final_position) / 8;
    footer_.resize(num_bytes_to_read);
    return rb.ReadUint8Span(absl::MakeSpan(footer_));
  }
  // The dispatched function read past the end of the payload. Something could
  // be inconsistent between the parsing logic and the claimed OBU size.
  return absl::InvalidArgumentError(absl::StrCat(
      "Read beyond the end of the OBU for obu_type=", header_.obu_type));
}

void ObuBase::PrintHeader(int64_t payload_size_bytes) const {
  // TODO(b/299480731): Use the correct `LebGenerator` when printing OBU
  //                    headers.
  header_.Print(*LebGenerator::Create(), payload_size_bytes);
}

}  // namespace iamf_tools
