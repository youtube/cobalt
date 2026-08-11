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
 * @file IAMF_OBU.h
 * @brief OBU APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_OBU_H
#define IAMF_OBU_H

#include <stdint.h>

#include "IAMF_defines.h"

#define IAMF_OBJ(a) ((IAMF_Object *)(a))
#define IAMF_ELEMENT(a) ((IAMF_Element *)(a))
#define IAMF_PARAMETER(a) ((IAMF_Parameter *)(a))
#define IAMF_MIX_PRESENTATION(a) ((IAMF_MixPresentation *)(a))

typedef enum IAMF_OBU_Type {
  IAMF_OBU_DEFAULT = -1,
  IAMF_OBU_CODEC_CONFIG,
  IAMF_OBU_AUDIO_ELEMENT,
  IAMF_OBU_MIX_PRESENTATION,
  IAMF_OBU_PARAMETER_BLOCK,
  IAMF_OBU_TEMPORAL_DELIMITER,
  IAMF_OBU_AUDIO_FRAME,
  IAMF_OBU_AUDIO_FRAME_ID0,
  IAMF_OBU_AUDIO_FRAME_ID17 = 23,
  IAMF_OBU_SEQUENCE_HEADER = 31
} IAMF_OBU_Type;

typedef enum IAMF_OBU_Flag {
  IAMF_OBU_FLAG_REDUNDANT = 0x1,
} IAMF_OBU_Flag;

typedef enum IAMF_Element_Type {
  AUDIO_ELEMENT_TYPE_CHANNEL_BASED,
  AUDIO_ELEMENT_TYPE_SCENE_BASED
} IAMF_Element_Type;

typedef enum IAMF_Parameter_Animated_Type {
  PARAMETER_ANIMATED_TYPE_STEP,
  PARAMETER_ANIMATED_TYPE_LINEAR,
  PARAMETER_ANIMATED_TYPE_BEZIER
} IAMF_Parameter_Animated_Type;

typedef enum IAMF_Ambisonics_Mode {
  AMBISONICS_MODE_MONO,
  AMBISONICS_MODE_PROJECTION
} IAMF_Ambisonics_Mode;

typedef struct IAMF_OBU {
  uint8_t *data;
  uint32_t size;

  uint8_t type : 5;
  uint8_t redundant : 1;
  uint8_t trimming : 1;
  uint8_t extension : 1;

  uint64_t trim_start;
  uint64_t trim_end;

  uint8_t *ext_header;
  uint64_t ext_size;

  uint8_t *payload;
} IAMF_OBU;

typedef struct IAMF_Object {
  IAMF_OBU_Type type;
  IAMF_OBU_Flag flags;
} IAMF_Object;

#define IAMF_OBJECT_PARAM(p) ((IAMF_ObjectParameter *)(p))
#define IAMF_PARAMETER_PARAM(p) ((IAMF_ParameterParam *)(p))
#define IAMF_MIX_PRESENTATION_PARAM(p) ((IAMF_MixPresentationParam *)p)

enum {
  OPTION_ELEMENT_TYPE = 0x010001,
  OPTION_ELEMENT_CHANNELS,
};

typedef struct IAMF_ObjectParameter {
  IAMF_OBU_Type type;
} IAMF_ObjectParameter;

typedef struct ParameterBase ParameterBase;
#define PARAMETER_BASE(a) ((ParameterBase *)(a))

typedef struct IAMF_ParameterParam {
  IAMF_ObjectParameter base;
  ParameterBase *param_base;
  int nb_layers;
  uint32_t recon_gain_present_flags;
} IAMF_ParameterParam;

/**
 * Version Object (Sequence Header OBU).
 * */

typedef struct IAMF_Version {
  IAMF_Object obj;

  uint32_t iamf_code;
  uint8_t primary_profile;
  uint8_t additional_profile;
} IAMF_Version;

/**
 * CodecConf Object (Codec Config OBU).
 * */

typedef struct IAMF_CodecConf {
  IAMF_Object obj;

  uint64_t codec_conf_id;

  // codec config
  uint32_t codec_id;
  uint64_t nb_samples_per_frame;
  short roll_distance;

  uint8_t *decoder_conf;
  uint32_t decoder_conf_size;
} IAMF_CodecConf;

/**
 * Element Object (Audio Element OBU).
 * */

typedef struct DemixingParameter DemixingParameter;
typedef struct ReconGainParameter ReconGainParameter;

typedef struct ScalableChannelLayoutConf ScalableChannelLayoutConf;
typedef struct AmbisonicsConf AmbisonicsConf;

typedef struct IAMF_Element {
  IAMF_Object obj;

  uint64_t element_id;
  uint8_t element_type;

  uint64_t codec_config_id;

  // substreams
  uint64_t nb_substreams;
  uint64_t *substream_ids;

  // parameters
  uint64_t nb_parameters;
  ParameterBase **parameters;

  // config
  union {
    ScalableChannelLayoutConf *channels_conf;
    AmbisonicsConf *ambisonics_conf;
  };
} IAMF_Element;

typedef struct ParameterSegment ParameterSegment;

struct ParameterBase {
  uint64_t type;
  uint64_t id;
  uint64_t rate;
  uint64_t duration;
  uint64_t nb_segments;
  uint64_t constant_segment_interval;
  uint32_t mode;

  ParameterSegment *segments;
};

struct DemixingParameter {
  ParameterBase base;
  uint8_t mode;
  uint8_t w;
};

struct ReconGainParameter {
  ParameterBase base;
};

typedef struct OutputGain {
  uint8_t output_gain_flag;
  int16_t output_gain;
} OutputGain;

typedef struct ChannelLayerConf {
  uint8_t loudspeaker_layout : 4;
  uint8_t output_gain_flag : 1;
  uint8_t recon_gain_flag : 1;
  uint8_t nb_substreams;
  uint8_t nb_coupled_substreams;
  OutputGain *output_gain_info;
} ChannelLayerConf;

struct ScalableChannelLayoutConf {
  uint32_t nb_layers;   // max valid layers
  uint32_t num_layers;  // total layers
  ChannelLayerConf *layer_conf_s;
};

struct AmbisonicsConf {
  uint64_t ambisonics_mode;
  int output_channel_count;
  int substream_count;
  int coupled_substream_count;
  uint8_t *mapping;
  int mapping_size;
};

/**
 * Mix Presentation Object (Mix Presentation OBU)
 * */

typedef struct SubMixPresentation SubMixPresentation;

#define TARGET_LAYOUT(a) ((TargetLayout *)(a))
#define SOUND_SYSTEM_LAYOUT(a) ((SoundSystemLayout *)(a))

typedef struct IAMF_MixPresentation {
  IAMF_Object obj;

  uint64_t mix_presentation_id;
  uint64_t num_labels;
  char **language;
  char **mix_presentation_friendly_label;

  uint64_t num_sub_mixes;
  SubMixPresentation *sub_mixes;
} IAMF_MixPresentation;

typedef struct TargetLayout {
  uint32_t type;
} TargetLayout;

typedef struct SoundSystemLayout {
  TargetLayout base;
  uint32_t sound_system;
} SoundSystemLayout;

typedef struct BinauralLayout {
  TargetLayout base;
} BinauralLayout;

typedef struct MixGainParameter {
  ParameterBase base;
  short mix_gain;
} MixGainParameter;

typedef struct ElementRenderingConf {
  uint8_t headphones_rendering_mode;
  uint32_t rendering_config_extension_size;
} ElementRenderingConf;

typedef struct ElementMixConf {
  MixGainParameter gain;
} ElementMixConf;

typedef struct ElementConf {
  uint64_t element_id;
  char **audio_element_friendly_label;
  ElementRenderingConf conf_r;
  ElementMixConf conf_m;
} ElementConf;

typedef struct OutputMixConf {
  MixGainParameter gain;
} OutputMixConf;

struct SubMixPresentation {
  uint64_t nb_elements;
  ElementConf *conf_s;

  OutputMixConf output_mix_config;

  uint64_t num_layouts;
  TargetLayout **layouts;
  IAMF_LoudnessInfo *loudness;
};

/**
 *  Parameter Object (Parameter Block OBU).
 * */

typedef struct IAMF_Parameter {
  IAMF_Object obj;

  uint64_t id;
  uint64_t duration;
  uint64_t nb_segments;
  uint64_t constant_segment_interval;
  uint64_t type;

  ParameterSegment **segments;
} IAMF_Parameter;

#define ANIMATED_PARAMETER_DEFINE(T1, T2)      \
  typedef struct AnimatedParameter_##T1_##T2 { \
    uint64_t animated_type;                    \
    T1 start;                                  \
    T1 end;                                    \
    T1 control;                                \
    T2 control_relative_time;                  \
  } AnimatedParameter_##T1_##T2

ANIMATED_PARAMETER_DEFINE(short, uint8_t);
ANIMATED_PARAMETER_DEFINE(float, float);

#define AnimatedParameter(T1, T2) AnimatedParameter_##T1_##T2

struct ParameterSegment {
  uint64_t type;
  uint64_t segment_interval;
};

typedef struct MixGainSegment {
  ParameterSegment seg;
  AnimatedParameter(short, uint8_t) mix_gain;
  AnimatedParameter(float, float) mix_gain_f;
} MixGainSegment;

typedef struct DemixingSegment {
  ParameterSegment seg;
  int demixing_mode;
} DemixingSegment;

typedef struct ReconGain {
  int layout;
  uint32_t flags;
  uint8_t *recon_gain;
  float *recon_gain_f;
} ReconGain;

typedef struct ReconGainList {
  int count;
  ReconGain *recon;
} ReconGainList;

typedef struct ReconGainSegment {
  ParameterSegment seg;
  ReconGainList list;
} ReconGainSegment;

/**
 *  Frame Object (Audio Frame OBU).
 * */

typedef struct IAMF_Frame {
  IAMF_Object obj;

  uint64_t id;

  uint64_t trim_start;
  uint64_t trim_end;

  uint8_t *data;
  uint32_t size;
} IAMF_Frame;

/**
 *  Sync Object (Sync OBU).
 * */

typedef struct ObjectSync ObjectSync;

typedef struct IAMF_Sync {
  IAMF_Object obj;
  uint64_t global_offset;
  uint64_t nb_obu_ids;
  ObjectSync *objs;
} IAMF_Sync;

struct ObjectSync {
  uint64_t obu_id;
  uint8_t obu_data_type;
  uint8_t reinitialize_decoder;
  int64_t relative_offset;
};

uint32_t IAMF_OBU_split(const uint8_t *data, uint32_t size, IAMF_OBU *obu);
int IAMF_OBU_is_descrptor_OBU(IAMF_OBU *obu);
int IAMF_OBU_is_reserved_OBU(IAMF_OBU *obu);
uint64_t IAMF_OBU_get_object_id(IAMF_OBU *obu);
const char *IAMF_OBU_type_string(IAMF_OBU_Type type);
IAMF_Object *IAMF_object_new(IAMF_OBU *obu, IAMF_ObjectParameter *param);
void IAMF_object_free(IAMF_Object *obj);
void IAMF_parameter_segment_free(ParameterSegment *seg);
#endif
