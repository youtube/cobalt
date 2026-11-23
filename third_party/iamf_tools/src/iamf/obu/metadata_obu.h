/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef OBU_METADATA_OBU_H_
#define OBU_METADATA_OBU_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

enum MetadataType : DecodedUleb128 {
  kMetadataTypeReserved = 0,
  kMetadataTypeITUT_T35 = 1,
  kMetadataTypeIamfTags = 2,
  // Values in the range of [3, (1 << 32) - 1] are reserved.
  kMetadataTypeReservedStart = 3,
  kMetadataTypeReservedEnd = std::numeric_limits<DecodedUleb128>::max()
};

struct MetadataITUTT35 {
  uint8_t itu_t_t35_country_code;
  std::optional<uint8_t> itu_t_t35_country_code_extension_byte;
  std::vector<uint8_t> itu_t_t35_payload_bytes;
};

struct MetadataIamfTags {
  struct Tag {
    std::string tag_name;
    std::string tag_value;
  };
  std::vector<Tag> tags;
};

using MetadataVariant = std::variant<MetadataITUTT35, MetadataIamfTags>;

class MetadataObu : public ObuBase {
 public:
  /*!\brief Creates a `MetadataObu`.
   *
   * \param header `ObuHeader` of the OBU.
   * \param metadata_variant `MetadataVariant` of the OBU.
   * \return `MetadataObu` on success. A specific status on failure.
   */
  static MetadataObu Create(const ObuHeader& header,
                            MetadataVariant metadata_variant);

  /*!\brief Creates a `MetadataObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param rb `ReadBitBuffer` where the `MetadataObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return `MetadataObu` on success. A specific status on failure.
   */
  static absl::StatusOr<MetadataObu> CreateFromBuffer(const ObuHeader& header,
                                                      int64_t payload_size,
                                                      ReadBitBuffer& rb);

  MetadataType GetMetadataType() const { return metadata_type_; }

  const MetadataVariant& GetMetadataVariant() const {
    return metadata_variant_;
  }

  /*!\brief Destructor. */
  ~MetadataObu() override = default;

  friend bool operator==(const MetadataObu& lhs,
                         const MetadataObu& rhs) = default;

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

 private:
  MetadataType metadata_type_;
  MetadataVariant metadata_variant_;

  // Used only by the factory create function.
  explicit MetadataObu(const ObuHeader& header)
      : ObuBase(header, kObuIaMetadata) {}

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

#endif  // OBU_METADATA_OBU_H_
