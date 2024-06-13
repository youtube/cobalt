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

/*
AOM-IAMF Standard Deliverable Status:
This software module is out of scope and not part of the IAMF Final Deliverable.
*/

/**
 * @file audio_effect_peak_limiter.h
 * @brief Peak Limiter APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef __AUDIO_PEAK_LIMITER_H_
#define __AUDIO_PEAK_LIMITER_H_

#define USE_TRUEPEAK 0

#if USE_TRUEPEAK
#include "audio_true_peak_meter.h"
#endif

#include <stdint.h>

#include "audio_defines.h"

typedef struct AudioEffectPeakLimiter {
  int init;
  int padsize;
  float currentGain;
  float targetStartGain;
  float targetEndGain;
  float attackSec;
  float releaseSec;
  float linearThreashold;
  float currentTC;
  float incTC;
  int numChannels;

  float delayData[MAX_OUTPUT_CHANNELS][MAX_DELAYSIZE + 1];
  float peakData[MAX_DELAYSIZE + 1];
  int entryIndex;
  int delaySize;
  int delayBufferSize;

#ifndef OLD_CODE
  int peak_pos;
#endif

#if USE_TRUEPEAK
  AudioTruePeakMeter truePeakMeters[MAX_OUTPUT_CHANNELS];
#endif
} AudioEffectPeakLimiter;

/**
 * @brief     Create a peak limiter.
 * @return    return a peak limiter handle.
 */
AudioEffectPeakLimiter* audio_effect_peak_limiter_create(void);

/**
 * @brief     Initialize the peak limiter.
 * @param     [in] ths : the peak limiter handle
 * @param     [in] threashold_db : peak threshold in dB
 * @param     [in] sample_rate : sample rate of audio signal
 * @param     [in] num_channels : number of channels in frame
 * @param     [in] atk_sec : attack duration in seconds
 * @param     [in] atk_sec : release duration in seconds
 * @param     [in] delay_size : number of samples in delay buffer
 */
void audio_effect_peak_limiter_init(AudioEffectPeakLimiter* ths,
                                    float threashold_db, int sample_rate,
                                    int num_channels, float atk_sec,
                                    float rel_sec, int delay_size);

/**
 * @brief     Process the pcm signal with predefined peak limiter.
 * @param     [in] ths : the peak limiter handle
 * @param     [in] inblock : the input pcm signal
 * @param     [in] outblock : the output pcm signal
 * @param     [in] frame_size : the frame size of one process block
 * @return    the number of processed samples
 */
int audio_effect_peak_limiter_process_block(AudioEffectPeakLimiter* ths,
                                            float* inblock, float* outblock,
                                            int frame_size);

/**
 * @brief     De-initialize the peak limiter.
 * @param     [in] ths : the peak limiter handle
 */
void audio_effect_peak_limiter_uninit(AudioEffectPeakLimiter* ths);

/**
 * @brief     Destroy the peak limiter.
 * @param     [in] ths : the peak limiter handle
 */
void audio_effect_peak_limiter_destroy(AudioEffectPeakLimiter* ths);

#endif /* __AUDIO_PEAK_LIMITER_H_ */
