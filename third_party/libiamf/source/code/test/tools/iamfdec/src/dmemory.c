/*
 * $Id: debug.c,v 1.5 2006/01/26 02:16:28 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 */
// https://www.codeproject.com/Articles/38340/Immediate-memory-corruption-detection

#if defined(_WIN32)
//#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
//#include "debug.h"

static int _syslog = 0;
static int _debug = 0;

void mc_set_debug(int debug) { _debug = debug; }
int mc_get_debug(void) { return _debug; }

extern void mc_set_syslog(int syslog) { _syslog = syslog; }

void mc_debug(const char *msg, ...) {
  va_list ap;
  if (_debug) {
    va_start(ap, msg);
#if HAVE_VSYSLOG
    if (_syslog) {
      vsyslog(LOG_DEBUG, msg, ap);
    } else
#endif
      vprintf(msg, ap);
    va_end(ap);
  }
}

void mc_error(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
#if HAVE_VSYSLOG
  if (_syslog) {
    vsyslog(LOG_ERR, msg, ap);
  } else
#endif
    vfprintf(stderr, msg, ap);
  va_end(ap);
}

void mc_info(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
#if HAVE_VSYSLOG
  if (_syslog) {
    vsyslog(LOG_INFO, msg, ap);
  } else
#endif
    vfprintf(stderr, msg, ap);
  va_end(ap);
}

#ifdef _DEBUG
#ifndef ASSERT
#define ASSERT(x)             \
  do {                        \
    if (!(x)) __debugbreak(); \
  } while (0)
#endif
#ifndef VERIFY
#define VERIFY(x) ASSERT(x)
#endif
#else
#ifndef ASSERT
#define ASSERT(x) \
  do {            \
    (x);          \
  } while (0)
#endif
#ifndef VERIFY
#define VERIFY(x) ASSERT(x)
#endif
#endif

static void *Allocate(size_t nSize);
static void Free(void *pPtr);

static int m_malloc_count;
static int m_report_on;

void *_dmalloc(int size, char *file, int line) { return (Allocate(size)); }

void *_dcalloc(int c, int size, char *file, int line) {
  void *ptr = Allocate(c * size);
  memset(ptr, 0, c * size);
  return (ptr);
}

void *_drealloc(char *old, int size, char *file, int line) {
  MEMORY_BASIC_INFORMATION mbi;
  void *ptr = Allocate(size);
  if (old) {
    VERIFY(VirtualQuery(old, &mbi, sizeof(mbi)) == sizeof(mbi));
    memcpy(ptr, old, mbi.RegionSize);
    Free(old);
  }
  return (ptr);
}

void _dfree(void *ptr, char *file, int line) { Free(ptr); }

char *_dstrdup(char *s) {
  int size;
  void *ptr = NULL;

  if (s) {
    size = strlen(s) + 1;
    ptr = Allocate(size);
    strcpy(ptr, s);
  }
  return (ptr);
}

void checkmmleak() { printf("m_malloc_count: %d\n", m_malloc_count); }

void setmmreport(int on) { m_report_on = on; }

static void *Allocate(size_t nSize) {
  void *pRet;

  pRet = VirtualAlloc(NULL, nSize, MEM_COMMIT, PAGE_READWRITE);
  if (!pRet) return NULL;
  m_malloc_count++;
  return pRet;
}

static void Free(void *pPtr) {
  MEMORY_BASIC_INFORMATION mbi;

  if (!pPtr) return;

  VERIFY(VirtualQuery(pPtr, &mbi, sizeof(mbi)) == sizeof(mbi));

  ASSERT(MEM_COMMIT == mbi.State);
  ASSERT(PAGE_READWRITE == mbi.Protect);
  m_malloc_count--;
  VERIFY(VirtualFree(pPtr, mbi.RegionSize, MEM_DECOMMIT));
}
#endif
