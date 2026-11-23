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
#include "iamf/obu/ia_sequence_header.h"

#include <cstdint>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

namespace {

/*!\brief The spec requires the `ia_code` field to be: "iamf".
 *
 * This four-character code (4CC) is used to determine the start of an IA
 * Sequence.
 */
constexpr uint32_t kIaCode = 0x69616d66;  // "iamf".

absl::Status ValidateProfileVersion(ProfileVersion profile_version) {
  switch (profile_version) {
    case ProfileVersion::kIamfSimpleProfile:
    case ProfileVersion::kIamfBaseProfile:
    case ProfileVersion::kIamfBaseEnhancedProfile:
    case ProfileVersion::kIamfBaseAdvancedProfile:
    case ProfileVersion::kIamfAdvanced1Profile:
    case ProfileVersion::kIamfAdvanced2Profile:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unexpected profile_version=", profile_version));
  }
}

}  // namespace

absl::Status IASequenceHeaderObu::Validate() const {
  MAYBE_RETURN_IF_NOT_OK(ValidateProfileVersion(primary_profile_));
  return absl::OkStatus();
}

absl::StatusOr<IASequenceHeaderObu> IASequenceHeaderObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb) {
  IASequenceHeaderObu ia_sequence_header_obu(header);
  RETURN_IF_NOT_OK(
      ia_sequence_header_obu.ReadAndValidatePayload(payload_size, rb));
  return ia_sequence_header_obu;
}

void IASequenceHeaderObu::PrintObu() const {
  ABSL_LOG(INFO) << "IA Sequence Header OBU:";
  ABSL_LOG(INFO) << "  ia_code= " << absl::StrCat(kIaCode);
  ABSL_LOG(INFO) << "  primary_profile= " << absl::StrCat(primary_profile_);
  ABSL_LOG(INFO) << "  additional_profile= "
                 << absl::StrCat(additional_profile_);
}

absl::Status IASequenceHeaderObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(Validate());
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(kIaCode, 32));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(static_cast<uint32_t>(primary_profile_), 8));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(static_cast<uint32_t>(additional_profile_), 8));

  return absl::OkStatus();
}

absl::Status IASequenceHeaderObu::ReadAndValidatePayloadDerived(
    int64_t /*payload_size*/, ReadBitBuffer& rb) {
  uint32_t ia_code;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, ia_code));
  // If the IA Code is any other value then the data may not actually be an IA
  // Sequence, or it may mean the data is corrupt / misaligned.
  RETURN_IF_NOT_OK(ValidateEqual(ia_code, kIaCode, "ia_code"));
  uint8_t primary_profile;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, primary_profile));
  primary_profile_ = static_cast<ProfileVersion>(primary_profile);
  uint8_t additional_profile;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, additional_profile));
  additional_profile_ = static_cast<ProfileVersion>(additional_profile);
  RETURN_IF_NOT_OK(Validate());
  return absl::OkStatus();
}

}  // namespace iamf_tools
