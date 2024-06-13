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
 * @file vlogging_tool_sr.h
 * @brief verification log generator.
 * @version 0.1
 * @date Created 03/29/2023
 **/

#include <stdarg.h>

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
int vlog_file_close();
int is_vlog_file_open();

int write_prefix(LOG_TYPE type, char* buf);
int write_postfix(LOG_TYPE type, char* buf);
int write_yaml_form(char* log, uint8_t indent, const char* format, ...);

#endif