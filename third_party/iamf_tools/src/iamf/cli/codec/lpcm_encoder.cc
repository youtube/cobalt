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
#include "iamf/cli/codec/lpcm_encoder.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"

namespace iamf_tools {

absl::Status LpcmEncoder::InitializeEncoder() {
  if (decoder_config_.sample_size_ % 8 != 0) {
    // `EncodeAudioFrame` assume the `bit_depth` is a multiple of 8.
    auto error_message = absl::StrCat(
        "Expected lpcm_decoder_config->sample_size to be a multiple of 8, but "
        "is instead: ",
        decoder_config_.sample_size_);
    return absl::InvalidArgumentError(error_message);
  }

  // `EncodeAudioFrame` assume there are only 2 possible values of
  // `sample_format_flags`. Even though the LPCM specification has this as an
  // extension point.
  if (decoder_config_.sample_format_flags_bitmask_ !=
          LpcmDecoderConfig::kLpcmBigEndian &&
      decoder_config_.sample_format_flags_bitmask_ !=
          LpcmDecoderConfig::kLpcmLittleEndian) {
    return absl::InvalidArgumentError("Unrecognized sample_format_flags");
  }

  ABSL_VLOG(1) << "  Configured LPCM encoder for " << num_samples_per_frame_
               << " samples of " << channel_count_.num_channels()
               << " channels as " << absl::StrCat(decoder_config_.sample_size_)
               << "-bit LPCM in "
               << (decoder_config_.sample_format_flags_bitmask_ &
                           LpcmDecoderConfig::kLpcmLittleEndian
                       ? "little"
                       : "big")
               << " endian";

  return absl::OkStatus();
}

absl::Status LpcmEncoder::EncodeAudioFrame(
    const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  RETURN_IF_NOT_OK(ValidateNotFinalized());
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));

  // Write the entire PCM frame the buffer. Nothing should be trimmed when
  // encoding the sample.
  auto& audio_frame = partial_audio_frame_with_data->obu.audio_frame_;
  const bool big_endian = !(decoder_config_.sample_format_flags_bitmask_ &
                            LpcmDecoderConfig::kLpcmLittleEndian);
  RETURN_IF_NOT_OK(WritePcmFrameToBuffer(samples, decoder_config_.sample_size_,
                                         big_endian, audio_frame));

  absl::MutexLock lock(mutex_);
  finalized_audio_frames_.emplace_back(
      std::move(*partial_audio_frame_with_data));

  return absl::OkStatus();
}

}  // namespace iamf_tools
