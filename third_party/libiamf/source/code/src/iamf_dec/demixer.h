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
