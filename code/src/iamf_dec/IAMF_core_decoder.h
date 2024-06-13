/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
