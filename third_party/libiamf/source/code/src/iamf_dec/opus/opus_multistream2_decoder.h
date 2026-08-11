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

/**
 * @file opus_multistream2_decode.h
 * @brief opus decoder APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef _OPUS_MULTISTREAM2_DECODER_H_
#define _OPUS_MULTISTREAM2_DECODER_H_

#include <stdint.h>

typedef struct OpusMS2Decoder OpusMS2Decoder;

/**
 * @brief     Create an opus decoder.
 * @param     [in] Fs : the sample rate.
 * @param     [in] streams : the audio stream count.
 * @param     [in] coupled_streams : the coupled audio stream count.
 * @param     [in] flags : pcm storage order.
 * @param     [in] error : the error code when open an aac decoder.
 * @return    return an opus decoder handle.
 */
OpusMS2Decoder *opus_multistream2_decoder_create(int Fs, int streams,
                                                 int coupled_streams,
                                                 uint32_t flags, int *error);

/**
 * @brief     Decode opus packet.
 * @param     [in] st : opus decoder handle.
 * @param     [in] buffer : the flac packet buffer.
 * @param     [in] len : the length of flac packet buffer.
 * @param     [in] pcm : the decoded pcm frame.
 * @param     [in] frame_size : the frame size of aac.
 * @return    return the number of decoded samples.
 */
int opus_multistream2_decode(OpusMS2Decoder *st, uint8_t *buffer[],
                             uint32_t len[], void *pcm, uint32_t frame_size);

/**
 * @brief     Destroy an opus decoder.
 * @param     [in] st : opus decoder handle.
 */
void opus_multistream2_decoder_destroy(OpusMS2Decoder *st);

#endif /* _OPUS_MULTISTREAM2_DECODER_H_ */
