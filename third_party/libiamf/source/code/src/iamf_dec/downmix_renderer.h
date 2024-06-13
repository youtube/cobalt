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
