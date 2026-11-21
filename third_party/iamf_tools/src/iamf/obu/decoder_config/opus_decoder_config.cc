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
#include "iamf/obu/decoder_config/opus_decoder_config.h"

#include <cstdint>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief The major version of Opus that is supported. */
const uint8_t kOpusMajorVersion = 0;

namespace {

absl::Status ValidateOpusMajorVersion(uint8_t opus_major_version) {
  // Opus Major version is in upper 4 bits. Higher versions may break backwards
  // compatibility and require software updates.
  if (opus_major_version > kOpusMajorVersion) {
    return absl::UnimplementedError("Unsupported Opus major version");
  }
  return absl::OkStatus();
}

// Validates the `OpusDecoderConfig`.
absl::Status ValidatePayload(const OpusDecoderConfig& decoder_config) {
  // Version 0 is invalid in the OPUS spec.
  if (decoder_config.version_ == 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid version= ", decoder_config.version_));
  }

  // OPUS Major version is in upper 4 bits. Higher versions may break backwards
  // compatibility and require software updates.
  const uint8_t decoder_config_major_version =
      (decoder_config.version_ & 0xf0) >> 4;
  MAYBE_RETURN_IF_NOT_OK(
      ValidateOpusMajorVersion(decoder_config_major_version));

  // Various below fields are fixed. The real value is determined from the Audio
  // Element OBU.
  MAYBE_RETURN_IF_NOT_OK(ValidateEqual(decoder_config.output_channel_count_,
                                       OpusDecoderConfig::kOutputChannelCount,
                                       "output_channel_count"));
  MAYBE_RETURN_IF_NOT_OK(ValidateEqual(decoder_config.output_gain_,
                                       OpusDecoderConfig::kOutputGain,
                                       "output_gain"));
  MAYBE_RETURN_IF_NOT_OK(ValidateEqual(decoder_config.mapping_family_,
                                       OpusDecoderConfig::kMappingFamily,
                                       "mapping_family"));

  return absl::OkStatus();
}

absl::Status ValidateAudioRollDistance(uint32_t num_samples_per_frame,
                                       int16_t audio_roll_distance) {
  const auto expected_roll_distance =
      OpusDecoderConfig::GetRequiredAudioRollDistance(num_samples_per_frame);
  if (!expected_roll_distance.ok()) {
    return expected_roll_distance.status();
  }

  return ValidateEqual(audio_roll_distance, *expected_roll_distance,
                       absl::StrCat("actual `audio_roll_distance` vs expected "
                                    "when `num_samples_per_frame= ",
                                    num_samples_per_frame));
}

}  // namespace

absl::StatusOr<int16_t> OpusDecoderConfig::GetRequiredAudioRollDistance(
    uint32_t num_samples_per_frame) {
  // Constant used to calculate legal audio roll distance for Opus.
  static constexpr int kOpusAudioRollDividend = 3840;

  // Prevent divide by 0. This is redundant as the spec ensures that
  // `num_samples_per_frame` SHALL NOT be 0.
  if (num_samples_per_frame == 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid num_samples_per_frame= ", num_samples_per_frame));
  }

  // Let R be the smallest integer greater than or equal to 3840 divided by the
  // frame size. The audio roll distance must be -R.
  int16_t expected_r =
      static_cast<int16_t>(kOpusAudioRollDividend / num_samples_per_frame);
  if (kOpusAudioRollDividend % num_samples_per_frame != 0) {
    expected_r += 1;
  }

  return -1 * expected_r;
}

absl::Status OpusDecoderConfig::ValidateAndWrite(uint32_t num_samples_per_frame,
                                                 int16_t audio_roll_distance,
                                                 WriteBitBuffer& wb) const {
  MAYBE_RETURN_IF_NOT_OK(
      ValidateAudioRollDistance(num_samples_per_frame, audio_roll_distance));
  RETURN_IF_NOT_OK(ValidatePayload(*this));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(version_, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(output_channel_count_, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(pre_skip_, 16));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(input_sample_rate_, 32));
  RETURN_IF_NOT_OK(wb.WriteSigned16(output_gain_));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(mapping_family_, 8));

  return absl::OkStatus();
}

absl::Status OpusDecoderConfig::ReadAndValidate(uint32_t num_samples_per_frame,
                                                int16_t audio_roll_distance,
                                                ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(
      ValidateAudioRollDistance(num_samples_per_frame, audio_roll_distance));

  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, version_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, output_channel_count_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(16, pre_skip_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, input_sample_rate_));
  RETURN_IF_NOT_OK(rb.ReadSigned16(output_gain_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, mapping_family_));

  RETURN_IF_NOT_OK(ValidatePayload(*this));
  return absl::OkStatus();
}

void OpusDecoderConfig::Print() const {
  ABSL_VLOG(1) << "    decoder_config(opus):";
  ABSL_VLOG(1) << "      version= " << absl::StrCat(version_);
  ABSL_VLOG(1) << "      output_channel_count= "
               << absl::StrCat(output_channel_count_);
  ABSL_VLOG(1) << "      pre_skip= " << pre_skip_;
  ABSL_VLOG(1) << "      input_sample_rate= " << input_sample_rate_;
  ABSL_VLOG(1) << "      output_gain= " << output_gain_;
  ABSL_VLOG(1) << "      mapping_family= " << absl::StrCat(mapping_family_);
}

}  // namespace iamf_tools
