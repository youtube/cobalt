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

#include <stdarg.h>
#include <stdint.h>

#ifndef _VLOGGING_TOOL_SR_H_
#define _VLOGGING_TOOL_SR_H_

typedef enum LOG_TYPE {
  LOG_OBU = 0,
  LOG_MP4BOX = 1,
  LOG_DECOP = 2,
  MAX_LOG_TYPE
} LOG_TYPE;

int vlog_file_open(const char* log_file_name);
int vlog_print(LOG_TYPE type, uint64_t key, const char* format, ...);
int vlog_obu(uint32_t obu_type, void* obu,
             uint64_t num_samples_to_trim_at_start,
             uint64_t num_samples_to_trim_at_end);
int vlog_decop(char* decop_text);
int vlog_file_close();
int is_vlog_file_open();

int write_prefix(LOG_TYPE type, char* buf);
int write_postfix(LOG_TYPE type, char* buf);
int write_yaml_form(char* log, uint8_t indent, const char* format, ...);

#endif