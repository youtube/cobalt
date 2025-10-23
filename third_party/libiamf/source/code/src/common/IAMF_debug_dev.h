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
 * @file IAMF_debug_dev.h
 * @brief Debug APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DEBUG_DEV_H
#define IAMF_DEBUG_DEV_H

#ifdef IA_DBG_LEVEL
#undef IA_DBG_LEVEL
#endif

// #define IA_DBG_LEVEL 0
#define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W)
// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_I)
// #define IA_DBG_LEVEL (IA_DBG_E | IA_DBG_W | IA_DBG_I | IA_DBG_D)
// #define IA_DBG_LEVEL (IA_DBG_E|IA_DBG_W|IA_DBG_D|IA_DBG_I|IA_DBG_T)

#define DFN_SIZE 32

#define OBU2F(data, size, type)                                           \
  do {                                                                    \
    static int g_obu_count = 0;                                           \
    static char g_dump[DFN_SIZE];                                         \
    snprintf(g_dump, DFN_SIZE, "/tmp/obu/obu_%06d_%02d.dat", g_obu_count, \
             type);                                                       \
    FILE *f = fopen(g_dump, "w");                                         \
    if (f) {                                                              \
      fwrite(data, 1, size, f);                                           \
      fclose(f);                                                          \
    }                                                                     \
    ++g_obu_count;                                                        \
  } while (0)

#endif /* IAMF_DEBUG_DEV_H */
