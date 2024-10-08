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
 * @file downmix_renderer.h
 * @brief DMRenderer APIs.
 * @version 0.1
 * @date Created 06/21/2023
 **/

#ifndef __DOWNMIX_RENDERER_H_
#define __DOWNMIX_RENDERER_H_

#include <stdint.h>

#include "IAMF_defines.h"

typedef struct Downmixer DMRenderer;

/**
 * @brief     Open a downmix renderer.
 * @param     [in] in : the input layout of downmix renderer.
 * @param     [in] out : the output layout of downmix renderer.
 * @return    return a downmix renderer handle.
 */
DMRenderer *DMRenderer_open(IAChannelLayoutType in, IAChannelLayoutType out);

/**
 * @brief     Close the downmix renderer.
 * @param     [in] thisp : the downmix renderer handle.
 */
void DMRenderer_close(DMRenderer *thisp);

/**
 * @brief     Set the demix mode and demix weight index for downmix renderer.
 * @param     [in] thisp : the downmix renderer handle.
 * @param     [in] mode : the demix mode.
 * @param     [in] w_idx : the demix weight index.
 * @return    @ref IAErrCode.
 */
int DMRenderer_set_mode_weight(DMRenderer *thisp, int mode, int w_idx);

/**
 * @brief     Do downmix rendering from input layout to output layout.
 * @param     [in] thisp : the downmix renderer handle.
 * @param     [in] in : the input audio pcm.
 * @param     [in] out : the output audio pcm.
 * @param     [in] s : the start downmix position.
 * @param     [in] size : the defined frame size.
 * @return    @ref IAErrCode.
 */
int DMRenderer_downmix(DMRenderer *thisp, float *in, float *out, uint32_t s,
                       uint32_t duration, uint32_t size);

#endif /* __DOWNMIX_RENDERER_H_ */
