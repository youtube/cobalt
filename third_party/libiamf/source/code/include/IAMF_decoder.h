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
 * @file IAMF_decoder.h
 * @brief IAMF decoder APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DECODER_H
#define IAMF_DECODER_H

#include <stdint.h>

#include "IAMF_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IAMF_StreamInfo {
  uint32_t max_frame_size;
} IAMF_StreamInfo;

/**@}*/
/**\name Immersive audio decoder functions */
/**@{*/

typedef struct IAMF_Decoder *IAMF_DecoderHandle;

/**
 * @brief     Open an iamf decoder.
 * @return    return an iamf decoder handle.
 */
IAMF_DecoderHandle IAMF_decoder_open(void);

/**
 * @brief     Close an iamf decoder.
 * @param     [in] handle : iamf decoder handle.
 */
int IAMF_decoder_close(IAMF_DecoderHandle handle);

/**
 * @brief     Configurate an iamf decoder. The first configurating decoder must
 *            need descriptor OBUs, then if only some properties have been
 *            changed, @ref IAMF_decoder_set_mix_presentation_label API. the
 *            descriptor OBUs is not needed.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] data : the bitstream.
 * @param     [in] size : the size in bytes of bitstream.
 * @param     [in & out] rsize : the size in bytes of bitstream that has been
 *                               consumed. If is null, it means the data is the
 *                               complete configuration OBUs.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_configure(IAMF_DecoderHandle handle, const uint8_t *data,
                           uint32_t size, uint32_t *rsize);

/**
 * @brief     Decode bitstream.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] data : the OBUs in bitstream.
 *                        if is null, the output is delay signal.
 * @param     [in] size : the size in bytes of bitstream.
 * @param     [in & out] rsize : the size in bytes of bitstream that has been
 *                               consumed. If it is null, it means the data is
 *                               a complete access unit which includes all OBUs
 *                               of substream frames and parameters.
 * @param     [out] pcm : output signal.
 * @return    the number of decoded samples or @ref IAErrCode.
 */
int IAMF_decoder_decode(IAMF_DecoderHandle handle, const uint8_t *data,
                        int32_t size, uint32_t *rsize, void *pcm);

/**
 * @brief     Set mix presentation id to select the mix that match the user's
 *            preferences.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] id : an identifier for a Mix Presentation.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_mix_presentation_id(IAMF_DecoderHandle handle,
                                         uint64_t id);

/**
 * @brief     Set sound system output layout.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] ss : the sound system (@ref IAMF_SoundSystem).
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_output_layout_set_sound_system(IAMF_DecoderHandle handle,
                                                IAMF_SoundSystem ss);

/**
 * @brief     Set binaural output layout.
 * @param     [in] handle : iamf decoder handle.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_output_layout_set_binaural(IAMF_DecoderHandle handle);

/**
 * @brief     Get the number of channels of the sound system.
 * @param     [in] ss : the sound system (@ref IAMF_SoundSystem).
 * @return    the number of channels.
 */
int IAMF_layout_sound_system_channels_count(IAMF_SoundSystem ss);

/**
 * @brief     Get the number of channels of binaural pattern.
 * @return    the number of channels.
 */
int IAMF_layout_binaural_channels_count();

/**
 * @brief     Get the codec capability of iamf. Need to free string manually.
 *            The codec string format will be:
 *            "iamf.xxx.yyy.Opus;iamf.xxx.yyy.mp4a.40.2" Where xxx is three
 *            digits to indicate the value of the primary_profile and yyy
 *            is three digits to indicate the value of the additional_profile.
 * @return    the supported codec string.
 */
char *IAMF_decoder_get_codec_capability();

/**
 * @brief     Set target normalization loudness value, then loudness will be
 *            adjusted to the setting target.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] loundness : target normalization loudness in LKFS.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_normalization_loudness(IAMF_DecoderHandle handle,
                                            float loudness);

/**
 * @brief     Set pcm output bitdepth.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] bit depth : target bit depth in bit.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_bit_depth(IAMF_DecoderHandle handle, uint32_t bit_depth);

/**
 * @brief     Enable peak limiter. In the decoder, the peak limiter is enabled
 *            by default.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] enable : 1 indicates enabled, and 0 indicates disable.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_peak_limiter_enable(IAMF_DecoderHandle handle,
                                     uint32_t enable);

/**
 * @brief     Set peak threshold value of peak limiter.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] db : peak threshold in dB.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_peak_limiter_set_threshold(IAMF_DecoderHandle handle,
                                            float db);

/**
 * @brief     Get peak threshold value of peak limiter.
 * @param     [in] handle : iamf decoder handle.
 * @return    Peak threshold in dB.
 */
float IAMF_decoder_peak_limiter_get_threshold(IAMF_DecoderHandle handle);

/**
 * @brief     Set pcm output sampling rate.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] rate : sampling rate.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_sampling_rate(IAMF_DecoderHandle handle, uint32_t rate);

/**
 * @brief     Get stream info. Must be used after decoder configuration.
 *            max frame size could be gotten.
 * @param     [in] handle : iamf decoder handle.
 * @return    @stream info.
 */
IAMF_StreamInfo *IAMF_decoder_get_stream_info(IAMF_DecoderHandle handle);

// Following functions are provided for handling metadata and pts.
typedef struct IAMF_Param {
  int parameter_length;
  uint32_t parameter_definition_type;
  union {
    uint32_t dmixp_mode;
  };
} IAMF_Param;

typedef enum IAMF_SoundMode {
  IAMF_SOUND_MODE_NONE = -2,
  IAMF_SOUND_MODE_NA = -1,
  IAMF_SOUND_MODE_STEREO,
  IAMF_SOUND_MODE_MULTICHANNEL,
  IAMF_SOUND_MODE_BINAURAL
} IAMF_SoundMode;

typedef struct IAMF_extradata {
  IAMF_SoundSystem output_sound_system;
  uint32_t number_of_samples;
  uint32_t bitdepth;
  uint32_t sampling_rate;
  IAMF_SoundMode output_sound_mode;

  int num_loudness_layouts;
  IAMF_Layout *loudness_layout;
  IAMF_LoudnessInfo *loudness;

  uint32_t num_parameters;
  IAMF_Param *param;
} IAMF_extradata;

/**
 * @brief     Set the start timestamp and time base to decoder.
 *            max frame size could be gotten.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] pts : the start timestamp.
 * @param     [in] time_base : the time base used for pts.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_pts(IAMF_DecoderHandle handle, int64_t pts,
                         uint32_t time_base);

/**
 * @brief     Get the metadata and pts corresponding to last PCM frame.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] pts : the timestamp of last PCM frame.
 * @param     [in] metadata : the metadata of last PCM frame.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_get_last_metadata(IAMF_DecoderHandle handle, int64_t *pts,
                                   IAMF_extradata *metadata);
#ifdef __cplusplus
}
#endif

#endif /* IAMF_DECODER_H */
