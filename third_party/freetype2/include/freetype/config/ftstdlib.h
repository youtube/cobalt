/****************************************************************************
 *
 * ftstdlib.h
 *
 *   ANSI-specific library and header configuration file (specification
 *   only).
 *
 * Copyright (C) 2002-2020 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


  /**************************************************************************
   *
   * This file is used to group all `#includes` to the ANSI~C library that
   * FreeType normally requires.  It also defines macros to rename the
   * standard functions within the FreeType source code.
   *
   * Load a file which defines `FTSTDLIB_H_` before this one to override it.
   *
   */


#ifndef FTSTDLIB_H_
#define FTSTDLIB_H_


#include <stddef.h>

#define ft_ptrdiff_t  ptrdiff_t


  /**************************************************************************
   *
   *                          integer limits
   *
   * `UINT_MAX` and `ULONG_MAX` are used to automatically compute the size of
   * `int` and `long` in bytes at compile-time.  So far, this works for all
   * platforms the library has been tested on.
   *
   * Note that on the extremely rare platforms that do not provide integer
   * types that are _exactly_ 16 and 32~bits wide (e.g., some old Crays where
   * `int` is 36~bits), we do not make any guarantee about the correct
   * behaviour of FreeType~2 with all fonts.
   *
   * In these cases, `ftconfig.h` will refuse to compile anyway with a
   * message like 'couldn't find 32-bit type' or something similar.
   *
   */


#include <limits.h>

#define FT_CHAR_BIT    CHAR_BIT
#define FT_USHORT_MAX  USHRT_MAX
#define FT_INT_MAX     INT_MAX
#define FT_INT_MIN     INT_MIN
#define FT_UINT_MAX    UINT_MAX
#define FT_LONG_MIN    LONG_MIN
#define FT_LONG_MAX    LONG_MAX
#define FT_ULONG_MAX   ULONG_MAX


  /**************************************************************************
   *
   *                character and string processing
   *
   */
#include <string.h>

#define ft_memchr memchr
#define ft_memcmp   memcmp
#define ft_memcpy   memcpy
#define ft_memmove  memmove
#define ft_memset   memset
#define ft_strcat   strcat
#define ft_strcmp   strcmp
#define ft_strcpy   strcpy
#define ft_strlen   strlen
#define ft_strncmp  strncmp
#define ft_strncpy  strncpy
#define ft_strrchr  strrchr
#define ft_strstr   strstr

  /**************************************************************************
   *
   *                          file handling
   *
   */

#if defined(STARBOARD)
#include "starboard/string.h"
#include "starboard/file.h"

#ifndef SEEK_SET
#define SEEK_SET    kSbFileFromBegin
#define SEEK_CUR    kSbFileFromCurrent
#define SEEK_END    kSbFileFromEnd
#endif
#define FT_FILE     SbFilePrivate
#define ft_fclose   SbFileClose
#define ft_fopen(p, m) SbFileOpen((p), SbFileModeStringToFlags(m), NULL, NULL)
#define ft_fread(b, s, n, f) SbFileRead((f), (char *)(b), (s) * (n))
#define ft_fseek(f, o, w) SbFileSeek((f), (w), (o))
#define ft_ftell(f)    SbFileSeek((f), kSbFileFromCurrent, 0)
#define ft_sprintf  SbStringFormatUnsafeF
#else
#include <stdio.h>

#define FT_FILE     FILE
#define ft_fclose   fclose
#define ft_fopen    fopen
#define ft_fread    fread
#define ft_fseek    fseek
#define ft_ftell    ftell
#define ft_sprintf  sprintf
#endif


  /**************************************************************************
   *
   *                            sorting
   *
   */

#include <stdlib.h>
#define ft_qsort  qsort

  /**************************************************************************
   *
   *                       memory allocation
   *
   */

#if defined(STARBOARD)
#include "starboard/memory.h"
static SB_C_INLINE void *ft_scalloc(size_t nelem, size_t elsize) {
  size_t size = nelem * elsize;
  void *memory = SbMemoryAllocate(size);
  if (!memory) {
    return NULL;
  }

  memset(memory, 0, size);
  return memory;
}
#define ft_sfree     SbMemoryDeallocate
#define ft_smalloc   SbMemoryAllocate
#define ft_srealloc  SbMemoryReallocate
#else
#define ft_scalloc   calloc
#define ft_sfree     free
#define ft_smalloc   malloc
#define ft_srealloc  realloc
#endif


  /**************************************************************************
   *
   *                         miscellaneous
   *
   */
#include <stdlib.h>
#define ft_strtol  strtol 

#if defined(STARBOARD)
static SB_C_INLINE char* ft_getenv(const char* name){
  return NULL;
}
#else
#define ft_getenv  getenv
#endif


  /**************************************************************************
   *
   *                        execution control
   *
   */


#include <setjmp.h>

#define ft_jmp_buf     jmp_buf  /* note: this cannot be a typedef since  */
                                /*       `jmp_buf` is defined as a macro */
                                /*       on certain platforms            */

#define ft_longjmp     longjmp
#define ft_setjmp( b ) setjmp( *(ft_jmp_buf*) &(b) ) /* same thing here */


  /* The following is only used for debugging purposes, i.e., if   */
  /* `FT_DEBUG_LEVEL_ERROR` or `FT_DEBUG_LEVEL_TRACE` are defined. */

#include <stdarg.h>


#endif /* FTSTDLIB_H_ */


/* END */
