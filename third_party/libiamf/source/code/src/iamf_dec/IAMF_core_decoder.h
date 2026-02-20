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
 * @file IAMF_core_decoder.h
 * @brief Core decoder APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_CORE_DECODER_H_
#define IAMF_CORE_DECODER_H_

#include <stdint.h>

#include "IAMF_codec.h"
#include "IAMF_defines.h"

typedef struct IAMF_CoreDecoder IAMF_CoreDecoder;

/**
 * @brief     Open an iamf core decoder.
 * @param     [in] cid : the codec id.
 * @return    return an iamf core decoder handle.
 */
IAMF_CoreDecoder *iamf_core_decoder_open(IAMF_CodecID cid);

/**
 * @brief     Close the iamf core decoder.
 * @param     [in] ths : the core decoder handle.
 */
void iamf_core_decoder_close(IAMF_CoreDecoder *ths);

/**
 * @brief     Initialize the iamf core decoder.
 * @param     [in] ths : the core decoder handle.
 * @return    @ref IAErrCode.
 */
int iamf_core_decoder_init(IAMF_CoreDecoder *ths);

/**
 * @brief     Set the codec specific data to iamf core decoder.
 * @param     [in] ths : the core decoder handle.
 * @param     [in] spec : the codec specific data.
 * @param     [in] len : the length of codec specific data.
 * @return    @ref IAErrCode.
 */
int iamf_core_decoder_set_codec_conf(IAMF_CoreDecoder *ths, uint8_t *spec,
                                     uint32_t len);

/**
 * @brief     Set the stream information of core decoder.
 * @param     [in] ths : the core decoder handle.
 * @param     [in] mode : the type of core decoder, ambisonics or channel-based.
 * @param     [in] channels : the number of channels.
 * @param     [in] streams : the number of streams.
 * @param     [in] coupled_streams : the number of coupled streams.
 * @param     [in] mapping : the matrix for ambisonics, channel_mapping or
 * demixing_matrix.
 * @param     [in] mapping_size : the length of mapping matrix.
 * @return    @ref IAErrCode.
 */
int iamf_core_decoder_set_streams_info(IAMF_CoreDecoder *ths, uint32_t mode,
                                       uint8_t channels, uint8_t streams,
                                       uint8_t coupled_streams,
                                       uint8_t mapping[],
                                       uint32_t mapping_size);

/**
 * @brief     Decode iamf packet.
 * @param     [in] ths : the iamf core decoder handle.
 * @param     [in] buffer : the iamf packet buffer.
 * @param     [in] sizes : the size of iamf packet buffer.
 * @param     [in] count : the stream count.
 * @param     [in] out : the decoded pcm frame.
 * @param     [in] frame_size : the size of one audio frame.
 * @return    @ref IAErrCode.
 */
int iamf_core_decoder_decode(IAMF_CoreDecoder *ths, uint8_t *buffers[],
                             uint32_t *sizes, uint32_t count, float *out,
                             uint32_t frame_size);

/**
 * @brief     Get the delay of iamf decoding.
 * @return    return the decoder delay.
 */
int iamf_core_decoder_get_delay(IAMF_CoreDecoder *ths);

#endif /* IAMF_CORE_DECODER_H_ */
