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

#ifndef CLI_CODEC_FLAC_DECODER_STREAM_CALLBACKS_H_
#define CLI_CODEC_FLAC_DECODER_STREAM_CALLBACKS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "iamf/obu/types.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

namespace flac_callbacks {

/*!\brief Data to be passed to the libflac decoder callbacks.
 *
 * The callback functions work by reading and writing to this struct.
 */
class LibFlacCallbackData {
 public:
  /*!\brief Constructor.
   *
   * \param num_samples_per_channel Number of samples per channel to
   *        process.
   * \param num_channels Number of channels to process.
   * \param decoded_frame Reference to the decoded frame, where decoded samples
   *        are written. The vector may be resized to fit the channels and
   *        time ticks within the function.
   */
  LibFlacCallbackData(
      uint32_t num_samples_per_channel, uint32_t num_channels,
      std::vector<std::vector<InternalSampleType>>& decoded_frame)
      : num_samples_per_channel_(num_samples_per_channel),
        num_channels_(num_channels),
        decoded_frame_(decoded_frame) {}

  /*!\brief Sets the frame to be decoded.
   *
   * \param raw_encoded_frame Frame to decode.
   */
  void SetEncodedFrame(absl::Span<const uint8_t> raw_encoded_frame);

  /*!\brief Retrieve the next slice to be decoded.
   *
   * Subsequent calls to this function will return the next slice of the encoded
   * frame. The output span is valid until the next call to `SetEncodedFrame`.
   *
   * \param chunk_size Maximum number of bytes to return.
   * \return Next slice to be decoded, or an empty span if the encoded frame is
   *         exhausted.
   */
  absl::Span<const uint8_t> GetNextSlice(size_t chunk_size);

  const uint32_t num_samples_per_channel_;
  const uint32_t num_channels_;

  // Reference to the backing data for the decoded frame.
  std::vector<std::vector<InternalSampleType>>& decoded_frame_;

 private:
  // Backing data for the next frame to be decoded.
  std::vector<uint8_t> encoded_frame_;

  // Index of the next byte to be read from the encoded frame.
  size_t next_byte_index_ = 0;
};

/*!\brief Reads an encoded flac frame into the libflac decoder
 *
 * This callback function is used whenever the decoder needs more input data.
 *
 * \param decoder Unused libflac stream decoder. This parameter is not used in
 *        this implementation, but is included to override the libflac
 *        signature.
 * \param buffer Output buffer for the encoded frame.
 * \param bytes At input, the maximum size of the buffer. At output, the actual
 *        number of bytes read (on successful read).
 * \param client_data Universal pointer, which in this case should point to
 *        a `LibFlacCallbackData`.
 *
 * \return A libflac read status indicating whether the read was successful.
 */
FLAC__StreamDecoderReadStatus LibFlacReadCallback(
    const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t* bytes,
    void* client_data);

/*!\brief Writes a decoded flac frame to an instance of FlacDecoder.
 *
 * This callback function is used to write out a decoded frame from the libflac
 * decoder.
 *
 * \param decoder Unused libflac stream decoder. This parameter is not used in
 *        this implementation, but is included to override the libflac
 *        signature.
 * \param frame libflac encoded frame metadata.
 * \param buffer Array of pointers to decoded channels of data. Each pointer
 *        will point to an array of signed samples of length
 *        `frame->header.blocksize`. Channels will be ordered according to the
 *        FLAC specification.
 * \param client_data universal pointer, which in this case should point to a
 *        `LibFlacCallbackData`.
 *
 * \return A libflac write status indicating whether the write was successful.
 */
FLAC__StreamDecoderWriteStatus LibFlacWriteCallback(
    const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
    const FLAC__int32* const buffer[], void* client_data);

/*!\brief Logs an error from the libflac decoder.
 *
 *  This function will be called whenever an error occurs during libflac
 *  decoding.
 *
 * \param decoder Unused libflac stream decoder. This parameter is not used in
 *        this implementation, but is included to override the libflac
 *        signature.
 * \param status The error encountered by the decoder.
 * \param client_data Universal pointer, which in this case should point to
 *        `LibFlacCallbackData`. Unused in this implementation.
 */
void LibFlacErrorCallback(const FLAC__StreamDecoder* /*decoder*/,
                          FLAC__StreamDecoderErrorStatus status,
                          void* /*client_data*/);

}  // namespace flac_callbacks

}  // namespace iamf_tools

#endif  // CLI_CODEC_FLAC_DECODER_STREAM_CALLBACKS_H_
