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
#ifndef OBU_AUDIO_FRAME_H_
#define OBU_AUDIO_FRAME_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
/*!\brief The Audio Frame OBU.
 *
 * The length and meaning of the `audio_frame` field depends on the associated
 * `CodecConfigObu` and `AudioElementObu`.
 *
 * For IAMF-OPUS the field represents an opus packet of RFC6716.
 * For IAMF-AAC-LC the field represents an raw_data_block() of the AAC spec.
 * For IAMF-FLAC the field represents an FRAME of the FLAC spec.
 * For IAMF-LPCM the field represents PCM samples. When more than one byte is
 * used to represent a PCM sample, the byte order (i.e. its endianness) is
 * indicated in `sample_format_flags` from the corresponding `CodecConfigObu`.
 */
class AudioFrameObu : public ObuBase {
 public:
  /*!\brief Creates an `AudioFrameObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param rb `ReadBitBuffer` where the `AudioFrameObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return a `AudioFrameObu` on success. A specific status on failure.
   */
  static absl::StatusOr<AudioFrameObu> CreateFromBuffer(const ObuHeader& header,
                                                        int64_t payload_size,
                                                        ReadBitBuffer& rb);

  /*!\brief Constructor.
   *
   * \param header `ObuHeader` of the OBU.
   * \param substream_id Substream ID.
   * \param audio_frame Audio frame.
   */
  AudioFrameObu(const ObuHeader& header, DecodedUleb128 substream_id,
                absl::Span<const uint8_t> audio_frame);

  /*!\brief Destructor.*/
  ~AudioFrameObu() = default;

  friend bool operator==(const AudioFrameObu& lhs,
                         const AudioFrameObu& rhs) = default;

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  /*!\brief Gets the substream ID of the OBU.
   * \return Substream ID.
   */
  DecodedUleb128 GetSubstreamId() const { return audio_substream_id_; }

  std::vector<uint8_t> audio_frame_;

 private:
  // This field is not serialized when in the range [0, 17].
  DecodedUleb128 audio_substream_id_;

  // Used only by the factory create function.
  explicit AudioFrameObu(const ObuHeader& header)
      : ObuBase(header, header.obu_type),
        audio_frame_({}),
        audio_substream_id_(0) {}
  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific status on
   *         failure.
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

#endif  // OBU_AUDIO_FRAME_H_
