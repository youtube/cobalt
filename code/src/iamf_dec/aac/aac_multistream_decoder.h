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
