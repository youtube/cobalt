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
