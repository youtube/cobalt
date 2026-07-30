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
#ifndef CLI_AUDIO_FRAME_DECODER_H_
#define CLI_AUDIO_FRAME_DECODER_H_

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Decodes Audio Frame OBUs based on the associated codec.
 *
 * This class is related to the "Codec Decoder" as used in the IAMF
 * specification. "The Codec Decoder for each Audio Substream outputs the
 * decoded channels."
 *
 * This class manages the underlying codec decoders for all substreams. Codec
 * decoders may be stateful; this class manages a one-to-one mapping between
 * codec decoders and substream.
 *
 * Call `InitDecodersForSubstreams` with pairs of `SubstreamIdLabelsMap` and
 * `CodecConfigObu`. This typically will require one call per Audio Element OBU.
 *
 * Then call `Decode` repeatedly with an `AudioFrameWithData`. Because the
 * codec decoders are stateful, it is important to call `Decode` for a given
 * substream in chronological order. However, when the substreams differ, the
 * passed in order may be arbitrary.
 */
class AudioFrameDecoder {
 public:
  /*!\brief Constructor. */
  AudioFrameDecoder() = default;

  /*!\brief Initialize codec decoders for each substream.
   *
   * \param substream_id_to_labels Substreams and their associated labels to
   *        initialize. The number of channels is determined by the number of
   *        labels.
   * \param codec_config Codec Config OBU to use for all substreams.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitDecodersForSubstreams(
      const SubstreamIdLabelsMap& substream_id_to_labels,
      const CodecConfigObu& codec_config);

  /*!\brief Decodes an Audio Frame OBU.
   *
   * \param audio_frame Audio Frame OBU to decode in place.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Decode(AudioFrameWithData& audio_frame);

 private:
  // Map of substream IDs to the relevant decoder. This is necessary to process
  // streams with stateful decoders correctly.
  absl::flat_hash_map<DecodedUleb128, std::unique_ptr<DecoderBase>>
      substream_id_to_decoder_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_DECODER_H_
