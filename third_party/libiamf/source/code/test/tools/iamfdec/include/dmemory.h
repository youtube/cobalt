/*
* $Id: debug.h,v 1.5 2006/01/30 23:07:57 mclark Exp $
*
* Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
* Michael Clark <michael@metaparadigm.com>
* Copyright (c) 2009 Hewlett-Packard Development Company, L.P.
*
* This library is free software; you can redistribute it and/or modify
* it under the terms of the MIT license. See COPYING for details.
*
*/

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
