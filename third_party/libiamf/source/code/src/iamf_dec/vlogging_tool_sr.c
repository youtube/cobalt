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
 * @file vlogging_tool_sr.h
 * @brief verification log generator.
 * @version 0.1
 * @date Created 03/29/2023
 **/

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IAMF_OBU.h"
#include "IAMF_utils.h"
#include "bitstream.h"
#include "vlogging_tool_sr.h"

#define LOG_BUFFER_SIZE 100000

typedef struct vlogdata {
  int log_type;
  uint64_t key;
  struct vlogdata* prev;
  struct vlogdata* next;
  int is_longtext;
  char* ltext;
  char text[256];
} VLOG_DATA;

typedef struct vlog_file {
  FILE* f;
  char file_name[FILENAME_MAX];
  int is_open;
  VLOG_DATA* head[MAX_LOG_TYPE];
} VLOG_FILE;

static VLOG_FILE log_file = {
    0,
};

static uint32_t get_4cc_codec_id(char a, char b, char c, char d) {
  return ((a) | (b << 8) | (c << 16) | (d << 24));
}

static uint32_t htonl2(uint32_t x) {
  return (x >> 24) | ((x >> 8) & 0x0000FF00) | ((x << 8) & 0x00FF0000) |
         (x << 24);
}

static uint64_t htonll2(uint64_t x) {
  return ((((uint64_t)htonl2((uint32_t)x & 0xFFFFFFFF)) << 32) +
          htonl2((uint32_t)(x >> 32)));
}

int vlog_file_open(const char* log_file_name) {
  int i;
  unsigned char bom[] = {0xEF, 0xBB, 0xBF};
  if (log_file.is_open && strcmp(log_file_name, log_file.file_name) == 0) {
    return 0;
  }

#if defined(_WIN32)
  if (_access(log_file_name, 0) != -1) {
#else
  if (access(log_file_name, 0) != -1) {
#endif
    // already exist
    if (remove(log_file_name) == -1) return -1;
  }

  {
    log_file.f = fopen(log_file_name, "a");
    fwrite(bom, 1, 3, log_file.f);
    strcpy(log_file.file_name, log_file_name);
    log_file.is_open = 1;

    for (i = 0; i < MAX_LOG_TYPE; i++) {
      log_file.head[i] = NULL;
    }

    return 0;
  }
}

int vlog_file_close() {
  int i;
  VLOG_DATA *t_logdata, *t_logdata_free;
  int print_order[MAX_LOG_TYPE] = {LOG_MP4BOX, LOG_OBU, LOG_DECOP};

  if (log_file.f && log_file.is_open) {
    for (i = 0; i < MAX_LOG_TYPE; i++) {
      t_logdata = log_file.head[print_order[i]];
      while (t_logdata) {
        if (t_logdata->is_longtext) {
          fwrite(t_logdata->ltext, 1, strlen(t_logdata->ltext), log_file.f);
          free(t_logdata->ltext);
          t_logdata_free = t_logdata;
          t_logdata = t_logdata->next;
          free(t_logdata_free);
        } else {
          fwrite(t_logdata->text, 1, strlen(t_logdata->text), log_file.f);
          t_logdata_free = t_logdata;
          t_logdata = t_logdata->next;
          free(t_logdata_free);
        }
      }
      log_file.head[print_order[i]] = NULL;
    }

    fclose(log_file.f);
    log_file.f = NULL;
    memset(log_file.file_name, 0, sizeof(log_file.file_name));
    log_file.is_open = 0;

    return 0;
  }

  return -1;
}

int is_vlog_file_open() {
  if (!log_file.f || !log_file.is_open) return 0;
  return (1);
}

int vlog_print(LOG_TYPE type, uint64_t key, const char* format, ...) {
  if (!log_file.f || !log_file.is_open) return -1;

  va_list args;
  int len = 0;
  char* buffer = NULL;
  VLOG_DATA *t_logdata, *logdata_new;

  va_start(args, format);
#if defined(_WIN32)
  len = _vscprintf(format, args) + 1;  // terminateing '\0'
#else
  len = vprintf(format, args) + 1;
#endif

  buffer = (char*)malloc(len * sizeof(char));
  if (buffer) {
#if defined(_WIN32)
    vsprintf_s(buffer, len, format, args);
#else
    vsnprintf(buffer, len, format, args);
#endif
  }
  va_end(args);

  logdata_new = malloc(sizeof(VLOG_DATA));
  if (logdata_new) {
    // set up new logdata
    logdata_new->log_type = type;
    logdata_new->key = key;
    if (len < sizeof(logdata_new->text)) {  // short text log
      strcpy(logdata_new->text, buffer);
      free(buffer);
      logdata_new->is_longtext = 0;
      logdata_new->ltext = NULL;
    } else {  // longtext log
      logdata_new->is_longtext = 1;
      logdata_new->ltext = buffer;
      logdata_new->text[0] = 0;
    }

    // add new logdata into logdata chain
    t_logdata = log_file.head[type];
    if (log_file.head[type] == NULL) {  // head is empty
      logdata_new->prev = NULL;
      logdata_new->next = NULL;
      log_file.head[type] = logdata_new;
    } else {  // head is ocuppied.
      if (logdata_new->key <
          t_logdata->key)  // t_logdata == log_file.head[log_type];
      {                    // head is needed to update
        logdata_new->next = t_logdata->next;
        t_logdata->prev = logdata_new;
        log_file.head[type] = logdata_new;
      } else {  // body or tail is needed to update
        while (t_logdata) {
          if (logdata_new->key < t_logdata->key) {  // add ne logdata into body
            logdata_new->next = t_logdata;
            t_logdata->prev->next = logdata_new;
            logdata_new->prev = t_logdata->prev;
            t_logdata->prev = logdata_new;
            break;
          } else {
            if (t_logdata->next) {  // skip this t_logdata
              t_logdata = t_logdata->next;
            } else {  // add new logdata into tail
              logdata_new->prev = t_logdata;
              t_logdata->next = logdata_new;
              logdata_new->next = NULL;
              break;
            }
          }
        }
      }
    }
  }

  return 0;
}

int write_prefix(LOG_TYPE type, char* buf) {
  int len = 0;

  switch (type) {
    case LOG_OBU:
      len = sprintf(buf, "#0\n");
      break;
    case LOG_MP4BOX:
      len = sprintf(buf, "#1\n");
      break;
    case LOG_DECOP:
      len = sprintf(buf, "#2\n");
      break;
    default:
      break;
  }

  return len;
}

int write_postfix(LOG_TYPE type, char* buf) {
  int len = 0;

  switch (type) {
    case LOG_OBU:
    case LOG_MP4BOX:
    case LOG_DECOP:
      len = sprintf(buf, "##\n");
      break;
    default:
      break;
  }

  return len;
}

int write_yaml_form(char* log, uint8_t indent, const char* format, ...) {
  int ret = 0;
  for (uint8_t i = 0; i < indent; ++i) {
    ret += sprintf(log + ret, "  ");
  }

  va_list args;
  int len = 0;

  va_start(args, format);
#if defined(_WIN32)
  len = _vscprintf(format, args) + 1;  // terminateing '\0'
  vsprintf_s(log + ret, len, format, args);
#else
  len = vprintf(format, args) + 1;
  va_start(args, format);
  vsnprintf(log + ret, len, format, args);
#endif
  va_end(args);

  ret += len - 1;
  ret += sprintf(log + ret, "\n");

  return ret;
}

static void write_sequence_header_log(uint64_t idx, void* obu, char* log) {
  IAMF_Version* mc_obu = (IAMF_Version*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "IaSequenceHeaderOBU_%llu:", idx);
  log += write_yaml_form(log, 0, "- ia_code: %u", htonl2(mc_obu->iamf_code));
  log +=
      write_yaml_form(log, 1, "primary_profile: %u", mc_obu->primary_profile);
  log += write_yaml_form(log, 1, "additional_profile: %u",
                         mc_obu->additional_profile);
  write_postfix(LOG_OBU, log);
}

static void write_codec_config_log(uint64_t idx, void* obu, char* log) {
  IAMF_CodecConf* cc_obu = (IAMF_CodecConf*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "CodecConfigOBU_%llu:", idx);
  log +=
      write_yaml_form(log, 0, "- codec_config_id: %llu", cc_obu->codec_conf_id);
  log += write_yaml_form(log, 1, "codec_config:");
  log += write_yaml_form(log, 2, "codec_id: %u", htonl2(cc_obu->codec_id));
  log += write_yaml_form(log, 2, "num_samples_per_frame: %llu",
                         cc_obu->nb_samples_per_frame);
  log +=
      write_yaml_form(log, 2, "audio_roll_distance: %d", cc_obu->roll_distance);

  // NOTE: Self parsing

  if (cc_obu->codec_id == get_4cc_codec_id('m', 'p', '4', 'a') ||
      cc_obu->codec_id == get_4cc_codec_id('e', 's', 'd', 's')) {
    log += write_yaml_form(log, 2, "decoder_config_aac:");

    BitStream b;
    bs(&b, cc_obu->decoder_conf, cc_obu->decoder_conf_size);

    // decoder_config_descriptor_tag
    // object_type_indication
    // stream_type
    // upstream
    // reserved
    // buffer_size_db
    // max_bitrate
    // average_bit_rate
    // decoder_specific_info
    //    - decoder_specific_info_descriptor_tag
    //    - audio_object_type
    //    - sample_frequency_index
    //    - sampleing_frequency
    //    - channel_configuration
    // ga_specific_config
    //    - frame_length_flag
    //    - depends_on_core_coder
    //    - extension_flag

    log += write_yaml_form(log, 3, "decoder_config_descriptor_tag: %u",
                           bs_get32b(&b, 8));
    log +=
        write_yaml_form(log, 3, "object_type_indication: %u", bs_get32b(&b, 8));
    log += write_yaml_form(log, 3, "stream_type: %u", bs_get32b(&b, 6));
    log += write_yaml_form(log, 3, "upstream: %u", bs_get32b(&b, 1));
    bs_skip(&b, 1);  // reserved

    bs_get32b(&b, 24);  // buffer_size_db
    bs_get32b(&b, 32);  // max_bitrate
    bs_get32b(&b, 32);  // average_bit_rate

    log += write_yaml_form(log, 3, "decoder_specific_info:");
    log += write_yaml_form(log, 4, "decoder_specific_info_descriptor_tag: %u",
                           bs_get32b(&b, 8));
    log += write_yaml_form(log, 4, "audio_object_type: %u", bs_get32b(&b, 5));
    uint8_t sample_frequency_index =
        bs_get32b(&b, 4);  // sample_frequency_index
    if (sample_frequency_index == 0xf) {
      bs_get32b(&b, 24);  // sampling_frequency
    }
    log +=
        write_yaml_form(log, 4, "channel_configuration: %u", bs_get32b(&b, 4));

    log += write_yaml_form(log, 3, "ga_specific_config:");
    log += write_yaml_form(log, 4, "frame_length_flag: %u", bs_get32b(&b, 1));
    log +=
        write_yaml_form(log, 4, "depends_on_core_coder: %u", bs_get32b(&b, 1));
    log += write_yaml_form(log, 4, "extension_flag: %u", bs_get32b(&b, 1));
  } else if (cc_obu->codec_id == get_4cc_codec_id('f', 'L', 'a', 'C')) {
    // "fLaC", METADATA_BLOCK

    log += write_yaml_form(log, 2, "decoder_config_flac:");
    log += write_yaml_form(log, 3, "metadata_blocks:");

    BitStream b;
    bs(&b, cc_obu->decoder_conf, cc_obu->decoder_conf_size);

    uint8_t last_metadata_block_flag = 0;
    uint8_t block_type = 0;
    uint32_t metadata_data_block_length = 0;

    do {
      last_metadata_block_flag = bs_get32b(&b, 1);
      block_type = bs_get32b(&b, 7);
      metadata_data_block_length = bs_get32b(&b, 24);

      log += write_yaml_form(log, 4, "- header:");
      log += write_yaml_form(log, 6, "last_metadata_block_flag: %u",
                             last_metadata_block_flag);
      log += write_yaml_form(log, 6, "block_type: %u", block_type);
      log += write_yaml_form(log, 6, "metadata_data_block_length: %lu",
                             metadata_data_block_length);

      // STREAM_INFO
      if (block_type == 0) {
        // <16>
        // <16>
        // <24>
        // <24>
        // <20>
        // <3>
        // <5>
        // <36>
        // <128>

        uint16_t minimum_block_size = bs_get32b(&b, 16);
        uint16_t maximum_block_size = bs_get32b(&b, 16);
        uint32_t minumum_frame_size = bs_get32b(&b, 24);
        uint32_t maximum_frame_size = bs_get32b(&b, 24);
        uint32_t sample_rate = bs_get32b(&b, 20);
        ;
        uint8_t number_of_channels = bs_get32b(&b, 3);
        uint8_t bits_per_sample = bs_get32b(&b, 5);
        uint64_t total_samples_in_stream = 0;
        uint8_t md5_signature[16] = {
            0,
        };

        uint64_t be_value = 0;  // uint8_t be_value[8] = { 0, };
        bs_read(&b, (uint8_t*)&be_value, 4);
        be_value <<= 32;

        // htonll2
        total_samples_in_stream = htonll2(be_value);

        bs_read(&b, (uint8_t*)md5_signature, 16);

        log += write_yaml_form(log, 5, "stream_info:");

        log += write_yaml_form(log, 6, "minimum_block_size: %u",
                               minimum_block_size);
        log += write_yaml_form(log, 6, "maximum_block_size: %u",
                               maximum_block_size);
        log += write_yaml_form(log, 6, "minimum_frame_size: %lu",
                               minumum_frame_size);
        log += write_yaml_form(log, 6, "maximum_frame_size: %lu",
                               maximum_frame_size);
        log += write_yaml_form(log, 6, "sample_rate: %lu", sample_rate);
        log += write_yaml_form(log, 6, "number_of_channels: %u",
                               number_of_channels);
        log += write_yaml_form(log, 6, "bits_per_sample: %u", bits_per_sample);
        log += write_yaml_form(log, 6, "total_samples_in_stream: %llu",
                               total_samples_in_stream);

        char hex_string[20] = {
            0,
        };
        int pos = 0;
        for (int j = 0; j < 16; ++j) {
          pos += sprintf(hex_string + pos, "%X", md5_signature[j]);
        }
        log += write_yaml_form(log, 6, "md5_signature: %s", hex_string);
      }

    } while (!last_metadata_block_flag);

  } else if (cc_obu->codec_id == get_4cc_codec_id('O', 'p', 'u', 's') ||
             cc_obu->codec_id == get_4cc_codec_id('d', 'O', 'p', 's')) {
    uint8_t version = readu8(cc_obu->decoder_conf, 0);
    uint8_t output_channel_count = readu8(cc_obu->decoder_conf, 1);
    uint16_t pre_skip = readu16be(cc_obu->decoder_conf, 2);
    uint32_t input_sample_rate = readu32be(cc_obu->decoder_conf, 4);
    uint16_t output_gain = readu16be(cc_obu->decoder_conf, 8);
    uint8_t channel_mapping_family = readu8(cc_obu->decoder_conf, 10);

    log += write_yaml_form(log, 2, "decoder_config_opus:");
    log += write_yaml_form(log, 3, "version: %u", version);
    log += write_yaml_form(log, 3, "output_channel_count: %u",
                           output_channel_count);
    log += write_yaml_form(log, 3, "pre_skip: %u", pre_skip);
    log += write_yaml_form(log, 3, "input_sample_rate: %lu", input_sample_rate);
    log += write_yaml_form(log, 3, "output_gain: %u", output_gain);
    log +=
        write_yaml_form(log, 3, "mapping_family: %u", channel_mapping_family);
  } else if (cc_obu->codec_id == get_4cc_codec_id('i', 'p', 'c', 'm')) {
    uint8_t sample_format_flags = readu8(cc_obu->decoder_conf, 0);
    uint8_t sample_size = readu8(cc_obu->decoder_conf, 1);
    uint32_t sample_rate = readu32be(cc_obu->decoder_conf, 2);

    log += write_yaml_form(log, 2, "decoder_config_lpcm:");
    log +=
        write_yaml_form(log, 3, "sample_format_flags: %u", sample_format_flags);
    log += write_yaml_form(log, 3, "sample_size: %u", sample_size);
    log += write_yaml_form(log, 3, "sample_rate: %lu", sample_rate);
  }
  write_postfix(LOG_OBU, log);
}

static void write_audio_element_log(uint64_t idx, void* obu, char* log) {
  IAMF_Element* ae_obu = (IAMF_Element*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "AudioElementOBU_%llu:", idx);
  log +=
      write_yaml_form(log, 0, "- audio_element_id: %llu", ae_obu->element_id);
  log +=
      write_yaml_form(log, 1, "audio_element_type: %u", ae_obu->element_type);
  log +=
      write_yaml_form(log, 1, "codec_config_id: %llu", ae_obu->codec_config_id);
  log += write_yaml_form(log, 1, "num_substreams: %llu", ae_obu->nb_substreams);

  log += write_yaml_form(log, 1, "audio_substream_ids:");
  for (uint64_t i = 0; i < ae_obu->nb_substreams; ++i) {
    log += write_yaml_form(log, 1, "- %llu", ae_obu->substream_ids[i]);
  }
  log += write_yaml_form(log, 1, "num_parameters: %llu", ae_obu->nb_parameters);
  if (ae_obu->nb_parameters) {
    log += write_yaml_form(log, 1, "audio_element_params:");
    for (uint64_t i = 0; i < ae_obu->nb_parameters; ++i) {
      ParameterBase* param = ae_obu->parameters[i];
      if (!param) continue;
      log +=
          write_yaml_form(log, 1, "- param_definition_type: %llu", param->type);

      if (param->type == IAMF_PARAMETER_TYPE_DEMIXING) {
        DemixingParameter* dp = (DemixingParameter*)param;
        log += write_yaml_form(log, 2, "demixing_param:");

        log += write_yaml_form(log, 3, "param_definition:");
        log += write_yaml_form(log, 4, "parameter_id: %llu", dp->base.id);
        log += write_yaml_form(log, 4, "parameter_rate: %llu", dp->base.rate);
        log +=
            write_yaml_form(log, 4, "param_definition_mode: %u", dp->base.mode);
        if (dp->base.mode == 0) {
          log += write_yaml_form(log, 4, "duration: %llu", dp->base.duration);
          log += write_yaml_form(log, 4, "num_subblocks: %llu",
                                 dp->base.nb_segments);
          log += write_yaml_form(log, 4, "constant_subblock_duration: %llu",
                                 dp->base.constant_segment_interval);
          if (dp->base.constant_segment_interval == 0) {
            log += write_yaml_form(log, 4, "subblock_durations:");
            for (uint64_t j = 0; j < dp->base.nb_segments; ++j) {
              log += write_yaml_form(log, 4, "- %llu",
                                     dp->base.segments[j].segment_interval);
            }
          }
        }
        log += write_yaml_form(log, 3, "default_demixing_info_parameter_data:");
        log += write_yaml_form(log, 4, "dmixp_mode: %lu", dp->mode);

        log += write_yaml_form(log, 3, "default_w: %lu", dp->w);
      } else if (param->type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
        ReconGainParameter* rp = (ReconGainParameter*)param;
        log += write_yaml_form(log, 2, "recon_gain_param:");

        log += write_yaml_form(log, 3, "param_definition:");
        log += write_yaml_form(log, 4, "parameter_id: %llu", rp->base.id);
        log += write_yaml_form(log, 4, "parameter_rate: %llu", rp->base.rate);
        log +=
            write_yaml_form(log, 4, "param_definition_mode: %u", rp->base.mode);
        if (rp->base.mode == 0) {
          log += write_yaml_form(log, 4, "duration: %llu", rp->base.duration);
          log += write_yaml_form(log, 4, "num_subblocks: %llu",
                                 rp->base.nb_segments);
          log += write_yaml_form(log, 4, "constant_subblock_duration: %llu",
                                 rp->base.constant_segment_interval);
          if (rp->base.constant_segment_interval == 0) {
            log += write_yaml_form(log, 4, "subblock_durations:");
            for (uint64_t j = 0; j < rp->base.nb_segments; ++j) {
              log += write_yaml_form(log, 4, "- %llu",
                                     rp->base.segments[j].segment_interval);
            }
          }
        }
      }
    }
  }

  if (ae_obu->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    log += write_yaml_form(log, 1, "scalable_channel_layout_config:");
    log += write_yaml_form(log, 2, "num_layers: %u",
                           ae_obu->channels_conf->nb_layers);
    log += write_yaml_form(log, 2, "channel_audio_layer_configs:");
    for (uint32_t i = 0; i < ae_obu->channels_conf->nb_layers; ++i) {
      ChannelLayerConf* layer_conf = &ae_obu->channels_conf->layer_conf_s[i];
      log += write_yaml_form(log, 2, "- loudspeaker_layout: %u",
                             layer_conf->loudspeaker_layout);
      log += write_yaml_form(log, 3, "output_gain_is_present_flag: %u",
                             layer_conf->output_gain_flag);
      log += write_yaml_form(log, 3, "recon_gain_is_present_flag: %u",
                             layer_conf->recon_gain_flag);
      log += write_yaml_form(log, 3, "substream_count: %u",
                             layer_conf->nb_substreams);
      log += write_yaml_form(log, 3, "coupled_substream_count: %u",
                             layer_conf->nb_coupled_substreams);
      if (layer_conf->output_gain_flag) {
        log += write_yaml_form(log, 3, "output_gain_flag: %u",
                               layer_conf->output_gain_info->output_gain_flag);
        log += write_yaml_form(log, 3, "output_gain: %d",
                               layer_conf->output_gain_info->output_gain);
      }
    }
  } else if (ae_obu->element_type == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
    // log += write_yaml_form(log, 0, "- scene_based:");
    log += write_yaml_form(log, 1, "ambisonics_config:");

    AmbisonicsConf* ambisonics_conf = ae_obu->ambisonics_conf;
    log += write_yaml_form(log, 2, "ambisonics_mode: %llu",
                           ambisonics_conf->ambisonics_mode);
    if (ambisonics_conf->ambisonics_mode == AMBISONICS_MONO) {
      log += write_yaml_form(log, 2, "ambisonics_mono_config:");
      log += write_yaml_form(log, 3, "output_channel_count: %u",
                             ambisonics_conf->output_channel_count);
      log += write_yaml_form(log, 3, "substream_count: %u",
                             ambisonics_conf->substream_count);

      log += write_yaml_form(log, 3, "channel_mapping:");
      for (uint64_t i = 0; i < ambisonics_conf->mapping_size; ++i) {
        log += write_yaml_form(log, 3, "- %u", ambisonics_conf->mapping[i]);
      }
    } else if (ambisonics_conf->ambisonics_mode == AMBISONICS_PROJECTION) {
      log += write_yaml_form(log, 2, "ambisonics_projection_config:");
      log += write_yaml_form(log, 3, "output_channel_count: %u",
                             ambisonics_conf->output_channel_count);
      log += write_yaml_form(log, 3, "substream_count: %u",
                             ambisonics_conf->substream_count);
      log += write_yaml_form(log, 3, "coupled_substream_count: %u",
                             ambisonics_conf->coupled_substream_count);
      log += write_yaml_form(log, 3, "demixing_matrix:");
      for (uint64_t i = 0; i < ambisonics_conf->mapping_size; i += 2) {
        int16_t value = (ambisonics_conf->mapping[i] << 8) |
                        ambisonics_conf->mapping[i + 1];
        log += write_yaml_form(log, 3, "- %d", value);
      }
    }
  }

  write_postfix(LOG_OBU, log);
}

static void write_mix_presentation_log(uint64_t idx, void* obu, char* log) {
  IAMF_MixPresentation* mp_obu = (IAMF_MixPresentation*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "MixPresentationOBU_%llu:", idx);
  log += write_yaml_form(log, 0, "- mix_presentation_id: %llu",
                         mp_obu->mix_presentation_id);
  log += write_yaml_form(log, 1, "count_label: %llu", mp_obu->num_labels);
  log += write_yaml_form(log, 1, "language_labels:");
  for (uint64_t i = 0; i < mp_obu->num_labels; ++i) {
    log += write_yaml_form(log, 1, "- \"%s\"", mp_obu->language[i]);
  }
  log += write_yaml_form(log, 1, "mix_presentation_annotations_array:");
  for (uint64_t i = 0; i < mp_obu->num_labels; ++i) {
    log += write_yaml_form(log, 1, "- mix_presentation_annotations:");
    log += write_yaml_form(log, 2, "mix_presentation_friendly_label: \"%s\"",
                           mp_obu->mix_presentation_friendly_label[i]);
  }
  log += write_yaml_form(log, 1, "num_sub_mixes: %llu", mp_obu->num_sub_mixes);
  log += write_yaml_form(log, 1, "sub_mixes:");
  for (uint64_t i = 0; i < mp_obu->num_sub_mixes; ++i) {
    SubMixPresentation* submix = &mp_obu->sub_mixes[i];
    log += write_yaml_form(log, 1, "- num_audio_elements: %llu",
                           submix->nb_elements);
    log += write_yaml_form(log, 2, "audio_elements:");
    for (uint64_t j = 0; j < submix->nb_elements; ++j) {
      ElementConf* conf_s = &submix->conf_s[j];
      log += write_yaml_form(log, 2, "- audio_element_id: %llu",
                             conf_s->element_id);
      log += write_yaml_form(log, 3,
                             "mix_presentation_element_annotations_array:");
      for (uint64_t k = 0; k < mp_obu->num_labels; ++k) {
        log +=
            write_yaml_form(log, 3, "- mix_presentation_element_annotations:");
        log += write_yaml_form(log, 4, "audio_element_friendly_label: \"%s\"",
                               conf_s->audio_element_friendly_label[k]);
      }

      log += write_yaml_form(log, 3, "rendering_config:");
      log += write_yaml_form(log, 4, "headphones_rendering_mode: %u",
                             conf_s->conf_r.headphones_rendering_mode);
      log += write_yaml_form(log, 4, "rendering_config_extension_size: %u",
                             conf_s->conf_r.rendering_config_extension_size);

      log += write_yaml_form(log, 3, "element_mix_config:");
      log += write_yaml_form(log, 4, "mix_gain:");
      log += write_yaml_form(log, 5, "param_definition:");
      log += write_yaml_form(log, 6, "parameter_id: %llu",
                             conf_s->conf_m.gain.base.id);
      log += write_yaml_form(log, 6, "parameter_rate: %llu",
                             conf_s->conf_m.gain.base.rate);
      log += write_yaml_form(log, 6, "param_definition_mode: %u",
                             conf_s->conf_m.gain.base.mode);
      if (conf_s->conf_m.gain.base.mode == 0) {
        log += write_yaml_form(log, 6, "duration: %llu",
                               conf_s->conf_m.gain.base.duration);
        log += write_yaml_form(log, 6, "num_subblocks: %llu",
                               conf_s->conf_m.gain.base.nb_segments);
        log +=
            write_yaml_form(log, 6, "constant_subblock_duration: %llu",
                            conf_s->conf_m.gain.base.constant_segment_interval);
        if (conf_s->conf_m.gain.base.constant_segment_interval == 0) {
          log += write_yaml_form(log, 6, "subblock_durations:");
          for (uint64_t k = 0; k < conf_s->conf_m.gain.base.nb_segments; ++k) {
            log += write_yaml_form(
                log, 6, "- %llu",
                conf_s->conf_m.gain.base.segments[k].segment_interval);
          }
        }
      }
      log += write_yaml_form(log, 5, "default_mix_gain: %d",
                             conf_s->conf_m.gain.mix_gain);
    }

    OutputMixConf* output_mix_config = &submix->output_mix_config;
    log += write_yaml_form(log, 2, "output_mix_config:");
    log += write_yaml_form(log, 3, "output_mix_gain:");
    log += write_yaml_form(log, 4, "param_definition:");
    log += write_yaml_form(log, 5, "parameter_id: %llu",
                           output_mix_config->gain.base.id);
    log += write_yaml_form(log, 5, "parameter_rate: %llu",
                           output_mix_config->gain.base.rate);
    log += write_yaml_form(log, 5, "param_definition_mode: %u",
                           output_mix_config->gain.base.mode);
    if (output_mix_config->gain.base.mode == 0) {
      log += write_yaml_form(log, 5, "duration: %llu",
                             output_mix_config->gain.base.duration);
      log += write_yaml_form(log, 5, "num_subblocks: %llu",
                             output_mix_config->gain.base.nb_segments);
      log += write_yaml_form(
          log, 5, "constant_subblock_duration: %llu",
          output_mix_config->gain.base.constant_segment_interval);
      if (output_mix_config->gain.base.constant_segment_interval == 0) {
        log += write_yaml_form(log, 5, "subblock_durations:");
        for (uint64_t k = 0; k < output_mix_config->gain.base.nb_segments;
             ++k) {
          log += write_yaml_form(
              log, 5, "- %llu",
              output_mix_config->gain.base.segments[k].segment_interval);
        }
      }
    }
    log += write_yaml_form(log, 4, "default_mix_gain: %d",
                           output_mix_config->gain.mix_gain);

    log += write_yaml_form(log, 2, "num_layouts: %llu", submix->num_layouts);

    log += write_yaml_form(log, 2, "layouts:");
    for (uint64_t j = 0; j < submix->num_layouts; ++j) {
      if (!submix->layouts[j]) continue;
      // layout
      log += write_yaml_form(log, 2, "- loudness_layout:");

      uint32_t layout_type = submix->layouts[j]->type;
      log += write_yaml_form(log, 4, "layout_type: %lu", layout_type);

      if (layout_type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
        log += write_yaml_form(log, 4, "ss_layout:");

        SoundSystemLayout* ss = SOUND_SYSTEM_LAYOUT(submix->layouts[j]);
        log += write_yaml_form(log, 5, "sound_system: %u", ss->sound_system);
      }

      // loudness
      log += write_yaml_form(log, 3, "loudness:");
      log += write_yaml_form(log, 4, "info_type_bit_masks: [%u]",
                             submix->loudness[j].info_type);
      log += write_yaml_form(log, 4, "integrated_loudness: %d",
                             submix->loudness[j].integrated_loudness);
      log += write_yaml_form(log, 4, "digital_peak: %d",
                             submix->loudness[j].digital_peak);
      if (submix->loudness[j].info_type & 1) {
        log += write_yaml_form(log, 4, "true_peak: %d",
                               submix->loudness[j].true_peak);
      }

      if (submix->loudness[j].info_type & 2) {
        log += write_yaml_form(log, 4, "anchored_loudness:");
        log += write_yaml_form(log, 5, "num_anchored_loudness: %u",
                               submix->loudness[j].num_anchor_loudness);

        if (submix->loudness[j].num_anchor_loudness) {
          log += write_yaml_form(log, 5, "anchor_elements:");
          for (uint8_t k = 0; k < submix->loudness[j].num_anchor_loudness;
               ++k) {
            anchor_loudness_t anchor_loudness_info =
                submix->loudness[j].anchor_loudness[k];
            log += write_yaml_form(log, 5, "- anchor_element: %u",
                                   anchor_loudness_info.anchor_element);
            log += write_yaml_form(log, 6, "anchored_loudness: %d",
                                   anchor_loudness_info.anchored_loudness);
          }
        }
      }
    }
  }
  write_postfix(LOG_OBU, log);
}

static void write_parameter_block_log(uint64_t idx, void* obu, char* log) {
  IAMF_Parameter* para = (IAMF_Parameter*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "ParameterBlockOBU_%llu:", idx);
  log += write_yaml_form(log, 0, "- parameter_id: %llu", para->id);
  log += write_yaml_form(log, 1, "duration: %llu", para->duration);
  log += write_yaml_form(log, 1, "num_subblocks: %llu", para->nb_segments);
  log += write_yaml_form(log, 1, "constant_subblock_duration: %llu",
                         para->constant_segment_interval);
  log += write_yaml_form(log, 1, "subblocks:");
  for (uint64_t i = 0; i < para->nb_segments; ++i) {
    if (para->type == IAMF_PARAMETER_TYPE_MIX_GAIN) {
      MixGainSegment* mg = (MixGainSegment*)para->segments[i];
      log += write_yaml_form(log, 1, "- mix_gain_parameter_data:");
      log += write_yaml_form(log, 3, "subblock_duration: %llu",
                             mg->seg.segment_interval);
      log += write_yaml_form(log, 3, "animation_type: %llu",
                             mg->mix_gain.animated_type);
      log += write_yaml_form(log, 3, "param_data:");
      if (mg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_STEP) {
        log += write_yaml_form(log, 4, "step:");
        log += write_yaml_form(log, 5, "start_point_value: %d",
                               mg->mix_gain.start);
      } else if (mg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_LINEAR) {
        log += write_yaml_form(log, 4, "linear:");
        log += write_yaml_form(log, 5, "start_point_value: %d",
                               mg->mix_gain.start);
        log += write_yaml_form(log, 5, "end_point_value: %d", mg->mix_gain.end);
      } else if (mg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_BEZIER) {
        log += write_yaml_form(log, 4, "bezier:");
        log += write_yaml_form(log, 5, "start_point_value: %d",
                               mg->mix_gain.start);
        log += write_yaml_form(log, 5, "end_point_value: %d", mg->mix_gain.end);
        log += write_yaml_form(log, 5, "control_point_value: %d",
                               mg->mix_gain.control);
        log += write_yaml_form(log, 5, "control_point_relative_time: %u",
                               mg->mix_gain.control_relative_time & 0xFF);
      }
    } else if (para->type == IAMF_PARAMETER_TYPE_DEMIXING) {
      DemixingSegment* mode = (DemixingSegment*)para->segments[i];
      log += write_yaml_form(log, 1, "- demixing_info_parameter_data:");
      log += write_yaml_form(log, 3, "subblock_duration: %llu",
                             mode->seg.segment_interval);
      log += write_yaml_form(log, 3, "dmixp_mode: %lu", mode->demixing_mode);
    } else if (para->type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      ReconGainSegment* mode = (ReconGainSegment*)para->segments[i];
      log += write_yaml_form(log, 1, "- recon_gain_info_parameter_data:");
      for (uint64_t j = 0; j < mode->list.count; ++j) {
        log += write_yaml_form(log, 3, "recon_gains_for_layer:");
        ReconGain recon_gain = mode->list.recon[j];
        int channels = bit1_count(recon_gain.flags);
        if (channels > 0) {
          // leb128
          // read 2 bytes
          uint8_t cur_channel = 0;

          uint32_t recon_channel = recon_gain.flags & 0xFFFF;
          if (recon_channel & 0x8000) {
            uint32_t ch = ((recon_channel & 0xFF00) >> 8);
            for (uint8_t k = 0; k < 7; ++k) {
              if (ch & 0x01) {
                log += write_yaml_form(log, 4, "recon_gain:");
                log += write_yaml_form(log, 5, "key: %u", k);
                log += write_yaml_form(log, 5, "value: %d",
                                       recon_gain.recon_gain[cur_channel++]);
              }
              ch >>= 1;
            }

            ch = (recon_channel & 0x00FF);
            for (uint8_t k = 7; k < 12; ++k) {
              if (ch & 0x01) {
                log += write_yaml_form(log, 4, "recon_gain:");
                log += write_yaml_form(log, 5, "key: %u", k);
                log += write_yaml_form(log, 5, "value: %d",
                                       recon_gain.recon_gain[cur_channel++]);
              }
              ch >>= 1;
            }
          } else {
            uint32_t ch = recon_channel & 0xFF;
            for (uint8_t k = 0; k < 7; ++k) {
              if (ch & 0x01) {
                log += write_yaml_form(log, 4, "recon_gain:");
                log += write_yaml_form(log, 5, "key: %u", k);
                log += write_yaml_form(log, 5, "value: %d",
                                       recon_gain.recon_gain[cur_channel++]);
              }
              ch >>= 1;
            }
          }
        }
      }
    }
  }

  write_postfix(LOG_OBU, log);
}

static void write_audio_frame_log(uint64_t idx, void* obu, char* log,
                                  uint64_t num_samples_to_trim_at_start,
                                  uint64_t num_samples_to_trim_at_end) {
  IAMF_Frame* af_obu = (IAMF_Frame*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "AudioFrameOBU_%llu:", idx);
  log += write_yaml_form(log, 0, "- audio_substream_id: %llu", af_obu->id);
  log += write_yaml_form(log, 1, "num_samples_to_trim_at_start: %llu",
                         num_samples_to_trim_at_start);
  log += write_yaml_form(log, 1, "num_samples_to_trim_at_end: %llu",
                         num_samples_to_trim_at_end);
  log += write_yaml_form(log, 1, "size_of_audio_frame: %u", af_obu->size);
  write_postfix(LOG_OBU, log);
}

static void write_temporal_delimiter_block_log(uint64_t idx, void* obu,
                                               char* log) {
  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "TemporalDelimiterOBU_%llu:", idx);
  write_postfix(LOG_OBU, log);
}

#ifdef SYNC_OBU
static void write_sync_log(uint64_t idx, void* obu, char* log) {
  IAMF_Sync* sc_obu = (IAMF_Sync*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "SyncOBU_%llu:", idx);
  log +=
      write_yaml_form(log, 0, "- global_offset: %llu", sc_obu->global_offset);
  log += write_yaml_form(log, 1, "num_obu_ids: %llu", sc_obu->nb_obu_ids);

  log += write_yaml_form(log, 1, "sync_array:");
  for (uint64_t i = 0; i < sc_obu->nb_obu_ids; ++i) {
    log += write_yaml_form(log, 1, "- obu_id: %llu", sc_obu->objs[i].obu_id);
    log += write_yaml_form(log, 2, "obu_data_type: %u",
                           sc_obu->objs[i].obu_data_type);
    log += write_yaml_form(log, 2, "reinitialize_decoder: %u",
                           sc_obu->objs[i].reinitialize_decoder);
    log += write_yaml_form(log, 2, "relative_offset: %d",
                           sc_obu->objs[i].relative_offset);
  }
  write_postfix(LOG_OBU, log);
}
#endif

int vlog_obu(uint32_t obu_type, void* obu,
             uint64_t num_samples_to_trim_at_start,
             uint64_t num_samples_to_trim_at_end) {
  if (!is_vlog_file_open()) return -1;

  static uint64_t obu_count = 0;
  static char log[LOG_BUFFER_SIZE];
  uint64_t key;

  log[0] = 0;
  key = obu_count;

  switch (obu_type) {
    case IAMF_OBU_CODEC_CONFIG:
      write_codec_config_log(obu_count++, obu, log);
      break;
    case IAMF_OBU_AUDIO_ELEMENT:
      write_audio_element_log(obu_count++, obu, log);
      break;
    case IAMF_OBU_MIX_PRESENTATION:
      write_mix_presentation_log(obu_count++, obu, log);
      break;
    case IAMF_OBU_PARAMETER_BLOCK:
      write_parameter_block_log(obu_count++, obu, log);
      break;
    case IAMF_OBU_TEMPORAL_DELIMITER:
      write_temporal_delimiter_block_log(obu_count++, obu, log);
      break;
#ifdef SYNC_OBU
    case IAMF_OBU_SYNC:
      write_sync_log(obu_count++, obu, log);
      break;
#endif
    case IAMF_OBU_SEQUENCE_HEADER:
      write_sequence_header_log(obu_count++, obu, log);
      break;
    default:
      if (obu_type >= IAMF_OBU_AUDIO_FRAME &&
          obu_type <= IAMF_OBU_AUDIO_FRAME_ID17) {
        write_audio_frame_log(obu_count++, obu, log,
                              num_samples_to_trim_at_start,
                              num_samples_to_trim_at_end);
      }
      break;
  }

  return vlog_print(LOG_OBU, key, log);
}

int vlog_decop(char* decop_text) {
  if (!is_vlog_file_open()) return -1;

  static uint64_t decop_log_count = 0;
  static char log_b[LOG_BUFFER_SIZE];
  char* log = log_b;
  uint64_t key = decop_log_count++;

  log += write_prefix(LOG_DECOP, log);
  log += write_yaml_form(log, 0, "DECOP_%llu:", decop_log_count);
  log += write_yaml_form(log, 0, "- text: %s", decop_text);
  write_postfix(LOG_DECOP, log);

  return vlog_print(LOG_DECOP, key, log_b);
}
