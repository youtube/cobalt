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
