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

Demixer *demixer_open(uint32_t frame_size);
void demixer_close(Demixer *);

int demixer_set_frame_offset(Demixer *, uint32_t offset);
int demixer_set_channel_layout(Demixer *, IAChannelLayoutType);
int demixer_set_channels_order(Demixer *, IAChannel *, int);
int demixer_set_output_gain(Demixer *, IAChannel *, float *, int);
int demixer_set_demixing_info(Demixer *, int, int);
int demixer_set_recon_gain(Demixer *, int, IAChannel *, float *, uint32_t);
int demixer_demixing(Demixer *, float *, float *, uint32_t);

#endif /* __DEMIXER_H_ */
