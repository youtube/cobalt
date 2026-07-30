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

#include "iamf/cli/codec/flac_decoder_stream_callbacks.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/types/span.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/types.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

namespace flac_callbacks {

namespace {
using absl::MakeConstSpan;
}  // namespace

FLAC__StreamDecoderReadStatus LibFlacReadCallback(
    const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t* bytes,
    void* client_data) {
  if (bytes == nullptr || buffer == nullptr || client_data == nullptr) {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }

  auto libflac_callback_data = static_cast<LibFlacCallbackData*>(client_data);
  // We are contracted to fill in up to the next `*bytes` bytes of the buffer.
  // If there is more data, then there will be a subsequent call to this
  // callback.
  const auto encoded_frame_slice = libflac_callback_data->GetNextSlice(*bytes);
  if (encoded_frame_slice.empty()) {
    // No more data to read.
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }

  for (size_t i = 0; i < encoded_frame_slice.size(); ++i) {
    buffer[i] = encoded_frame_slice[i];
  }
  *bytes = encoded_frame_slice.size();
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus LibFlacWriteCallback(
    const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
    const FLAC__int32* const buffer[], void* client_data) {
  auto* libflac_callback_data = static_cast<LibFlacCallbackData*>(client_data);
  if (libflac_callback_data->num_samples_per_channel_ <
      frame->header.blocksize) {
    ABSL_LOG(ERROR) << "Frame blocksize " << frame->header.blocksize
                    << " does not match expected number of samples per channel "
                    << libflac_callback_data->num_samples_per_channel_;
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  // Reject frames whose channel count does not match the expected count from
  // the IAMF codec config.  Without this check, a crafted FLAC frame could
  // claim more channels than the downstream pipeline is sized for.
  if (frame->header.channels != libflac_callback_data->num_channels_) {
    ABSL_LOG(ERROR) << "Frame channel count " << frame->header.channels
                    << " does not match expected "
                    << libflac_callback_data->num_channels_;
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  auto& decoded_samples = libflac_callback_data->decoded_frame_;
  decoded_samples.resize(frame->header.channels);
  for (uint32_t c = 0; c < frame->header.channels; ++c) {
    const FLAC__int32* const channel_buffer = buffer[c];
    auto& decoded_samples_for_channel = decoded_samples[c];

    // Zero-initialize a vector of the maximum number of samples per channel.
    // But only fill in based on the actual number of samples in the frame.
    decoded_samples_for_channel.resize(
        libflac_callback_data->num_samples_per_channel_, 0);
    auto status = ValidateInRange<uint32_t>(
        frame->header.bits_per_sample,
        {{FlacStreamInfoStrictConstraints::kMinBitsPerSample},
         {FlacStreamInfoStrictConstraints::kMaxBitsPerSample}},
        "bits_per_sample");
    if (!status.ok()) {
      ABSL_LOG(ERROR) << status;
      return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    for (uint32_t t = 0; t < frame->header.blocksize; ++t) {
      decoded_samples_for_channel[t] =
          Int32ToNormalizedFloatingPoint<InternalSampleType>(
              channel_buffer[t] << (32 - frame->header.bits_per_sample));
    }
  }
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void LibFlacErrorCallback(const FLAC__StreamDecoder* /*decoder*/,
                          FLAC__StreamDecoderErrorStatus status,
                          void* /*client_data*/) {
  switch (status) {
    case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
      ABSL_LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC";
      break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
      ABSL_LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER";
      break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
      ABSL_LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH";
      break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
      ABSL_LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM";
      break;
    default:
      ABSL_LOG(ERROR) << "Unknown FLAC__StreamDecoderErrorStatus= " << status;
      break;
  }
}

void LibFlacCallbackData::SetEncodedFrame(
    absl::Span<const uint8_t> raw_encoded_frame) {
  // Cache the frame, and reset the bookkeeping index.
  encoded_frame_ =
      std::vector<uint8_t>(raw_encoded_frame.begin(), raw_encoded_frame.end());
  next_byte_index_ = 0;
}

absl::Span<const uint8_t> LibFlacCallbackData::GetNextSlice(size_t chunk_size) {
  // Ok, the buffer is exhausted. Return an empty span.
  if (next_byte_index_ >= encoded_frame_.size()) {
    return {};
  }

  // Grab the next slice, advance the bookkeeping index; effectively consecutive
  // calls stride over the source frame.
  auto next_slice =
      MakeConstSpan(encoded_frame_).subspan(next_byte_index_, chunk_size);
  next_byte_index_ += next_slice.size();
  return next_slice;
}

}  // namespace flac_callbacks

}  // namespace iamf_tools
