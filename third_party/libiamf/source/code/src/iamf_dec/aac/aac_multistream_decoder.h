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
 * @file aac_multistream_decoder.h
 * @brief AAC decoder APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef _AAC_MULTISTREAM2_DECODER_H_
#define _AAC_MULTISTREAM2_DECODER_H_

#include <stdint.h>

typedef struct AACMSDecoder AACMSDecoder;

/**
 * @brief     Open an aac decoder.
 * @param     [in] config : the codec configure buffer.
 * @param     [in] size : the size of codec configure buffer.
 * @param     [in] streams : the audio stream count.
 * @param     [in] coupled_streams : the coupled audio stream count.
 * @param     [in] flags : pcm storage order.
 * @param     [in] error : the error code when open an aac decoder.
 * @return    return an aac decoder handle.
 */
AACMSDecoder *aac_multistream_decoder_open(uint8_t *config, uint32_t size,
                                           int streams, int coupled_streams,
                                           uint32_t flags, int *error);

/**
 * @brief     Decode aac packet.
 * @param     [in] st : aac decoder handle.
 * @param     [in] buffer : the aac packet buffer.
 * @param     [in] len : the length of aac packet buffer.
 * @param     [in] pcm : the decoded pcm frame.
 * @param     [in] frame_size : the frame size of aac.
 * @return    return the number of decoded samples.
 */
int aac_multistream_decode(AACMSDecoder *st, uint8_t *buffer[], uint32_t len[],
                           void *pcm, uint32_t frame_size);

/**
 * @brief     Close the aac decoder.
 * @param     [in] st : aac decoder handle.
 */
void aac_multistream_decoder_close(AACMSDecoder *st);

/**
 * @brief     Get the decoder delay of aac.
 * @param     [in] st : aac decoder handle.
 * @return    return the decoder delay of aac.
 */
int aac_multistream_decoder_get_delay(AACMSDecoder *st);

#endif /* _AAC_MULTISTREAM2_DECODER_H_ */
