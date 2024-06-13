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
 * @file dmemory.c
 * @brief Memory malloc and free.
 * @version 0.1
 * @date Created 03/03/2023
 **/


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
