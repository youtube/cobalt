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
 * @file IAMF_debug.h
 * @brief Debug APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DEBUG_H
#define IAMF_DEBUG_H

#ifndef SUPPORT_VERIFIER
#define SUPPORT_VERIFIER 0
#endif

#include <stdio.h>
#include <string.h>

#if SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#ifndef __MODULE__
#define __MODULE__ \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#ifdef IA_DBG

#ifndef IA_TAG
#define IA_TAG "IAMF"
#endif

#define IA_DBG_E 0x01
#define IA_DBG_W 0x02
#define IA_DBG_I 0x04
#define IA_DBG_D 0x08
#define IA_DBG_T 0x10

#define IA_DBG_LEVEL (IA_DBG_E | IA_DBG_W)

#ifdef IA_DEV
#include "IAMF_debug_dev.h"
#define obu_dump(a, b, c) OBU2F(a, b, c)
#else
#define obu_dump(a, b, c)
#endif

#ifdef WIN32
#if SUPPORT_VERIFIER
#define ia_log(level, slevel, fmt, ...)                                     \
  do {                                                                      \
    if ((IA_DBG_E & level) || (IA_DBG_W & level)) {                         \
      char text[2048];                                                      \
      sprintf(text, "[%-9s:%s(%4d):%s]>" fmt, slevel, __MODULE__, __LINE__, \
              __FUNCTION__, ##__VA_ARGS__);                                 \
      vlog_decop(text);                                                     \
    }                                                                       \
    if (IA_DBG_LEVEL & level) {                                             \
      fprintf(stderr, "[%-9s:%s:%s(%4d):%s]>" fmt "\n", IA_TAG, slevel,     \
              __MODULE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);           \
    }                                                                       \
  } while (0)
#else  // !SUPPORT_VERIFIER
#define ia_log(level, slevel, fmt, ...)                                 \
  do {                                                                  \
    if (IA_DBG_LEVEL & level) {                                         \
      fprintf(stderr, "[%-9s:%s:%s(%4d):%s]>" fmt "\n", IA_TAG, slevel, \
              __MODULE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);       \
    }                                                                   \
  } while (0)
#endif
#define ia_loge(fmt, ...) ia_log(IA_DBG_E, "ERROR", fmt, __VA_ARGS__)
#define ia_logw(fmt, ...) ia_log(IA_DBG_W, "WARN ", fmt, __VA_ARGS__)
#define ia_logi(fmt, ...) ia_log(IA_DBG_I, "INFO ", fmt, __VA_ARGS__)
#define ia_logd(fmt, ...) ia_log(IA_DBG_D, "DEBUG", fmt, __VA_ARGS__)
#define ia_logt(fmt, ...) ia_log(IA_DBG_T, "TRACE", fmt, __VA_ARGS__)
#else
#if SUPPORT_VERIFIER
#define ia_log(level, slevel, fmt, arg...)                              \
  do {                                                                  \
    if ((IA_DBG_E & level) || (IA_DBG_W & level)) {                     \
      char text[2048];                                                  \
      sprintf(text, "[%-9s:%s(%4d):%s]>" fmt "\n", slevel, __MODULE__,  \
              __LINE__, __FUNCTION__, ##arg);                           \
      vlog_decop(text);                                                 \
    }                                                                   \
    if (IA_DBG_LEVEL & level) {                                         \
      fprintf(stderr, "[%-9s:%s:%s(%4d):%s]>" fmt "\n", IA_TAG, slevel, \
              __MODULE__, __LINE__, __FUNCTION__, ##arg);               \
    }                                                                   \
  } while (0)
#else  // !SUPPORT_VERIFIER
#define ia_log(level, slevel, fmt, arg...)                              \
  do {                                                                  \
    if (IA_DBG_LEVEL & level) {                                         \
      fprintf(stderr, "[%-9s:%s:%s(%4d):%s]>" fmt "\n", IA_TAG, slevel, \
              __MODULE__, __LINE__, __FUNCTION__, ##arg);               \
    }                                                                   \
  } while (0)
#endif
#define ia_loge(fmt, arg...) ia_log(IA_DBG_E, "ERROR", fmt, ##arg)
#define ia_logw(fmt, arg...) ia_log(IA_DBG_W, "WARN ", fmt, ##arg)
#define ia_logi(fmt, arg...) ia_log(IA_DBG_I, "INFO ", fmt, ##arg)
#define ia_logd(fmt, arg...) ia_log(IA_DBG_D, "DEBUG", fmt, ##arg)
#define ia_logt(fmt, arg...) ia_log(IA_DBG_T, "TRACE", fmt, ##arg)
#endif

#else
#ifdef WIN32
#if SUPPORT_VERIFIER
#define ia_log(level, slevel, fmt, ...)                                   \
  do {                                                                    \
    char text[2048];                                                      \
    sprintf(text, "[%-9s:%s(%4d):%s]>" fmt, slevel, __MODULE__, __LINE__, \
            __FUNCTION__, ##__VA_ARGS__);                                 \
    vlog_decop(text);                                                     \
  } while (0)
#define ia_loge(fmt, ...) ia_log(IA_DBG_E, "ERROR", fmt, __VA_ARGS__)
#define ia_logw(fmt, ...) ia_log(IA_DBG_W, "WARN ", fmt, __VA_ARGS__)
#else  // !SUPPORT_VERIFIER
#define ia_loge(fmt, ...)
#define ia_logw(fmt, ...)
#endif
#define ia_logi(fmt, ...)
#define ia_logd(fmt, ...)
#define ia_logt(fmt, ...)
#else  // !WIN32
#if SUPPORT_VERIFIER
#define ia_log(level, slevel, fmt, arg...)                                     \
  do {                                                                         \
    char text[2048];                                                           \
    sprintf(text, "[%-9s:%s(%4d):%s]>" fmt "\n", slevel, __MODULE__, __LINE__, \
            __FUNCTION__, ##arg);                                              \
  } while (0)
#define ia_loge(fmt, arg...) ia_log(IA_DBG_E, "ERROR", fmt, ##arg)
#define ia_logw(fmt, arg...) ia_log(IA_DBG_W, "WARN ", fmt, ##arg)
#else  // !SUPPORT_VERIFIER
#define ia_loge(fmt, ...)
#define ia_logw(fmt, ...)
#endif
#define ia_logi(fmt, arg...)
#define ia_logd(fmt, arg...)
#define ia_logt(fmt, arg...)
#endif
#define obu_dump(a, b, c)

#endif

#endif /*  IAMF_DEBUG_H */
