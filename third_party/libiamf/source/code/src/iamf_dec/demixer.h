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
 * @file Demixer.h
 * @brief Demixer APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef __DEMIXER_H_
#define __DEMIXER_H_

#include <stdint.h>

#include "IAMF_defines.h"
#include "IAMF_types.h"

typedef struct Demixer Demixer;

/**
 * @brief     Open a demixer.
 * @param     [in] frame_size : the frame size of demix process.
 * @return    return a demixer handle.
 */
Demixer *demixer_open(uint32_t frame_size);

/**
 * @brief     Close the demixer.
 * @param     [in] ths : the demixer handle.
 * @param     [in] frame_size : the frame size of demix process.
 * @return    return a demixer handle.
 */
void demixer_close(Demixer *ths);

/**
 * @brief     Set the offset for different demix hanlding in one audio frame.
 * @param     [in] ths : the demixer handle.
 * @param     [in] offset : the frame size of demix process.
 * @return    @ref IAErrCode.
 */
int demixer_set_frame_offset(Demixer *ths, uint32_t offset);

/**
 * @brief     Set the target layout of demixer.
 * @param     [in] ths : the demixer handle.
 * @param     [in] layout : the target layout of demixer.
 * @return    @ref IAErrCode.
 */
int demixer_set_channel_layout(Demixer *ths, IAChannelLayoutType layout);

/**
 * @brief     Set the channle order in one audio frame.
 * @param     [in] ths : the demixer handle.
 * @param     [in] chs : the channle order.
 * @param     [in] count : the number of channels in frame.
 * @return    @ref IAErrCode.
 */
int demixer_set_channels_order(Demixer *ths, IAChannel *chs, int count);

/**
 * @brief     Set output gain for all scalable audio channel layers.
 * @param     [in] ths : the demixer handle.
 * @param     [in] chs : the channels that need to apply output gain.
 * @param     [in] count : the number of channels that need to apply output
 * gain.
 * @return    @ref IAErrCode.
 */
int demixer_set_output_gain(Demixer *ths, IAChannel *chs, float *gain,
                            int count);

/**
 * @brief     Set demix information: demix mode and demix weight index.
 * @param     [in] ths : the demixer handle.
 * @param     [in] mode : the demix mode.
 * @param     [in] w_idx : the demix weight index.
 * @return    @ref IAErrCode.
 */
int demixer_set_demixing_info(Demixer *ths, int mode, int w_idx);

/**
 * @brief     Set the reconstruct gain to smooth demixed audio frame.
 * @param     [in] ths : the demixer handle.
 * @param     [in] chs : the channels that need to apply reconstruct gain.
 * @param     [in] recon_gain : the reconstruct value.
 * @param     [in] flags : a bitmask that indicates which channels recon_gain is
 * applied to.
 * @return    @ref IAErrCode.
 */
int demixer_set_recon_gain(Demixer *ths, int count, IAChannel *chs,
                           float *recon_gain, uint32_t flags);

/**
 * @brief     Do demixing to get the audio pcm of target layout.
 * @param     [in] ths : the demixer handle.
 * @param     [in] dst : the output audio pcm.
 * @param     [in] src : the input audio pcm.
 * @param     [in] size : the audio samples of one demixing block.
 * @return    @ref IAErrCode.
 */
int demixer_demixing(Demixer *ths, float *dst, float *src, uint32_t size);

#endif /* __DEMIXER_H_ */
