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
#include "iamf/obu/codec_config.h"

#include <cstdint>
#include <utility>
#include <variant>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status ValidateNumSamplesPerFrame(uint32_t num_samples_per_frame) {
  // The spec only explicitly forbids a frame size of zero, with no upper bound.
  //
  // For practical purpsoes, it is reasonable to restrict the upper bound to
  // prevent excessive memory usage. Underlyng codecs or their implementations
  // usually have conservative limits.
  //
  // - AAC: 1024 samples per frame.
  // - FLAC: 65,535 samples per frame.
  // - Opus: 60 ms frames (at 48 kHz).
  //
  // LPCM has no limit under the IAMF spec. Here we pick a large limit that
  // would support frames up to 1 second at 96 kHz. Which is both a longer
  // duration and a higher sample rate than we would typically expect.
  constexpr uint32_t kMinPracticalFrameSize = 1;
  return ValidateInRange(
      num_samples_per_frame,
      {kMinPracticalFrameSize, CodecConfigObu::kMaxPracticalFrameSize},
      "Number of samples per frame");
}

absl::Status OverrideAudioRollDistance(CodecConfig::CodecId codec_id,
                                       uint32_t num_samples_per_frame,
                                       int16_t& output_audio_roll_distance) {
  switch (codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdOpus: {
      auto audio_roll_distance =
          OpusDecoderConfig::GetRequiredAudioRollDistance(
              num_samples_per_frame);
      if (!audio_roll_distance.ok()) {
        return audio_roll_distance.status();
      }
      output_audio_roll_distance = *audio_roll_distance;
      return absl::OkStatus();
    }
    case kCodecIdLpcm:
      output_audio_roll_distance =
          LpcmDecoderConfig::GetRequiredAudioRollDistance();
      return absl::OkStatus();
    case kCodecIdFlac:
      output_audio_roll_distance =
          FlacDecoderConfig::GetRequiredAudioRollDistance();
      return absl::OkStatus();
    case kCodecIdAacLc:
      output_audio_roll_distance =
          AacDecoderConfig::GetRequiredAudioRollDistance();
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_id));
  }
}

absl::Status SetSampleRatesAndBitDepths(
    uint32_t codec_id, const DecoderConfig& decoder_config,
    uint32_t& output_sample_rate, uint32_t& input_sample_rate,
    uint8_t& bit_depth_to_measure_loudness) {
  switch (codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdOpus: {
      const auto& opus_decoder_config =
          std::get<OpusDecoderConfig>(decoder_config);
      output_sample_rate = opus_decoder_config.GetOutputSampleRate();
      input_sample_rate = opus_decoder_config.GetInputSampleRate();
      bit_depth_to_measure_loudness =
          OpusDecoderConfig::GetBitDepthToMeasureLoudness();
      break;
    }
    case kCodecIdLpcm: {
      const auto& lpcm_decoder_config =
          std::get<LpcmDecoderConfig>(decoder_config);
      RETURN_IF_NOT_OK(
          lpcm_decoder_config.GetOutputSampleRate(output_sample_rate));
      input_sample_rate = output_sample_rate;
      RETURN_IF_NOT_OK(lpcm_decoder_config.GetBitDepthToMeasureLoudness(
          bit_depth_to_measure_loudness));
      break;
    }
    case kCodecIdAacLc:
      RETURN_IF_NOT_OK(std::get<AacDecoderConfig>(decoder_config)
                           .GetOutputSampleRate(output_sample_rate));
      input_sample_rate = output_sample_rate;
      bit_depth_to_measure_loudness =
          AacDecoderConfig::GetBitDepthToMeasureLoudness();
      break;
    case kCodecIdFlac: {
      const auto& flac_decoder_config =
          std::get<FlacDecoderConfig>(decoder_config);
      RETURN_IF_NOT_OK(
          flac_decoder_config.GetOutputSampleRate(output_sample_rate));
      input_sample_rate = output_sample_rate;
      RETURN_IF_NOT_OK(flac_decoder_config.GetBitDepthToMeasureLoudness(
          bit_depth_to_measure_loudness));
      break;
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_id));
  }

  // For safety, check that this is never zero. But usually the decoder configs
  // themselves would have rejected zero.
  return ValidateNotEqual(output_sample_rate, uint32_t{0}, "Sample rate");
}

absl::Status InitializeCodecConfigAndMetadata(
    bool automatically_override_roll_distance, CodecConfig& codec_config,
    uint32_t& output_sample_rate, uint32_t& input_sample_rate,
    uint8_t& bit_depth_to_measure_loudness) {
  RETURN_IF_NOT_OK(SetSampleRatesAndBitDepths(
      codec_config.codec_id, codec_config.decoder_config, output_sample_rate,
      input_sample_rate, bit_depth_to_measure_loudness));

  if (automatically_override_roll_distance) {
    RETURN_IF_NOT_OK(OverrideAudioRollDistance(
        codec_config.codec_id, codec_config.num_samples_per_frame,
        codec_config.audio_roll_distance));
  }

  return absl::OkStatus();
}

absl::Status ValidateAndWriteDecoderConfig(const CodecConfig& codec_config,
                                           WriteBitBuffer& wb) {
  // Write the `decoder_config` struct portion. This is codec specific.
  const int16_t audio_roll_distance = codec_config.audio_roll_distance;
  const uint32_t num_samples_per_frame = codec_config.num_samples_per_frame;
  switch (codec_config.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdOpus:
      return std::get<OpusDecoderConfig>(codec_config.decoder_config)
          .ValidateAndWrite(num_samples_per_frame, audio_roll_distance, wb);
    case kCodecIdLpcm:
      return std::get<LpcmDecoderConfig>(codec_config.decoder_config)
          .ValidateAndWrite(audio_roll_distance, wb);
    case kCodecIdAacLc:
      return std::get<AacDecoderConfig>(codec_config.decoder_config)
          .ValidateAndWrite(audio_roll_distance, wb);
    case kCodecIdFlac:
      return std::get<FlacDecoderConfig>(codec_config.decoder_config)
          .ValidateAndWrite(num_samples_per_frame, audio_roll_distance, wb);
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_config.codec_id));
  }
}

absl::Status ReadAndValidateDecoderConfig(ReadBitBuffer& rb,
                                          CodecConfig& codec_config) {
  const int16_t audio_roll_distance = codec_config.audio_roll_distance;
  const uint32_t num_samples_per_frame = codec_config.num_samples_per_frame;
  // Read the `decoder_config` struct portion. This is codec specific.
  switch (codec_config.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdOpus: {
      OpusDecoderConfig opus_decoder_config;
      RETURN_IF_NOT_OK(opus_decoder_config.ReadAndValidate(
          num_samples_per_frame, audio_roll_distance, rb));
      codec_config.decoder_config = opus_decoder_config;
      return absl::OkStatus();
    }
    case kCodecIdLpcm: {
      LpcmDecoderConfig lpcm_decoder_config;
      RETURN_IF_NOT_OK(
          lpcm_decoder_config.ReadAndValidate(audio_roll_distance, rb));
      codec_config.decoder_config = lpcm_decoder_config;
      return absl::OkStatus();
    }
    case kCodecIdAacLc: {
      AacDecoderConfig aac_decoder_config;
      RETURN_IF_NOT_OK(
          aac_decoder_config.ReadAndValidate(audio_roll_distance, rb));
      codec_config.decoder_config = aac_decoder_config;
      return absl::OkStatus();
    }
    case kCodecIdFlac: {
      FlacDecoderConfig flac_decoder_config;
      RETURN_IF_NOT_OK(flac_decoder_config.ReadAndValidate(
          num_samples_per_frame, audio_roll_distance, rb));
      codec_config.decoder_config = flac_decoder_config;
      return absl::OkStatus();
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_config.codec_id));
  }
}

}  // namespace

absl::StatusOr<CodecConfigObu> CodecConfigObu::Create(
    const ObuHeader& header, DecodedUleb128 codec_config_id,
    const CodecConfig& input_codec_config,
    bool automatically_override_roll_distance) {
  RETURN_IF_NOT_OK(
      ValidateNumSamplesPerFrame(input_codec_config.num_samples_per_frame));
  // Copy the codec config, it may be modified to correct the roll distance.
  CodecConfig codec_config = input_codec_config;
  uint32_t output_sample_rate = 0;
  uint32_t input_sample_rate = 0;
  uint8_t bit_depth_to_measure_loudness = 0;
  RETURN_IF_NOT_OK(InitializeCodecConfigAndMetadata(
      automatically_override_roll_distance, codec_config, output_sample_rate,
      input_sample_rate, bit_depth_to_measure_loudness));

  auto obu =
      CodecConfigObu(header, codec_config_id, codec_config, output_sample_rate,
                     input_sample_rate, bit_depth_to_measure_loudness);
  obu.PrintObu();
  return obu;
}

CodecConfigObu::CodecConfigObu(const ObuHeader& header,
                               const DecodedUleb128 codec_config_id,
                               const CodecConfig& codec_config,
                               uint32_t output_sample_rate,
                               uint32_t input_sample_rate,
                               uint8_t bit_depth_to_measure_loudness)
    : ObuBase(header, kObuIaCodecConfig),
      codec_config_id_(codec_config_id),
      codec_config_(std::move(codec_config)),
      input_sample_rate_(input_sample_rate),
      output_sample_rate_(output_sample_rate),
      bit_depth_to_measure_loudness_(bit_depth_to_measure_loudness) {}

absl::StatusOr<CodecConfigObu> CodecConfigObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb) {
  CodecConfigObu codec_config_obu(header);
  RETURN_IF_NOT_OK(codec_config_obu.ReadAndValidatePayload(payload_size, rb));

  // Initialize the statistics about the codec config.
  RETURN_IF_NOT_OK(InitializeCodecConfigAndMetadata(
      /*automatically_override_roll_distance=*/false,
      codec_config_obu.codec_config_, codec_config_obu.output_sample_rate_,
      codec_config_obu.input_sample_rate_,
      codec_config_obu.bit_depth_to_measure_loudness_));
  codec_config_obu.PrintObu();
  return codec_config_obu;
}

absl::Status CodecConfigObu::ValidateAndWritePayload(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUleb128(codec_config_id_));

  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(codec_config_.codec_id, 32));
  RETURN_IF_NOT_OK(
      ValidateNumSamplesPerFrame(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(wb.WriteUleb128(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(wb.WriteSigned16(codec_config_.audio_roll_distance));

  // Write the `decoder_config_`. This is codec specific.
  RETURN_IF_NOT_OK(ValidateAndWriteDecoderConfig(codec_config_, wb));

  return absl::OkStatus();
}

absl::Status CodecConfigObu::ReadAndValidatePayloadDerived(
    int64_t /*payload_size*/, ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadULeb128(codec_config_id_));
  uint64_t codec_id;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, codec_id));
  codec_config_.codec_id = static_cast<CodecConfig::CodecId>(codec_id);
  RETURN_IF_NOT_OK(rb.ReadULeb128(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(
      ValidateNumSamplesPerFrame(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(rb.ReadSigned16(codec_config_.audio_roll_distance));

  // Read the `decoder_config_`. This is codec specific.
  RETURN_IF_NOT_OK(ReadAndValidateDecoderConfig(rb, codec_config_));
  return absl::OkStatus();
}

void CodecConfigObu::PrintObu() const {
  ABSL_VLOG(1) << "Codec Config OBU:";
  ABSL_VLOG(1) << "  codec_config_id= " << codec_config_id_;
  ABSL_VLOG(1) << "  codec_config:";
  ABSL_VLOG(1) << "    codec_id= " << codec_config_.codec_id;
  ABSL_VLOG(1) << "    num_samples_per_frame= " << GetNumSamplesPerFrame();
  ABSL_VLOG(1) << "    audio_roll_distance= "
               << codec_config_.audio_roll_distance;

  // Print the `decoder_config_`. This is codec specific.
  switch (codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
      std::get<LpcmDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    case kCodecIdOpus:
      std::get<OpusDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    case kCodecIdFlac:
      std::get<FlacDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    case kCodecIdAacLc:
      std::get<AacDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    default:
      ABSL_LOG(ERROR) << "Unknown codec_id: " << codec_config_.codec_id;
      break;
  }

  ABSL_VLOG(1) << "  // input_sample_rate_= " << input_sample_rate_;
  ABSL_VLOG(1) << "  // output_sample_rate_= " << output_sample_rate_;
  ABSL_VLOG(1) << "  // bit_depth_to_measure_loudness_= "
               << absl::StrCat(bit_depth_to_measure_loudness_);
}

absl::Status CodecConfigObu::SetCodecDelay(uint16_t codec_delay) {
  switch (codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
    case kCodecIdFlac:
    case kCodecIdAacLc:
      // Ok the `decoder_config` does not have a field for codec delay.
      return absl::OkStatus();
    case kCodecIdOpus: {
      OpusDecoderConfig* opus_decoder_config =
          std::get_if<OpusDecoderConfig>(&codec_config_.decoder_config);
      if (opus_decoder_config == nullptr) {
        return absl::InvalidArgumentError(
            "OpusDecoderConfig is not set in CodecConfig.");
      }
      opus_decoder_config->pre_skip_ = codec_delay;
      return absl::OkStatus();
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_config_.codec_id));
  }
}

bool CodecConfigObu::IsLossless() const {
  using enum CodecConfig::CodecId;
  return codec_config_.codec_id == kCodecIdFlac ||
         codec_config_.codec_id == kCodecIdLpcm;
}

}  // namespace iamf_tools
