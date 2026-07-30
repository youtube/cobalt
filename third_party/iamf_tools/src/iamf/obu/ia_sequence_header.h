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
#ifndef OBU_IA_SEQUENCE_HEADER_H_
#define OBU_IA_SEQUENCE_HEADER_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

/*!\brief An 8-bit enum for the profile. */
enum class ProfileVersion : uint8_t {
  kIamfSimpleProfile = 0,
  kIamfBaseProfile = 1,
  kIamfBaseEnhancedProfile = 2,
  kIamfBaseAdvancedProfile = 3,
  kIamfAdvanced1Profile = 4,
  kIamfAdvanced2Profile = 5,
  kIamfReserved255Profile = 255
};

class IASequenceHeaderObu : public ObuBase {
 public:
  /*!\brief Constructor. */
  IASequenceHeaderObu(const ObuHeader& header, ProfileVersion primary_profile,
                      ProfileVersion additional_profile)
      : ObuBase(header, kObuIaSequenceHeader),
        primary_profile_(primary_profile),
        additional_profile_(additional_profile) {}

  IASequenceHeaderObu() = default;

  /*!\brief Creates a `IASequenceHeaderObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param rb `ReadBitBuffer` where the `IASequenceHeaderObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return `IASequenceHeaderObu` on success. A specific status on failure.
   */
  static absl::StatusOr<IASequenceHeaderObu> CreateFromBuffer(
      const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb);

  /*!\brief Destructor. */
  ~IASequenceHeaderObu() override = default;

  friend bool operator==(const IASequenceHeaderObu& lhs,
                         const IASequenceHeaderObu& rhs) = default;

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  /*!\brief Validates the OBU.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate() const;

  /*!\brief Gets the primary profile of the OBU.
   *
   * \return primary profile of the OBU.
   */
  ProfileVersion GetPrimaryProfile() const { return primary_profile_; }

  /*!\brief Gets the additional profile of the OBU.
   *
   * \return additional profile of the OBU.
   */
  ProfileVersion GetAdditionalProfile() const { return additional_profile_; }

 private:
  // `ia_code` is inserted automatically.
  ProfileVersion primary_profile_;
  ProfileVersion additional_profile_;

  // Used only by the factory create function.
  explicit IASequenceHeaderObu(const ObuHeader& header)
      : ObuBase(header, kObuIaSequenceHeader),
        primary_profile_(ProfileVersion::kIamfBaseProfile),
        additional_profile_(ProfileVersion::kIamfBaseProfile) {}

  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if OBU is valid. A specific status on
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param payload_size Size of the obu payload in bytes.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                             ReadBitBuffer& rb) override;
};
}  // namespace iamf_tools

#endif  // OBU_IA_SEQUENCE_HEADER_H_
