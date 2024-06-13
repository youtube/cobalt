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
 * @file IAMF_debug.h
 * @brief Debug APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DEBUG_H
#define IAMF_DEBUG_H

#ifdef IA_DBG

#include <stdio.h>
#include <string.h>

#ifndef IA_TAG
#define IA_TAG "IAMF"
#endif

#define IA_DBG_E 0x01
#define IA_DBG_W 0x02
#define IA_DBG_I 0x04
#define IA_DBG_D 0x08
#define IA_DBG_T 0x10

#define IA_DBG_LEVEL 0

#ifdef IA_DEV
#include "IAMF_debug_dev.h"
#define obu_dump(a, b, c) OBU2F(a, b, c)
#else
#define obu_dump(a, b, c)
#endif

#ifndef __MODULE__
#define __MODULE__ \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#ifdef WIN32
#define ia_log(level, slevel, fmt, ...)                                 \
  do {                                                                  \
    if (IA_DBG_LEVEL & level) {                                             \
      fprintf(stderr, "[%-9s:%s:%s(%4d):%s]>" fmt "\n", IA_TAG, slevel, \
              __MODULE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);       \
    }                                                                   \
  } while (0)

#define ia_loge(fmt, ...) ia_log(IA_DBG_E, "ERROR", fmt, __VA_ARGS__)
#define ia_logw(fmt, ...) ia_log(IA_DBG_W, "WARN ", fmt, __VA_ARGS__)
#define ia_logi(fmt, ...) ia_log(IA_DBG_I, "INFO ", fmt, __VA_ARGS__)
#define ia_logd(fmt, ...) ia_log(IA_DBG_D, "DEBUG", fmt, __VA_ARGS__)
#define ia_logt(fmt, ...) ia_log(IA_DBG_T, "TRACE", fmt, __VA_ARGS__)
#else
#define ia_log(level, slevel, fmt, arg...)                              \
  do {                                                                  \
    if (IA_DBG_LEVEL & level) {                                         \
      fprintf(stderr, "[%-9s:%s:%s(%4d):%s]>" fmt "\n", IA_TAG, slevel, \
              __MODULE__, __LINE__, __FUNCTION__, ##arg);               \
    }                                                                   \
  } while (0)

#define ia_loge(fmt, arg...) ia_log(IA_DBG_E, "ERROR", fmt, ##arg)
#define ia_logw(fmt, arg...) ia_log(IA_DBG_W, "WARN ", fmt, ##arg)
#define ia_logi(fmt, arg...) ia_log(IA_DBG_I, "INFO ", fmt, ##arg)
#define ia_logd(fmt, arg...) ia_log(IA_DBG_D, "DEBUG", fmt, ##arg)
#define ia_logt(fmt, arg...) ia_log(IA_DBG_T, "TRACE", fmt, ##arg)
#endif

#else
#ifdef WIN32
#define ia_loge(fmt, ...)
#define ia_logw(fmt, ...)
#define ia_logi(fmt, ...)
#define ia_logd(fmt, ...)
#define ia_logt(fmt, ...)
#else
#define ia_loge(fmt, arg...)
#define ia_logw(fmt, arg...)
#define ia_logi(fmt, arg...)
#define ia_logd(fmt, arg...)
#define ia_logt(fmt, arg...)
#endif
#define obu_dump(a, b, c)

#endif

#endif /*  IAMF_DEBUG_H */
