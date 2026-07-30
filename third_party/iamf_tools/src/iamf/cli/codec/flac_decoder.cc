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

#include "iamf/cli/codec/flac_decoder.h"

#include <cstdint>
#include <memory>

#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/flac_decoder_stream_callbacks.h"
#include "iamf/obu/substream_channel_count.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

absl::StatusOr<std::unique_ptr<DecoderBase>> FlacDecoder::Create(
    SubstreamChannelCount channel_count, uint32_t num_samples_per_frame) {
  FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
  if (decoder == nullptr) {
    return absl::InternalError("Failed to create FLAC stream decoder.");
  }

  auto flac_decoder = absl::WrapUnique(
      new FlacDecoder(channel_count, num_samples_per_frame, decoder));

  FLAC__stream_decoder_init_stream(
      decoder, flac_callbacks::LibFlacReadCallback, /*seek_callback=*/nullptr,
      /*tell_callback=*/nullptr, /*length_callback=*/nullptr,
      /*eof_callback=*/nullptr, flac_callbacks::LibFlacWriteCallback,
      /*metadata_callback=*/nullptr, flac_callbacks::LibFlacErrorCallback,
      static_cast<void*>(flac_decoder->callback_data_.get()));

  return flac_decoder;
}

FlacDecoder::~FlacDecoder() {
  // The factory function prevents `decoder_` from ever being null.
  ABSL_CHECK_NE(decoder_, nullptr);
  FLAC__stream_decoder_delete(decoder_);
}

absl::Status FlacDecoder::Finalize() {
  // Signal to `libflac` the decoder is finished.
  if (!FLAC__stream_decoder_finish(decoder_)) {
    return absl::InternalError("Failed to finalize Flac stream decoder.");
  }
  return absl::OkStatus();
}

absl::Status FlacDecoder::DecodeAudioFrame(
    absl::Span<const uint8_t> encoded_frame) {
  // Set the encoded frame to be decoded; the libflac decoder will copy the
  // data using LibFlacReadCallback.
  callback_data_->SetEncodedFrame(encoded_frame);
  if (!FLAC__stream_decoder_process_single(decoder_)) {
    // More specific error information is logged in LibFlacErrorCallback.
    return absl::InternalError("Failed to decode FLAC frame.");
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
