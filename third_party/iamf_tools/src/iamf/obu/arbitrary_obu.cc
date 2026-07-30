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
#include "iamf/obu/arbitrary_obu.h"

#include <cstdint>
#include <list>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

absl::Status ArbitraryObu::WriteObusWithHook(
    InsertionHook insertion_hook, const std::list<ArbitraryObu>& arbitrary_obus,
    WriteBitBuffer& wb) {
  for (const auto& arbitrary_obu : arbitrary_obus) {
    if (arbitrary_obu.insertion_hook_ == insertion_hook) {
      RETURN_IF_NOT_OK(arbitrary_obu.ValidateAndWriteObu(wb));
    }
  }
  return absl::OkStatus();
}

absl::Status ArbitraryObu::ValidateAndWritePayload(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUint8Span(absl::MakeConstSpan(payload_)));
  // Usually we want to fail when an arbitrary OBU signals an invalid bitstream.
  // However, to create invalid test files we still want to insert them.
  MAYBE_RETURN_IF_NOT_OK(ValidateNotEqual(
      invalidates_bitstream_, true,
      absl::StrCat("Bitstream invalidated by an arbitrary OBU with obu_type= ",
                   header_.obu_type)));
  return absl::OkStatus();
}

absl::Status ArbitraryObu::ReadAndValidatePayloadDerived(
    int64_t /*payload_size*/, ReadBitBuffer& /*rb*/) {
  // TODO(b/329705373): Read in `payload_size` bytes to `payload_`.
  return absl::UnimplementedError(
      "ArbitraryOBU ReadAndValidatePayloadDerived not yet implemented.");
}

void ArbitraryObu::PrintObu() const {
  ABSL_LOG(INFO) << "Arbitrary OBU:";
  ABSL_LOG(INFO) << "// invalidates_bitstream= " << invalidates_bitstream_;
  ABSL_LOG(INFO) << "// insertion_hook= " << absl::StrCat(insertion_hook_);

  PrintHeader(static_cast<int64_t>(payload_.size()));

  ABSL_LOG(INFO) << "  payload omitted.";
}

}  // namespace iamf_tools
