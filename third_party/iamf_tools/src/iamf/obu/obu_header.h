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
#ifndef OBU_OBU_HEADER_H_
#define OBU_OBU_HEADER_H_

#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A 5-bit enum for the type of OBU. */
enum ObuType : uint8_t {
  kObuIaCodecConfig = 0,
  kObuIaAudioElement = 1,
  kObuIaMixPresentation = 2,
  kObuIaParameterBlock = 3,
  kObuIaTemporalDelimiter = 4,
  kObuIaAudioFrame = 5,
  kObuIaAudioFrameId0 = 6,
  kObuIaAudioFrameId1 = 7,
  kObuIaAudioFrameId2 = 8,
  kObuIaAudioFrameId3 = 9,
  kObuIaAudioFrameId4 = 10,
  kObuIaAudioFrameId5 = 11,
  kObuIaAudioFrameId6 = 12,
  kObuIaAudioFrameId7 = 13,
  kObuIaAudioFrameId8 = 14,
  kObuIaAudioFrameId9 = 15,
  kObuIaAudioFrameId10 = 16,
  kObuIaAudioFrameId11 = 17,
  kObuIaAudioFrameId12 = 18,
  kObuIaAudioFrameId13 = 19,
  kObuIaAudioFrameId14 = 20,
  kObuIaAudioFrameId15 = 21,
  kObuIaAudioFrameId16 = 22,
  kObuIaAudioFrameId17 = 23,
  kObuIaMetadata = 24,
  kObuIaReserved25 = 25,
  kObuIaReserved26 = 26,
  kObuIaReserved27 = 27,
  kObuIaReserved28 = 28,
  kObuIaReserved29 = 29,
  kObuIaReserved30 = 30,
  kObuIaSequenceHeader = 31,
};

template <typename Sink>
void AbslStringify(Sink& sink, ObuType obu_type) {
  constexpr auto kObuTypeAndDebugString =
      std::to_array<std::pair<ObuType, absl::string_view>>({
          {kObuIaCodecConfig, "Codec Config"},
          {kObuIaAudioElement, "Audio Element"},
          {kObuIaMixPresentation, "Mix Presentation"},
          {kObuIaParameterBlock, "Parameter Block"},
          {kObuIaTemporalDelimiter, "Temporal Delimiter"},
          {kObuIaAudioFrame, "Audio Frame (explicit ID)"},
          {kObuIaAudioFrameId0, "Audio Frame ID 0"},
          {kObuIaAudioFrameId1, "Audio Frame ID 1"},
          {kObuIaAudioFrameId2, "Audio Frame ID 2"},
          {kObuIaAudioFrameId3, "Audio Frame ID 3"},
          {kObuIaAudioFrameId4, "Audio Frame ID 4"},
          {kObuIaAudioFrameId5, "Audio Frame ID 5"},
          {kObuIaAudioFrameId6, "Audio Frame ID 6"},
          {kObuIaAudioFrameId7, "Audio Frame ID 7"},
          {kObuIaAudioFrameId8, "Audio Frame ID 8"},
          {kObuIaAudioFrameId9, "Audio Frame ID 9"},
          {kObuIaAudioFrameId10, "Audio Frame ID 10"},
          {kObuIaAudioFrameId11, "Audio Frame ID 11"},
          {kObuIaAudioFrameId12, "Audio Frame ID 12"},
          {kObuIaAudioFrameId13, "Audio Frame ID 13"},
          {kObuIaAudioFrameId14, "Audio Frame ID 14"},
          {kObuIaAudioFrameId15, "Audio Frame ID 15"},
          {kObuIaAudioFrameId16, "Audio Frame ID 16"},
          {kObuIaAudioFrameId17, "Audio Frame ID 17"},
          {kObuIaMetadata, "Metadata"},
          {kObuIaReserved25, "Reserved 25"},
          {kObuIaReserved26, "Reserved 26"},
          {kObuIaReserved27, "Reserved 27"},
          {kObuIaReserved28, "Reserved 28"},
          {kObuIaReserved29, "Reserved 29"},
          {kObuIaReserved30, "Reserved 30"},
          {kObuIaSequenceHeader, "IA Sequence Header"},
      });
  static const auto kObuTypeToDebugString =
      BuildStaticMapFromPairs(kObuTypeAndDebugString);

  auto debug_string = LookupInMap(*kObuTypeToDebugString, obu_type, "ObuType");
  if (debug_string.ok()) {
    sink.Append(*debug_string);
  } else {
    sink.Append(absl::StrCat("Unknown ObuType(", obu_type, ")"));
  }
}

struct HeaderMetadata {
  ObuType obu_type;
  int64_t total_obu_size;
};

struct ObuHeader {
  friend bool operator==(const ObuHeader& lhs, const ObuHeader& rhs) = default;

  /*!\brief Validates and writes an `ObuHeader`.
   *
   * \param payload_serialized_size `payload_serialized_size` of the output OBU.
   *        The value MUST be able to be cast to `uint32_t` without losing data.
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *         if the fields are invalid or inconsistent or if writing the
   *         `Leb128` representation of `obu_size` fails.
   *         `absl::InvalidArgumentError()` if fields are set inconsistent with
   *         the IAMF specification or if the calculated `obu_size_` larger
   *         than IAMF limitations. Or a specific status if the write fails.
   */
  absl::Status ValidateAndWrite(int64_t payload_serialized_size,
                                WriteBitBuffer& wb) const;

  /*!\brief Validates and reads an `ObuHeader`.
   *
   * \param rb Buffer to read from.

   * \param output_payload_serialized_size `output_payload_serialized_size` Size
   *        of the payload of the OBU.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
   *         the fields are invalid or set in a manner that is inconsistent with
   *         the IAMF specification.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb,
                               int64_t& output_payload_serialized_size);

  /*!\brief Returns true if the optional fields flag is present and set.
   *
   * \return true if the optional fields flag is present and set, false if it is
   *         absent or if not relevant to the OBU type.
   */
  bool GetOptionalFieldsFlag() const;

  /*!\brief Returns true if the OBU is a key frame.
   *
   * Note: The underlying field is called `is_not_key_frame` in the IAMF
   * specification. This function returns the opposite of that field, for
   * temporal delimiter OBUs.
   *
   * \return true if the OBU is a key frame, false if it is not a key frame, or
   *         is not a temporal delimiter OBU.
   */
  bool GetIsKeyFrame() const;

  /*!\brief Returns true if the trimming status flag is present and set.
   *
   * \return true if the trimming status flag is present and set, false if it is
   *         absent or if not relevant to the OBU type.
   */
  bool GetObuTrimmingStatusFlag() const;

  /*!\brief Returns true if the extension header is present.
   *
   * \return true if the extension header is present, false otherwise.
   */
  bool GetExtensionHeaderFlag() const;

  /*!\brief Returns the size of the extension header in bytes.
   *
   * \return the size of the extension header in bytes, or 0 if it is absent.
   */
  DecodedUleb128 GetExtensionHeaderSize() const;

  /*!\brief Prints logging information about an `ObuHeader`.
   *
   * \param leb_generator `LebGenerator` to use when calculating `obu_size_`.
   * \param payload_serialized_size `payload_serialized_size` of the output OBU.
   *        The value MUST be able to be cast to `uint32_t` without losing data.
   */
  void Print(const LebGenerator& leb_generator,
             int64_t payload_serialized_size) const;

  /*!\brief Peeks the type and total OBU size from the bitstream.
   *
   * This function does not consume any data from the bitstream.
   *
   * \param rb Buffer to read from.
   * \return `HeaderMetadata` containing the OBU type and total OBU size if
   *         successful. Returns an absl::ResourceExhaustedError if there is not
   *         enough data to read the obu_type and obu_size. Returns other errors
   *         if the bitstream is invalid.
   */
  static absl::StatusOr<HeaderMetadata> PeekObuTypeAndTotalObuSize(
      ReadBitBuffer& rb);

  static bool IsTemporalUnitObuType(ObuType obu_type);

  ObuType obu_type;
  // `obu_size` is inserted automatically.
  bool obu_redundant_copy = false;
  bool type_specific_flag = false;
  /// `obu_extension_flag` is inserted automatically.
  DecodedUleb128 num_samples_to_trim_at_end = 0;
  DecodedUleb128 num_samples_to_trim_at_start = 0;
  // `extension_header_size` is inserted automatically.
  std::optional<std::vector<uint8_t>> extension_header_bytes;
};

}  // namespace iamf_tools

#endif  // OBU_OBU_HEADER_H_
