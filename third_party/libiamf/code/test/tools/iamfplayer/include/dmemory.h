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
 * @file dmemory.h
 * @brief Memory malloc and free.
 * @version 0.1
 * @date Created 03/03/2023
 **/


#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32

#define _drealloc(a, b, c, d) realloc(a, b)
#define _dmalloc(a, b, c) malloc(a)
#define _dcalloc(a, b, c, d) calloc(a, b)
#define _dfree(a, b, c) free(a)

#else

#ifndef JSON_EXPORT
#if defined(_MSC_VER)
#define JSON_EXPORT __declspec(dllexport)
#else
#define JSON_EXPORT extern
#endif
#endif

void mc_set_debug(int debug);
int mc_get_debug(void);

void mc_set_syslog(int syslog);

void mc_debug(const char *msg, ...);
void mc_error(const char *msg, ...);
void mc_info(const char *msg, ...);

#ifndef __STRING
#define __STRING(x) #x
#endif

#ifndef PARSER_BROKEN_FIXED

#define JASSERT(cond) \
  do {                \
  } while (0)

#else

#define JASSERT(cond)                                                       \
  do {                                                                      \
    if (!(cond)) {                                                          \
      mc_error(                                                             \
          "cjson assert failure %s:%d : cond \"" __STRING(cond) "failed\n", \
          __FILE__, __LINE__);                                              \
      *(int *)0 = 1;                                                        \
      abort();                                                              \
    }                                                                       \
  } while (0)

#endif

#define MC_ERROR(x, ...) mc_error(x, ##__VA_ARGS__)

#ifdef MC_MAINTAINER_MODE
#define MC_SET_DEBUG(x) mc_set_debug(x)
#define MC_GET_DEBUG() mc_get_debug()
#define MC_SET_SYSLOG(x) mc_set_syslog(x)
#define MC_DEBUG(x, ...) mc_debug(x, ##__VA_ARGS__)
#define MC_INFO(x, ...) mc_info(x, ##__VA_ARGS__)
#else
#define MC_SET_DEBUG(x) \
  if (0) mc_set_debug(x)
#define MC_GET_DEBUG() (0)
#define MC_SET_SYSLOG(x) \
  if (0) mc_set_syslog(x)
#define MC_DEBUG(x, ...) \
  if (0) mc_debug(x, ##__VA_ARGS__)
#define MC_INFO(x, ...) \
  if (0) mc_info(x, ##__VA_ARGS__)
#endif

void *_dmalloc(int size, char *file, int line);
void *_dcalloc(int c, int size, char *file, int line);
void *_drealloc(char *old, int size, char *file, int line);
void _dfree(void *ptr, char *file, int line);
char *_dstrdup(char *s);
void checkmmleak();
void setmmreport(int on);

#endif

#ifdef __cplusplus
}
#endif

#endif
