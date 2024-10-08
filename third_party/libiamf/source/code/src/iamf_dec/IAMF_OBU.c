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
 * @file IAMF_OBU.c
 * @brief OBU Parser.
 * @version 0.1
 * @date Created 03/03/2023
 **/
#include "IAMF_OBU.h"

#include <inttypes.h>

#include "IAMF_debug.h"
#include "IAMF_defines.h"
#include "IAMF_utils.h"
#include "bitstream.h"
#include "fixedp11_5.h"

#ifndef SUPPORT_VERIFIER
#define SUPPORT_VERIFIER 0
#endif
#if SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#define IAMF_OBU_MIN_SIZE 2
#define STRING_SIZE 128

#ifdef IA_TAG
#undef IA_TAG
#endif
#define IA_TAG "IAMF_OBU"

static uint32_t iamf_obu_get_payload_size(IAMF_OBU *obu);
static IAMF_Version *iamf_version_new(IAMF_OBU *obu);

static IAMF_CodecConf *iamf_codec_conf_new(IAMF_OBU *obu);
static void iamf_codec_conf_free(IAMF_CodecConf *obj);

static IAMF_Element *iamf_element_new(IAMF_OBU *obu);
static void iamf_element_free(IAMF_Element *obj);

static IAMF_MixPresentation *iamf_mix_presentation_new(IAMF_OBU *obu);
static void iamf_mix_presentation_free(IAMF_MixPresentation *obj);

static IAMF_Parameter *iamf_parameter_new(IAMF_OBU *obu,
                                          IAMF_ParameterParam *param);
static void iamf_parameter_free(IAMF_Parameter *obj);

static IAMF_Frame *iamf_frame_new(IAMF_OBU *obu);

static void iamf_parameter_recon_gain_segment_free(ReconGainSegment *seg);

uint32_t IAMF_OBU_split(const uint8_t *data, uint32_t size, IAMF_OBU *obu) {
  BitStream b;
  uint64_t ret = 0;

  if (size < IAMF_OBU_MIN_SIZE) {
    return 0;
  }

  bs(&b, data, size);

  memset(obu, 0, sizeof(IAMF_OBU));
  obu->type = bs_get32b(&b, 5);
  obu->redundant = bs_get32b(&b, 1);
  obu->trimming = bs_get32b(&b, 1);
  obu->extension = bs_get32b(&b, 1);

  ret = bs_getAleb128(&b);

  if (ret == UINT64_MAX || ret + bs_tell(&b) > size) return 0;

  ia_logt("===============================================");
  ia_logt(
      "obu header : %s (%d) type, redundant %d, trimming %d, extension %d, "
      "payload size %" PRIu64 ", obu size %" PRIu64 " vs size %u",
      IAMF_OBU_type_string(obu->type), obu->type, obu->redundant, obu->trimming,
      obu->extension, ret, bs_tell(&b) + ret, size);

  if (obu->redundant) {
    ia_logd("%s OBU redundant.", IAMF_OBU_type_string(obu->type));
  }

  obu->data = (uint8_t *)data;
  obu->size = bs_tell(&b) + (uint32_t)ret;
  obu_dump(data, obu->size, obu->type);

  if (obu->trimming) {
    obu->trim_end = bs_getAleb128(&b);    // num_samples_to_trim_at_end;
    obu->trim_start = bs_getAleb128(&b);  // num_samples_to_trim_at_start;
    ia_logt("trim samples at start %" PRIu64 ", at end %" PRIu64,
            obu->trim_start, obu->trim_end);
  }

  if (obu->extension) {
    obu->ext_size = bs_getAleb128(&b);  // extension_header_size;
    obu->ext_header = (uint8_t *)obu->data + bs_tell(&b);
    ia_logt("obu extension header at %u, size %" PRIu64, bs_tell(&b),
            obu->ext_size);
    bs_skipABytes(&b, obu->ext_size);  // skip extension header
  }

  ia_logt("obu payload start at %u", bs_tell(&b));
  obu->payload = (uint8_t *)data + bs_tell(&b);

#if SUPPORT_VERIFIER
  if (obu->type == IAMF_OBU_TEMPORAL_DELIMITER)
    vlog_obu(IAMF_OBU_TEMPORAL_DELIMITER, obu, 0, 0);
#endif

  return obu->size;
}

int IAMF_OBU_is_descrptor_OBU(IAMF_OBU *obu) {
  IAMF_OBU_Type type = obu->type;

  if (type == IAMF_OBU_CODEC_CONFIG || type == IAMF_OBU_AUDIO_ELEMENT ||
      type == IAMF_OBU_MIX_PRESENTATION || type == IAMF_OBU_SEQUENCE_HEADER) {
    return 1;
  }
  return 0;
}

int IAMF_OBU_is_reserved_OBU(IAMF_OBU *obu) {
  IAMF_OBU_Type type = obu->type;

  if (type > IAMF_OBU_AUDIO_FRAME_ID17 && type < IAMF_OBU_SEQUENCE_HEADER) {
    return 1;
  }
  return 0;
}

uint64_t IAMF_OBU_get_object_id(IAMF_OBU *obu) {
  if (obu->type == IAMF_OBU_PARAMETER_BLOCK) {
    BitStream b;
    bs(&b, obu->payload, iamf_obu_get_payload_size(obu));
    return bs_getAleb128(&b);
  }
  return (uint64_t)-1;
}

const char *IAMF_OBU_type_string(IAMF_OBU_Type type) {
  static const char *obu_type_string[] = {
      "Codec Config",     "Audio Element",      "Mix Presentation",
      "Parameter Block",  "Temporal Delimiter", "Audio Frame",
      "Audio Frame ID0",  "Audio Frame ID1",    "Audio Frame ID2",
      "Audio Frame ID3",  "Audio Frame ID4",    "Audio Frame ID5",
      "Audio Frame ID6",  "Audio Frame ID7",    "Audio Frame ID8",
      "Audio Frame ID9",  "Audio Frame ID10",   "Audio Frame ID11",
      "Audio Frame ID12", "Audio Frame ID13",   "Audio Frame ID14",
      "Audio Frame ID15", "Audio Frame ID16",   "Audio Frame ID17"};
  static const char *obu_ia_header_string = "IA Sequence Header";
  static const char *obu_invalid_obu_type_string = "Invalid OBU Type";

  if (type >= IAMF_OBU_CODEC_CONFIG && type <= IAMF_OBU_AUDIO_FRAME_ID17)
    return obu_type_string[type];

  if (type == IAMF_OBU_SEQUENCE_HEADER) return obu_ia_header_string;

  return obu_invalid_obu_type_string;
}

IAMF_Object *IAMF_object_new(IAMF_OBU *obu, IAMF_ObjectParameter *param) {
  IAMF_Object *obj = 0;

  switch (obu->type) {
    case IAMF_OBU_SEQUENCE_HEADER:
      obj = IAMF_OBJ(iamf_version_new(obu));
      break;
    case IAMF_OBU_CODEC_CONFIG:
      obj = IAMF_OBJ(iamf_codec_conf_new(obu));
      break;
    case IAMF_OBU_AUDIO_ELEMENT:
      obj = IAMF_OBJ(iamf_element_new(obu));
      break;
    case IAMF_OBU_MIX_PRESENTATION:
      obj = IAMF_OBJ(iamf_mix_presentation_new(obu));
      break;
    case IAMF_OBU_PARAMETER_BLOCK: {
      IAMF_ParameterParam *p = IAMF_PARAMETER_PARAM(param);
      obj = IAMF_OBJ(iamf_parameter_new(obu, p));
    } break;
    default:
      if (obu->type >= IAMF_OBU_AUDIO_FRAME &&
          obu->type <= IAMF_OBU_AUDIO_FRAME_ID17) {
        obj = IAMF_OBJ(iamf_frame_new(obu));
      } else if (obu->type != IAMF_OBU_TEMPORAL_DELIMITER) {
        ia_logw("Reserved OBU type %u", obu->type);
      }
      break;
  }

  if (obj && obu->redundant) obj->flags |= IAMF_OBU_FLAG_REDUNDANT;

  return obj;
}

void IAMF_object_free(IAMF_Object *obj) {
  if (obj) {
    switch (obj->type) {
      case IAMF_OBU_SEQUENCE_HEADER:
        free(obj);
        break;
      case IAMF_OBU_CODEC_CONFIG:
        iamf_codec_conf_free((IAMF_CodecConf *)obj);
        break;
      case IAMF_OBU_AUDIO_ELEMENT:
        iamf_element_free((IAMF_Element *)obj);
        break;
      case IAMF_OBU_MIX_PRESENTATION:
        iamf_mix_presentation_free((IAMF_MixPresentation *)obj);
        break;
      case IAMF_OBU_PARAMETER_BLOCK:
        iamf_parameter_free((IAMF_Parameter *)obj);
        break;
      default:
        if (obj->type >= IAMF_OBU_AUDIO_FRAME &&
            obj->type < IAMF_OBU_SEQUENCE_HEADER) {
          free(obj);
        }
        break;
    }
  }
}

void IAMF_parameter_segment_free(ParameterSegment *seg) {
  if (seg) {
    if (seg->type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      iamf_parameter_recon_gain_segment_free((ReconGainSegment *)seg);
    } else
      free(seg);
  }
}

uint32_t iamf_obu_get_payload_size(IAMF_OBU *obu) {
  return obu->size - (uint32_t)(obu->payload - obu->data);
}

static int _valid_profile(uint8_t primary, uint8_t addional) {
  return primary < IAMF_PROFILE_COUNT && primary <= addional;
}

IAMF_Version *iamf_version_new(IAMF_OBU *obu) {
  IAMF_Version *ver = 0;
  BitStream b;
  union {
    uint32_t _id;
    uint8_t _4cc[4];
  } code = {._4cc = {'i', 'a', 'm', 'f'}};

  ver = IAMF_MALLOCZ(IAMF_Version, 1);
  if (!ver) {
    ia_loge("fail to allocate memory for Version Object.");
    goto version_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  ver->obj.type = IAMF_OBU_SEQUENCE_HEADER;
  bs_read(&b, (uint8_t *)&ver->iamf_code, 4);
  ver->primary_profile = bs_getA8b(&b);
  ver->additional_profile = bs_getA8b(&b);

  ia_logd(
      "ia sequence header object: %.4s, primary profile %u, additional profile "
      "%u.",
      (char *)&ver->iamf_code, ver->primary_profile, ver->additional_profile);

  if (ver->iamf_code != code._id) {
    ia_loge("ia sequence header object: Invalid iamf code %.4s.",
            (char *)&ver->iamf_code);
    goto version_fail;
  }

  if (!_valid_profile(ver->primary_profile, ver->additional_profile)) {
    ia_loge(
        "ia sequence header object: Invalid primary profile %u or additional "
        "profile %u.",
        ver->primary_profile, ver->additional_profile);
    goto version_fail;
  }

#if SUPPORT_VERIFIER
  vlog_obu(IAMF_OBU_SEQUENCE_HEADER, ver, 0, 0);
#endif

  return ver;

version_fail:
  if (ver) free(ver);
  return 0;
}

static int _valid_codec(uint32_t codec) {
  return iamf_codec_check(iamf_codec_4cc_get_codecID(codec));
}

#define OPUS_VERSION_MAX 15
static int _valid_decoder_config(uint32_t codec, uint8_t *conf, size_t size) {
  if (iamf_codec_4cc_get_codecID(codec) == IAMF_CODEC_OPUS) {
    if (conf[0] > OPUS_VERSION_MAX) {
      ia_logw("opus config invalid: version %u should less than %u.", conf[0],
              OPUS_VERSION_MAX);
      return 0;
    }
  }
  return 1;
}

IAMF_CodecConf *iamf_codec_conf_new(IAMF_OBU *obu) {
  IAMF_CodecConf *conf = 0;
  BitStream b;

  conf = IAMF_MALLOCZ(IAMF_CodecConf, 1);
  if (!conf) {
    ia_loge("fail to allocate memory for Codec Config Object.");
    goto codec_conf_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  conf->obj.type = IAMF_OBU_CODEC_CONFIG;

  conf->codec_conf_id = bs_getAleb128(&b);
  bs_read(&b, (uint8_t *)&conf->codec_id, 4);
  conf->nb_samples_per_frame = bs_getAleb128(&b);
  conf->roll_distance = (int16_t)bs_getA16b(&b);

  conf->decoder_conf_size = iamf_obu_get_payload_size(obu) - bs_tell(&b);
  conf->decoder_conf = IAMF_MALLOC(uint8_t, conf->decoder_conf_size);
  if (!conf->decoder_conf) {
    ia_loge(
        "fail to allocate memory for decoder config of Codec Config Object.");
    goto codec_conf_fail;
  }
  bs_read(&b, conf->decoder_conf, conf->decoder_conf_size);
  ia_logd("codec configure object: id %" PRIu64
          ", codec %.4s, decoder configure size %d, "
          "samples per frame %" PRIu64 ", roll distance %d",
          conf->codec_conf_id, (char *)&conf->codec_id, conf->decoder_conf_size,
          conf->nb_samples_per_frame, conf->roll_distance);

  if (!_valid_codec(conf->codec_id)) {
    ia_logw("codec configure object: id %" PRIu64 ", invalid codec %.4s",
            conf->codec_conf_id, (char *)&conf->codec_id);
    goto codec_conf_fail;
  }

  if (!_valid_decoder_config(conf->codec_id, conf->decoder_conf,
                             conf->decoder_conf_size)) {
    ia_logw("decoder config is invalid, codec: %.4s", (char *)&conf->codec_id);
    goto codec_conf_fail;
  }

#if SUPPORT_VERIFIER
  vlog_obu(IAMF_OBU_CODEC_CONFIG, conf, 0, 0);
#endif

  return conf;

codec_conf_fail:
  if (conf) iamf_codec_conf_free(conf);
  return 0;
}

void iamf_codec_conf_free(IAMF_CodecConf *obj) {
  IAMF_FREE(obj->decoder_conf);
  free(obj);
}

static int iamf_parameter_base_init(ParameterBase *pb, IAMF_ParameterType type,
                                    BitStream *b) {
  pb->type = type;
  pb->id = bs_getAleb128(b);
  pb->rate = bs_getAleb128(b);
  pb->mode = bs_get32b(b, 1);
  ia_logd("Parameter Base: type %" PRIu64 ", id %" PRIu64 ", rate %" PRIu64
          ", mode %u",
          pb->type, pb->id, pb->rate, pb->mode);
  if (!pb->mode) {
    pb->duration = bs_getAleb128(b);
    pb->constant_segment_interval = bs_getAleb128(b);
    ia_logd("\tduration %" PRIu64 ", constant segment interval %" PRIu64,
            pb->duration, pb->constant_segment_interval);
    if (!pb->constant_segment_interval) {
      pb->nb_segments = bs_getAleb128(b);
      ia_logd("\tnumber of segment %" PRIu64, pb->nb_segments);
      pb->segments = IAMF_MALLOCZ(ParameterSegment, pb->nb_segments);
      if (!pb->segments) return IAMF_ERR_ALLOC_FAIL;
      for (int i = 0; i < pb->nb_segments; ++i) {
        ia_logd("\tSegment %d: %" PRIu64, i, pb->segments[i].segment_interval);
        pb->segments[i].segment_interval = bs_getAleb128(b);
      }
    } else {
      pb->nb_segments = (pb->duration + pb->constant_segment_interval - 1) /
                        pb->constant_segment_interval;
      ia_logd("\tnumber of segment %" PRIu64, pb->nb_segments);
    }
  }

  return IAMF_OK;
}

IAMF_Element *iamf_element_new(IAMF_OBU *obu) {
  IAMF_Element *elem = 0;
  BitStream b;
  uint32_t val;
  uint64_t type;
  ParameterBase *p = 0;

  elem = IAMF_MALLOCZ(IAMF_Element, 1);
  if (!elem) {
    ia_loge("fail to allocate memory for Audio Element Object.");
    goto element_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  elem->obj.type = IAMF_OBU_AUDIO_ELEMENT;
  elem->element_id = bs_getAleb128(&b);
  elem->element_type = bs_get32b(&b, 3);
  bs_get32b(&b, 5);

  elem->codec_config_id = bs_getAleb128(&b);

  val = bs_getAleb128(&b);
  elem->nb_substreams = val;
  ia_logd("element id %" PRIu64 ", type %d, codec config id %" PRIu64
          ", sub-streams count %" PRIu64,
          elem->element_id, elem->element_type, elem->codec_config_id,
          elem->nb_substreams);
  elem->substream_ids = IAMF_MALLOC(uint64_t, val);
  if (!elem->substream_ids) {
    ia_loge(
        "fail to allocate memory for substream ids of Audio Element Object.");
    goto element_fail;
  }
  for (uint32_t i = 0; i < val; ++i) {
    elem->substream_ids[i] = bs_getAleb128(&b);
    ia_logd("\t > sub-stream id %" PRIu64, elem->substream_ids[i]);
  }

  val = bs_getAleb128(&b);
  elem->nb_parameters = val;
  if (val) {
    elem->parameters = IAMF_MALLOCZ(ParameterBase *, val);
    if (!elem->parameters) {
      ia_loge(
          "fail to allocate memory for parameters of Audio Element Object.");
      goto element_fail;
    }
  }
  ia_logd("element parameters count %" PRIu64, elem->nb_parameters);
  for (uint32_t i = 0; i < val; ++i) {
    type = bs_getAleb128(&b);
    p = 0;
    if (type == IAMF_PARAMETER_TYPE_DEMIXING) {
      DemixingParameter *dp = IAMF_MALLOCZ(DemixingParameter, 1);
      p = PARAMETER_BASE(dp);
    } else if (type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      ReconGainParameter *rgp = IAMF_MALLOCZ(ReconGainParameter, 1);
      p = PARAMETER_BASE(rgp);
    } else {
      uint64_t size = bs_getAleb128(&b);
      bs_skipABytes(&b, size);
      ia_logw("Don't support parameter type %" PRIu64
              " in Audio Element %" PRId64
              ", parameter definition bytes %" PRIu64 ".",
              type, elem->element_id, size);
      continue;
    }

    if (!p) {
      ia_loge(
          "fail to allocate memory for parameter object of Audio Element "
          "Object.");
      goto element_fail;
    }
    elem->parameters[i] = p;
    if (iamf_parameter_base_init(p, type, &b) != IAMF_OK) goto element_fail;

    if (type == IAMF_PARAMETER_TYPE_DEMIXING) {
      DemixingParameter *dp = (DemixingParameter *)p;
      dp->mode = bs_get32b(&b, 3);
      bs_skip(&b, 5);
      dp->w = bs_get32b(&b, 4);
      bs_skip(&b, 4);
      ia_logd("default mode is %d, weight index %d", dp->mode & U8_MASK,
              dp->w & U8_MASK);
    }
  }

  if (elem->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ScalableChannelLayoutConf *chs_conf;
    int channels = 0;
    chs_conf = IAMF_MALLOCZ(ScalableChannelLayoutConf, 1);
    if (!chs_conf) {
      ia_loge(
          "fail to allocate memory for scalable channel config of Audio "
          "Element Object.");
      goto element_fail;
    }
    elem->channels_conf = chs_conf;

    val = bs_get32b(&b, 3);
    bs_skip(&b, 5);
    chs_conf->num_layers = val;
    ia_logd("scalable channel layers %d", chs_conf->num_layers);
    if (val) {
      ChannelLayerConf *layer_conf_s;
      layer_conf_s = IAMF_MALLOCZ(ChannelLayerConf, val);
      if (!layer_conf_s) {
        ia_loge(
            "fail to allocate memory for channel layer config of Audio Element "
            "Object.");
        goto element_fail;
      }
      chs_conf->layer_conf_s = layer_conf_s;
      for (uint32_t i = 0; i < val; ++i) {
        layer_conf_s[i].loudspeaker_layout = bs_get32b(&b, 4);
        layer_conf_s[i].output_gain_flag = bs_get32b(&b, 1);
        layer_conf_s[i].recon_gain_flag = bs_get32b(&b, 1);
        layer_conf_s[i].nb_substreams = bs_getA8b(&b);
        layer_conf_s[i].nb_coupled_substreams = bs_getA8b(&b);
        channels += (layer_conf_s[i].nb_substreams +
                     layer_conf_s[i].nb_coupled_substreams);
        if (chs_conf->nb_layers == i) {
          uint8_t loudspeaker_layout = layer_conf_s[i].loudspeaker_layout;
          if (ia_channel_layout_type_check(loudspeaker_layout) &&
              (layer_conf_s[i].nb_substreams > 0) &&
              (ia_channel_layout_get_channels_count(loudspeaker_layout) ==
               channels)) {
            ++chs_conf->nb_layers;
          } else {
            ia_logw("element (%" PRId64
                    ") Layer %d: Invalid loudspeaker layout %d",
                    elem->element_id, i, layer_conf_s[i].loudspeaker_layout);
          }
        }

        ia_logd(
            "\tlayer[%d] info: layout %d, output gain %d, recon gain %d, "
            "sub-streams count %d, coupled sub-streams %d",
            i, layer_conf_s[i].loudspeaker_layout,
            layer_conf_s[i].output_gain_flag, layer_conf_s[i].recon_gain_flag,
            layer_conf_s[i].nb_substreams,
            layer_conf_s[i].nb_coupled_substreams);

        if (layer_conf_s[i].output_gain_flag) {
          OutputGain *g = IAMF_MALLOCZ(OutputGain, 1);
          if (!g) {
            ia_loge(
                "fail to allocate memory for out gain of Audio Element "
                "Object.");
            goto element_fail;
          }
          layer_conf_s[i].output_gain_info = g;
          g->output_gain_flag = bs_get32b(&b, 6);
          g->output_gain = (int16_t)bs_getA16b(&b);
          ia_logd("\toutput gain : flag 0x%x, gain 0x%x",
                  g->output_gain_flag & U8_MASK, g->output_gain & U16_MASK);
        }
      }
      ia_logd("valid scalable channel layers %d", chs_conf->nb_layers);
    }
  } else if (elem->element_type == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
    AmbisonicsConf *conf = IAMF_MALLOCZ(AmbisonicsConf, 1);
    if (!conf) {
      ia_loge(
          "fail to allocate memory for ambisonics config of Audio Element "
          "Object.");
      goto element_fail;
    }

    elem->ambisonics_conf = conf;
    conf->ambisonics_mode = bs_getAleb128(&b);
    if (conf->ambisonics_mode == AMBISONICS_MODE_MONO) {
      conf->output_channel_count = bs_getA8b(&b);
      conf->substream_count = bs_getA8b(&b);
      conf->mapping_size = conf->output_channel_count;
      conf->mapping = IAMF_MALLOCZ(uint8_t, conf->mapping_size);
      if (!conf->mapping) {
        ia_loge(
            "fail to allocate memory for mono mapping of Audio Element "
            "Object.");
        goto element_fail;
      }
      bs_read(&b, conf->mapping, conf->mapping_size);
      ia_logd(
          "Ambisonics mode mono, channels %d, sub-stream %d, mapping size %d",
          conf->output_channel_count, conf->substream_count,
          conf->mapping_size);
    } else if (conf->ambisonics_mode == AMBISONICS_MODE_PROJECTION) {
      conf->output_channel_count = bs_getA8b(&b);
      conf->substream_count = bs_getA8b(&b);
      conf->coupled_substream_count = bs_getA8b(&b);
      conf->mapping_size =
          2 * conf->output_channel_count *
          (conf->substream_count + conf->coupled_substream_count);
      conf->mapping = IAMF_MALLOCZ(uint8_t, conf->mapping_size);
      if (!conf->mapping) {
        ia_loge(
            "fail to allocate memory for projection mapping of Audio Element "
            "Object.");
        goto element_fail;
      }
      bs_read(&b, conf->mapping, conf->mapping_size);
      ia_logd(
          "Ambisonics mode projection, channels %d, sub-stream %d, coupled "
          "sub-stream %d, matrix (%d x %d) size %d ",
          conf->output_channel_count, conf->substream_count,
          conf->coupled_substream_count, conf->output_channel_count,
          conf->substream_count + conf->coupled_substream_count,
          conf->mapping_size);
    } else {
      ia_logw("audio element object: id %" PRIu64
              ", invalid ambisonics mode %" PRIu64,
              elem->element_id, conf->ambisonics_mode);
      goto element_fail;
    }
  } else {
    uint64_t size = bs_getAleb128(&b);
    bs_skipABytes(&b, size);
    ia_logw("audio element object: id %" PRIu64
            ", Don't support type %u, element config "
            "bytes %" PRIu64,
            elem->element_id, elem->element_type, size);
  }

#if SUPPORT_VERIFIER
  vlog_obu(IAMF_OBU_AUDIO_ELEMENT, elem, 0, 0);
#endif
  return elem;

element_fail:
  if (elem) iamf_element_free(elem);
  return 0;
}

void iamf_element_free(IAMF_Element *obj) {
  IAMF_FREE(obj->substream_ids);

  if (obj->parameters) {
    for (int i = 0; i < obj->nb_parameters; ++i) {
      if (obj->parameters[i]) {
        IAMF_FREE(obj->parameters[i]->segments);
        free(obj->parameters[i]);
      }
    }
    free(obj->parameters);
  }

  if (obj->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
      obj->channels_conf) {
    ScalableChannelLayoutConf *conf = obj->channels_conf;
    if (conf->layer_conf_s) {
      for (int i = 0; i < conf->num_layers; ++i) {
        IAMF_FREE(conf->layer_conf_s[i].output_gain_info);
      }
      free(conf->layer_conf_s);
    }
    free(obj->channels_conf);
  } else if (obj->element_type == AUDIO_ELEMENT_TYPE_SCENE_BASED &&
             obj->ambisonics_conf) {
    IAMF_FREE(obj->ambisonics_conf->mapping);
    free(obj->ambisonics_conf);
  }

  free(obj);
}

IAMF_MixPresentation *iamf_mix_presentation_new(IAMF_OBU *obu) {
  IAMF_MixPresentation *mixp = 0;
  SubMixPresentation *sub = 0;
  OutputMixConf *output_mix_config;
  ElementConf *conf_s;
  BitStream b;
  uint32_t val;
  uint64_t size;
  int length = STRING_SIZE;

  mixp = IAMF_MALLOCZ(IAMF_MixPresentation, 1);
  if (!mixp) {
    ia_loge("fail to allocate memory for Mix Presentation Object.");
    goto mix_presentation_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  mixp->obj.type = IAMF_OBU_MIX_PRESENTATION;
  mixp->mix_presentation_id = bs_getAleb128(&b);
  mixp->num_labels = bs_getAleb128(&b);

  mixp->language = IAMF_MALLOCZ(char *, mixp->num_labels);
  mixp->mix_presentation_friendly_label =
      IAMF_MALLOCZ(char *, mixp->num_labels);

  if (!mixp->language || !mixp->mix_presentation_friendly_label) {
    ia_logd(
        "fail to allocate memory for languages or labels of Mix Presentation "
        "Object.");
    goto mix_presentation_fail;
  }

  if (length > iamf_obu_get_payload_size(obu))
    length = iamf_obu_get_payload_size(obu);

  for (int i = 0; i < mixp->num_labels; ++i) {
    mixp->language[i] = IAMF_MALLOCZ(char, length);
    mixp->mix_presentation_friendly_label[i] = IAMF_MALLOCZ(char, length);
    if (!mixp->language[i] || !mixp->mix_presentation_friendly_label[i]) {
      ia_logd(
          "fail to allocate memory for language or label of Mix Presentation "
          "Object.");
      goto mix_presentation_fail;
    }
  }

  for (int i = 0; i < mixp->num_labels; ++i)
    bs_readString(&b, mixp->language[i], length);

  for (int i = 0; i < mixp->num_labels; ++i)
    bs_readString(&b, mixp->mix_presentation_friendly_label[i], length);

  mixp->num_sub_mixes = bs_getAleb128(&b);
  ia_logd("Mix Presentation Object : id %" PRIu64 ", number of label %" PRIu64
          ", number of sub "
          "mixes %" PRIu64 ".",
          mixp->mix_presentation_id, mixp->num_labels, mixp->num_sub_mixes);

  if (!mixp->num_sub_mixes) {
    ia_logw("Mix Presentation Object: num_sub_mixes should not be set to 0.");
    goto mix_presentation_fail;
  } else if (mixp->num_sub_mixes > IAMF_MIX_PRESENTATION_MAX_SUBS) {
    ia_logw(
        "Mix Presentation Object: Do not support num_sub_mixes more than %u.",
        IAMF_MIX_PRESENTATION_MAX_SUBS);
    goto mix_presentation_fail;
  }

  ia_logd("languages: ");
  for (int i = 0; i < mixp->num_labels; ++i) ia_logd("\t%s", mixp->language[i]);

  ia_logd("mix presentation friendly labels: ");
  for (int i = 0; i < mixp->num_labels; ++i)
    ia_logd("\t%s", mixp->mix_presentation_friendly_label[i]);

  if (mixp->num_sub_mixes != 1) {
    ia_loge("the total of sub mixes should be 1, not support %" PRIu64,
            mixp->num_sub_mixes);
    goto mix_presentation_fail;
  }

  // sub mixes;
  mixp->sub_mixes = IAMF_MALLOCZ(SubMixPresentation, mixp->num_sub_mixes);
  if (!mixp->sub_mixes) {
    ia_loge(
        "fail to allocate memory for sub mix presentation of Mix Presentation "
        "Object.");
    goto mix_presentation_fail;
  }

  if (iamf_obu_get_payload_size(obu) > STRING_SIZE)
    length = iamf_obu_get_payload_size(obu);
  else
    length = STRING_SIZE;

  for (int n = 0; n < mixp->num_sub_mixes; ++n) {
    sub = &mixp->sub_mixes[n];

    val = bs_getAleb128(&b);
    sub->nb_elements = val;
    ia_logd("element count %" PRIu64, sub->nb_elements);
    if (!val) {
      ia_loge(
          "Mix Presentation Object: num_audio_elements should not be set to "
          "0.");
      goto mix_presentation_fail;
    }

    conf_s = IAMF_MALLOCZ(ElementConf, val);
    if (!conf_s) {
      ia_loge(
          "fail to allocate memory for mixing and rendering config of Mix "
          "Presentation Object.");
      goto mix_presentation_fail;
    }
    sub->conf_s = conf_s;
    for (uint32_t i = 0; i < val; ++i) {
      conf_s[i].element_id = bs_getAleb128(&b);
      conf_s[i].audio_element_friendly_label =
          IAMF_MALLOCZ(char *, mixp->num_labels);
      if (!conf_s[i].audio_element_friendly_label) {
        ia_loge(
            "fail to allocate memory for audio element labels of mixing and "
            "rendering config.");
        goto mix_presentation_fail;
      }

      for (int k = 0; k < mixp->num_labels; ++k) {
        conf_s[i].audio_element_friendly_label[k] = IAMF_MALLOCZ(char, length);
        if (!conf_s[i].audio_element_friendly_label[k]) {
          ia_loge(
              "fail to allocate memory for audio element label of mixing and "
              "rendering config.");
          goto mix_presentation_fail;
        }

        bs_readString(&b, conf_s[i].audio_element_friendly_label[k], length);
      }
      ia_logd("rendering info : element id %" PRIu64
              ", audio element friendly labels:",
              conf_s[i].element_id);

      for (int k = 0; k < mixp->num_labels; ++k)
        ia_logd("\t%s", conf_s[i].audio_element_friendly_label[k]);

      // rendering_config
      conf_s[i].conf_r.headphones_rendering_mode = bs_get32b(&b, 2);
      conf_s[i].conf_r.rendering_config_extension_size = size =
          bs_getAleb128(&b);
      bs_skipABytes(&b, size);
      ia_logd(
          "rendering config info: headphones rendering mode %u, extension size "
          "%" PRIu64,
          conf_s[i].conf_r.headphones_rendering_mode, size);

      // element_mix_config
      if (iamf_parameter_base_init(&conf_s[i].conf_m.gain.base,
                                   IAMF_PARAMETER_TYPE_MIX_GAIN, &b) != IAMF_OK)
        goto mix_presentation_fail;
      conf_s[i].conf_m.gain.mix_gain = (short)bs_getA16b(&b);
      ia_logd("element mix info : element mix gain 0x%x",
              conf_s[i].conf_m.gain.mix_gain & U16_MASK);
    }

    // output_mix_config
    output_mix_config = &sub->output_mix_config;

    if (iamf_parameter_base_init(&output_mix_config->gain.base,
                                 IAMF_PARAMETER_TYPE_MIX_GAIN, &b) != IAMF_OK)
      goto mix_presentation_fail;
    output_mix_config->gain.mix_gain = bs_getA16b(&b);

    sub->num_layouts = bs_getAleb128(&b);
    ia_logd("Output mix gain: id %" PRIu64 ", time base %" PRIu64
            ", mix gain 0x%x, number layout "
            "%" PRIu64,
            output_mix_config->gain.base.id, output_mix_config->gain.base.rate,
            output_mix_config->gain.mix_gain & U16_MASK, sub->num_layouts);
    if (sub->num_layouts > 0) {
      TargetLayout **layouts = IAMF_MALLOCZ(TargetLayout *, sub->num_layouts);
      IAMF_LoudnessInfo *loudness =
          IAMF_MALLOCZ(IAMF_LoudnessInfo, sub->num_layouts);
      uint32_t type = 0;
      sub->layouts = layouts;
      sub->loudness = loudness;
      if (!layouts || !loudness) {
        ia_loge(
            "fail to allocate memory for layout and loudness of Mix "
            "Presentation Object.");
        goto mix_presentation_fail;
      }
      for (int i = 0; i < sub->num_layouts; i++) {
        // Layout
        type = bs_get32b(&b, 2);
        if (type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
          SoundSystemLayout *ss = IAMF_MALLOCZ(SoundSystemLayout, 1);
          if (!ss) {
            ia_loge(
                "fail to allocate memory for sound system layout of Mix "
                "Presentation Object.");
            goto mix_presentation_fail;
          }
          layouts[i] = TARGET_LAYOUT(ss);
          ss->base.type = IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
          ss->sound_system = bs_get32b(&b, 4);
          ia_logd("\tLayout %d > sound system %d", i, ss->sound_system);
        } else if (type == IAMF_LAYOUT_TYPE_BINAURAL) {
          BinauralLayout *b = IAMF_MALLOCZ(BinauralLayout, 1);
          if (!b) {
            ia_loge(
                "fail to allocate memory for binaural layout of Mix "
                "Presentation Object.");
            goto mix_presentation_fail;
          }
          b->base.type = IAMF_LAYOUT_TYPE_BINAURAL;
          layouts[i] = TARGET_LAYOUT(b);
          ia_logd("\tLayout %d > binaural.", i);
        } else {
          ia_logw("Undefine layout type %d in mix presentation %" PRId64 ".",
                  type, mixp->mix_presentation_id);
        }
        bs_align(&b);

        // loudness
        loudness[i].info_type = bs_getA8b(&b);
        loudness[i].integrated_loudness = (int16_t)bs_getA16b(&b);
        loudness[i].digital_peak = (int16_t)bs_getA16b(&b);
        ia_logd(
            "\tLoudness : %d > info type 0x%x, integrated loudness 0x%x, "
            "digital peak 0x%x",
            i, loudness[i].info_type & U8_MASK,
            loudness[i].integrated_loudness & U16_MASK,
            loudness[i].digital_peak & U16_MASK);

#define LOUDNESS_INFO_TYPE_TRUE_PEAK 1
#define LOUDNESS_INFO_TYPE_ANCHORED 2
#define LOUDNESS_INFO_TYPE_ALL \
  (LOUDNESS_INFO_TYPE_TRUE_PEAK | LOUDNESS_INFO_TYPE_ANCHORED)

        if (loudness[i].info_type & LOUDNESS_INFO_TYPE_TRUE_PEAK) {
          loudness[i].true_peak = bs_getA16b(&b);
          ia_logd("\tloudness > %d > true peak 0x%x", i,
                  loudness[i].true_peak & U16_MASK);
        }

        if (loudness[i].info_type & LOUDNESS_INFO_TYPE_ANCHORED) {
          loudness[i].num_anchor_loudness = bs_getA8b(&b);
          if (loudness[i].num_anchor_loudness > 0) {
            loudness[i].anchor_loudness = IAMF_MALLOCZ(
                anchor_loudness_t, loudness[i].num_anchor_loudness);
            if (!loudness[i].anchor_loudness) {
              ia_loge(
                  "fail to allocate memory anchor loudness in loudness info.");
              goto mix_presentation_fail;
            }

            ia_logd("\tloudness > %d > number of anchor loudness %d", i,
                    loudness[i].num_anchor_loudness);
            for (int k = 0; k < loudness[i].num_anchor_loudness; ++k) {
              loudness[i].anchor_loudness[k].anchor_element = bs_getA8b(&b);
              loudness[i].anchor_loudness[k].anchored_loudness = bs_getA16b(&b);
              ia_logd("\t\tanchor loudness > %d > anchor element %u", k,
                      loudness[i].anchor_loudness[k].anchor_element);
              ia_logd(
                  "\t\tanchor loudness > %d > anchored loudness 0x%x", k,
                  loudness[i].anchor_loudness[k].anchored_loudness & U16_MASK);
            }
          }
        }

        if (loudness[i].info_type & ~LOUDNESS_INFO_TYPE_ALL) {
          size = bs_getAleb128(&b);
          bs_skipABytes(&b, size);
          ia_logd("extension loudness info size %" PRIu64, size);
        }
      }
    }
  }

#if SUPPORT_VERIFIER
  vlog_obu(IAMF_OBU_MIX_PRESENTATION, mixp, 0, 0);
#endif
  return mixp;

mix_presentation_fail:
  if (mixp) iamf_mix_presentation_free(mixp);
  return 0;
}

void iamf_mix_presentation_free(IAMF_MixPresentation *obj) {
  if (obj->language) {
    for (int i = 0; i < obj->num_labels; ++i) IAMF_FREE(obj->language[i]);
    free(obj->language);
  }

  if (obj->mix_presentation_friendly_label) {
    for (int i = 0; i < obj->num_labels; ++i)
      IAMF_FREE(obj->mix_presentation_friendly_label[i]);
    free(obj->mix_presentation_friendly_label);
  }

  if (obj->sub_mixes) {
    SubMixPresentation *sub;
    for (int i = 0; i < obj->num_sub_mixes; ++i) {
      sub = &obj->sub_mixes[i];

      if (sub->conf_s) {
        for (int i = 0; i < sub->nb_elements; ++i) {
          if (sub->conf_s[i].audio_element_friendly_label) {
            for (int k = 0; k < obj->num_labels; ++k)
              IAMF_FREE(sub->conf_s[i].audio_element_friendly_label[k]);
            free(sub->conf_s[i].audio_element_friendly_label);
          }
          IAMF_FREE(sub->conf_s[i].conf_m.gain.base.segments);
        }
        free(sub->conf_s);
      }

      IAMF_FREE(sub->output_mix_config.gain.base.segments);

      if (sub->layouts) {
        for (int i = 0; i < sub->num_layouts; ++i) {
          free(sub->layouts[i]);
        }
        free(sub->layouts);
      }

      if (sub->loudness) {
        for (int i = 0; i < sub->num_layouts; ++i)
          IAMF_FREE(sub->loudness[i].anchor_loudness);
      }
      IAMF_FREE(sub->loudness);
    }

    free(obj->sub_mixes);
  }

  free(obj);
}

static uint64_t iamf_parameter_get_segment_interval(uint64_t duration,
                                                    uint64_t const_interval,
                                                    uint64_t interval) {
  if (interval) return interval;
  return const_interval < duration ? const_interval : duration;
}

IAMF_Parameter *iamf_parameter_new(IAMF_OBU *obu,
                                   IAMF_ParameterParam *objParam) {
  IAMF_Parameter *para = 0;
  ParameterSegment *seg;
  BitStream b;
  uint64_t interval = 0;
  uint64_t intervals;
  uint64_t segment_interval;

  para = IAMF_MALLOCZ(IAMF_Parameter, 1);
  if (!para) {
    ia_loge("fail to allocate memory for Parameter Object.");
    goto parameter_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  para->obj.type = IAMF_OBU_PARAMETER_BLOCK;
  para->id = bs_getAleb128(&b);

  if (!objParam || !objParam->param_base) {
    // ia_loge("parameter object(%" PRIu64
    //         "): Invalid object parameters for Parameter "
    //         "Object.",
    //         para->id);
    goto parameter_fail;
  }

  ia_logd("parameter obu arguments: parameter type %" PRIu64,
          objParam->param_base->type);

  if (!objParam->param_base->mode) {
    intervals = para->duration = objParam->param_base->duration;
    para->nb_segments = objParam->param_base->nb_segments;
    para->constant_segment_interval =
        objParam->param_base->constant_segment_interval;
  } else {
    intervals = para->duration = bs_getAleb128(&b);
    para->constant_segment_interval = bs_getAleb128(&b);
    if (!para->constant_segment_interval) {
      para->nb_segments = bs_getAleb128(&b);
    } else {
      para->nb_segments =
          (para->duration + para->constant_segment_interval - 1) /
          para->constant_segment_interval;
    }
  }
  para->type = objParam->param_base->type;

  ia_logd("parameter id %" PRIu64 ", duration %" PRIu64
          ", segment count %" PRIu64
          ", const segment "
          "interval %" PRIu64 ", type %" PRIu64,
          para->id, para->duration, para->nb_segments,
          para->constant_segment_interval, para->type);

  para->segments = IAMF_MALLOCZ(ParameterSegment *, para->nb_segments);
  if (!para->segments) {
    ia_loge("fail to allocate segments for Parameter Object.");
    goto parameter_fail;
  }

  for (int i = 0; i < para->nb_segments; ++i) {
    if (!objParam->param_base->mode) {
      if (!para->constant_segment_interval) {
        interval = objParam->param_base->segments[i].segment_interval;
        ia_logd("parameter base segment interval %" PRIu64, interval);
      }
    } else {
      if (!para->constant_segment_interval) {
        interval = bs_getAleb128(&b);
        ia_logd("segment interval %" PRIu64, interval);
      }
    }

    segment_interval = iamf_parameter_get_segment_interval(
        intervals, para->constant_segment_interval, interval);
    intervals -= segment_interval;

    switch (para->type) {
      case IAMF_PARAMETER_TYPE_MIX_GAIN: {
        MixGainSegment *mg = IAMF_MALLOCZ(MixGainSegment, 1);
        float gain_db, gain1_db, gain2_db;
        if (!mg) {
          ia_loge("fail to allocate mix gain segment for Parameter Object.");
          goto parameter_fail;
        }

        mg->seg.type = IAMF_PARAMETER_TYPE_MIX_GAIN;
        seg = (ParameterSegment *)mg;
        para->segments[i] = seg;
        seg->segment_interval = segment_interval;
        mg->mix_gain_f.animated_type = mg->mix_gain.animated_type =
            bs_getAleb128(&b);
        mg->mix_gain.start = (short)bs_getA16b(&b);
        gain_db = q_to_float(mg->mix_gain.start, 8);
        mg->mix_gain_f.start = db2lin(gain_db);
        if (mg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_STEP) {
          ia_logd("\t mix gain seg %d: interval %" PRIu64
                  ", step, start %f(%fdb, "
                  "<0x%02x>)",
                  i, seg->segment_interval, mg->mix_gain_f.start, gain_db,
                  mg->mix_gain.start & U16_MASK);
        } else {
          mg->mix_gain.end = (short)bs_getA16b(&b);
          gain2_db = q_to_float(mg->mix_gain.end, 8);
          mg->mix_gain_f.end = db2lin(gain2_db);
          if (mg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_LINEAR) {
            ia_logd("\t mix gain seg %d: interval %" PRIu64
                    ", linear, start %f(%fdb, "
                    "<0x%02x>), end %f(%fdb, <0x%02x>)",
                    i, seg->segment_interval, mg->mix_gain_f.start, gain_db,
                    mg->mix_gain.start & U16_MASK, mg->mix_gain_f.end, gain2_db,
                    mg->mix_gain.end & U16_MASK);
          } else if (mg->mix_gain.animated_type ==
                     PARAMETER_ANIMATED_TYPE_BEZIER) {
            mg->mix_gain.control = bs_getA16b(&b);
            gain1_db = q_to_float(mg->mix_gain.control, 8);
            mg->mix_gain_f.control = db2lin(gain1_db);
            mg->mix_gain.control_relative_time = bs_getA8b(&b);
            mg->mix_gain_f.control_relative_time =
                qf_to_float(mg->mix_gain.control_relative_time, 8);
            ia_logd("\t mix gain seg %d: interval %" PRIu64
                    ", bezier, start %f (%fdb "
                    "<0x%02x>), end %f (%fdb <0x%02x>), control %f (%fdb "
                    "<0x%02x>), control relative time %f (0x%x)",
                    i, seg->segment_interval, mg->mix_gain_f.start, gain_db,
                    mg->mix_gain.start & U16_MASK, mg->mix_gain_f.end, gain2_db,
                    mg->mix_gain.end & U16_MASK, mg->mix_gain_f.control,
                    gain1_db, mg->mix_gain.control & U16_MASK,
                    mg->mix_gain_f.control_relative_time,
                    mg->mix_gain.control_relative_time & U8_MASK);
          }
        }
      } break;
      case IAMF_PARAMETER_TYPE_DEMIXING: {
        DemixingSegment *mode = IAMF_MALLOC(DemixingSegment, 1);
        if (!mode) {
          ia_loge("fail to allocate demixing segment for Parameter Object.");
          goto parameter_fail;
        }
        mode->seg.type = IAMF_PARAMETER_TYPE_DEMIXING;
        seg = (ParameterSegment *)mode;
        para->segments[i] = seg;
        seg->segment_interval = segment_interval;
        mode->demixing_mode = bs_get32b(&b, 3);
        ia_logd("segment interval %" PRIu64 ", demixing mode : %d",
                seg->segment_interval, mode->demixing_mode);
      } break;
      case IAMF_PARAMETER_TYPE_RECON_GAIN: {
        ia_logd("recon gain, layer count %d", objParam->nb_layers);

        if (objParam->nb_layers > 0) {
          int count = objParam->nb_layers;
          ReconGainList *list;
          ReconGain *recon;
          ReconGainSegment *recon_gain;
          int channels = 0;

          recon_gain = IAMF_MALLOCZ(ReconGainSegment, 1);
          if (!recon_gain) {
            ia_loge(
                "fail to allocate Recon gain segment for Parameter Object.");
            goto parameter_fail;
          }

          recon_gain->seg.type = IAMF_PARAMETER_TYPE_RECON_GAIN;
          list = &recon_gain->list;

          seg = (ParameterSegment *)recon_gain;
          para->segments[i] = seg;
          seg->segment_interval = segment_interval;
          ia_logd("there are %d recon gain info, list is %p", count, list);
          list->count = count;
          recon = IAMF_MALLOCZ(ReconGain, list->count);
          list->recon = recon;
          if (!recon) {
            ia_loge("fail to allocate Recon gain for Parameter Object.");
            goto parameter_fail;
          }
          for (int k = 0; k < list->count; ++k) {
            if (~objParam->recon_gain_present_flags & RSHIFT(k)) continue;
            recon[k].flags = bs_getAleb128(&b);
            channels = bit1_count(recon[k].flags);
            if (channels > 0) {
              recon[k].recon_gain = IAMF_MALLOCZ(uint8_t, channels);
              recon[k].recon_gain_f = IAMF_MALLOCZ(float, channels);
              if (!recon[k].recon_gain || !recon[k].recon_gain_f) {
                ia_loge(
                    "fail to allocate recon gain value for Parameter Object.");
                goto parameter_fail;
              }
              bs_read(&b, recon[k].recon_gain, channels);
              ia_logd("recon gain info %d : flags 0x%x, channels %d", k,
                      recon[k].flags, channels);
              for (int t = 0; t < channels; ++t) {
                recon[k].recon_gain_f[t] =
                    qf_to_float(recon[k].recon_gain[t], 8);
                ia_logd("\tch %d gain %f(0x%x)", t, recon[k].recon_gain_f[t],
                        recon[k].recon_gain[t] & U8_MASK);
              }
            }
          }
        }
      } break;
      default: {
        uint64_t size = bs_getAleb128(&b);
        bs_skipABytes(&b, size);
        ia_logw("parameter %" PRIu64 ", don't support extension type %" PRIu64
                ", and parameter "
                "data bytes %" PRIu64 " .",
                para->id, para->type, size);
      } break;
    }
  }

#if SUPPORT_VERIFIER
  vlog_obu(IAMF_OBU_PARAMETER_BLOCK, para, 0, 0);
#endif

  return para;

parameter_fail:
  if (para) iamf_parameter_free(para);
  return 0;
}

void iamf_parameter_free(IAMF_Parameter *obj) {
  if (obj->segments) {
    for (int i = 0; i < obj->nb_segments; ++i) {
      IAMF_parameter_segment_free(obj->segments[i]);
    }
    free(obj->segments);
  }
  free(obj);
}

IAMF_Frame *iamf_frame_new(IAMF_OBU *obu) {
  IAMF_Frame *pkt = 0;
  BitStream b;

  pkt = IAMF_MALLOCZ(IAMF_Frame, 1);
  if (!pkt) {
    ia_loge("fail to allocate memory for Audio Frame Object.");
    return 0;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  pkt->obj.type = obu->type;
  if (obu->type == IAMF_OBU_AUDIO_FRAME) {
    pkt->id = bs_getAleb128(&b);
  } else {
    pkt->id = obu->type - IAMF_OBU_AUDIO_FRAME_ID0;
  }
  pkt->trim_start = obu->trim_start;
  pkt->trim_end = obu->trim_end;
  pkt->data = obu->payload + bs_tell(&b);
  pkt->size = iamf_obu_get_payload_size(obu) - bs_tell(&b);

#if SUPPORT_VERIFIER
  vlog_obu(IAMF_OBU_AUDIO_FRAME, pkt, obu->trim_start, obu->trim_end);
#endif
  return pkt;
}

void iamf_parameter_recon_gain_segment_free(ReconGainSegment *seg) {
  if (seg->list.recon) {
    for (int i = 0; i < seg->list.count; ++i) {
      IAMF_FREE(seg->list.recon[i].recon_gain);
      IAMF_FREE(seg->list.recon[i].recon_gain_f);
    }
    free(seg->list.recon);
  }
  free(seg);
}
