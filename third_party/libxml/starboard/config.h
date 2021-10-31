#include "starboard/character.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/double.h"
#include "starboard/memory.h"
#include "starboard/string.h"
#include "starboard/types.h"

/* Define if getaddrinfo is there */
#undef HAVE_GETADDRINFO

/* Define to 1 if you have the <ansidecl.h> header file. */
#undef HAVE_ANSIDECL_H

/* Define to 1 if you have the <arpa/inet.h> header file. */
#undef HAVE_ARPA_INET_H

/* Define to 1 if you have the <arpa/nameser.h> header file. */
#undef HAVE_ARPA_NAMESER_H

/* Whether struct sockaddr::__ss_family exists */
#undef HAVE_BROKEN_SS_FAMILY

/* Define to 1 if you have the `class' function. */
#undef HAVE_CLASS

/* Define to 1 if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define to 1 if you have the <dirent.h> header file. */
#undef HAVE_DIRENT_H

/* Define to 1 if you have the <dlfcn.h> header file. */
#undef HAVE_DLFCN_H

/* Have dlopen based dso */
#undef HAVE_DLOPEN

/* Define to 1 if you have the <dl.h> header file. */
#undef HAVE_DL_H

/* Define to 1 if you have the <errno.h> header file. */
#undef HAVE_ERRNO_H

/* Define to 1 if you have the <fcntl.h> header file. */
#undef HAVE_FCNTL_H

/* Define to 1 if you have the `finite' function. */
#undef HAVE_FINITE

/* Define to 1 if you have the <float.h> header file. */
#undef HAVE_FLOAT_H

/* Define to 1 if you have the `fpclass' function. */
#undef HAVE_FPCLASS

/* Define to 1 if you have the `fprintf' function. */
#undef HAVE_FPRINTF

/* Define to 1 if you have the `fp_class' function. */
#undef HAVE_FP_CLASS

/* Define to 1 if you have the <fp_class.h> header file. */
#undef HAVE_FP_CLASS_H

/* Define to 1 if you have the `ftime' function. */
#undef HAVE_FTIME

/* Define if getaddrinfo is there */
#undef HAVE_GETADDRINFO

/* Define to 1 if you have the `gettimeofday' function. */
#undef HAVE_GETTIMEOFDAY

/* Define to 1 if you have the <ieeefp.h> header file. */
#undef HAVE_IEEEFP_H

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if you have the <inttypes.h.h> header file. */
#undef HAVE_INTTYPES_H_H

/* Define if isinf is there */
#undef HAVE_ISINF

/* Define if isnan is there */
#undef HAVE_ISNAN

/* Define to 1 if you have the `isnand' function. */
#undef HAVE_ISNAND

/* Define if history library is there (-lhistory) */
#undef HAVE_LIBHISTORY

/* Define if pthread library is there (-lpthread) */
#undef HAVE_LIBPTHREAD

/* Define if readline library is there (-lreadline) */
#undef HAVE_LIBREADLINE

/* Have compression library */
/* NOTE: We actually do have this, but do not need the integration. */
#undef HAVE_LIBZ

/* Define to 1 if you have the <limits.h> header file. */
#undef HAVE_LIMITS_H

/* Define to 1 if you have the `localtime' function. */
#undef HAVE_LOCALTIME

/* Define to 1 if you have the <malloc.h> header file. */
#undef HAVE_MALLOC_H

/* Define to 1 if you have the <math.h> header file. */
#define HAVE_MATH_H 1

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define to 1 if you have the <nan.h> header file. */
#undef HAVE_NAN_H

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
#undef HAVE_NDIR_H

/* Define to 1 if you have the <netdb.h> header file. */
#undef HAVE_NETDB_H

/* Define to 1 if you have the <netinet/in.h> header file. */
#undef HAVE_NETINET_IN_H

/* Define to 1 if you have the <poll.h> header file. */
#undef HAVE_POLL_H

/* Define to 1 if you have the `printf' function. */
#undef HAVE_PRINTF

/* Define if <pthread.h> is there */
#undef HAVE_PTHREAD_H

/* Define to 1 if you have the <resolv.h> header file. */
#undef HAVE_RESOLV_H

/* Have shl_load based dso */
#undef HAVE_SHLLOAD

/* Define to 1 if you have the `signal' function. */
#undef HAVE_SIGNAL

/* Define to 1 if you have the <signal.h> header file. */
#undef HAVE_SIGNAL_H

/* Define to 1 if you have the `snprintf' function. */
#undef HAVE_SNPRINTF

/* Define to 1 if you have the `sprintf' function. */
#undef HAVE_SPRINTF

/* Define to 1 if you have the `srand' function. */
#undef HAVE_SRAND

/* Define to 1 if you have the `sscanf' function. */
#undef HAVE_SSCANF

/* Define to 1 if you have the `stat' function. */
#undef HAVE_STAT

/* Define to 1 if you have the <stdarg.h> header file. */
#if SB_HAS(STDARG_H)
#define HAVE_STDARG_H 1
#else
#undef HAVE_STDARG_H
#endif

/* Define to 1 if you have the <stdarg.h> header file. */
#undef HAVE_STDARG_H

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define to 1 if you have the `strdup' function. */
#undef HAVE_STRDUP

/* Define to 1 if you have the `strerror' function. */
#undef HAVE_STRERROR

/* Define to 1 if you have the `strftime' function. */
#undef HAVE_STRFTIME

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define to 1 if you have the `strndup' function. */
#undef HAVE_STRNDUP

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
#undef HAVE_SYS_DIR_H

/* Define to 1 if you have the <sys/mman.h> header file. */
#undef HAVE_SYS_MMAN_H

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
#undef HAVE_SYS_NDIR_H

/* Define to 1 if you have the <sys/select.h> header file. */
#undef HAVE_SYS_SELECT_H

/* Define to 1 if you have the <sys/socket.h> header file. */
#undef HAVE_SYS_SOCKET_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/timeb.h> header file. */
#undef HAVE_SYS_TIMEB_H

/* Define to 1 if you have the <sys/time.h> header file. */
#undef HAVE_SYS_TIME_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the `time' function. */
#undef HAVE_TIME

/* Define to 1 if you have the <time.h> header file. */
#undef HAVE_TIME_H

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

#undef HAVE_VA_COPY

/* Define to 1 if you have the `vfprintf' function. */
#undef HAVE_VFPRINTF

/* Define to 1 if you have the `vsnprintf' function. */
#undef HAVE_VSNPRINTF

/* Define to 1 if you have the `vsprintf' function. */
#undef HAVE_VSPRINTF

/* Define to 1 if you have the <zlib.h> header file. */
/* #define HAVE_ZLIB_H 1 */

/* Define to 1 if you have the `_stat' function. */
 #undef HAVE__STAT 

/* Whether __va_copy() is available */
 #undef HAVE___VA_COPY 

/* Define as const if the declaration of iconv() needs const. */
#undef ICONV_CONST

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "libxml2"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Type cast for the send() function 2nd arg */
#define SEND_ARG2_CAST /**/

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Support for IPv6 */
#undef SUPPORT_IP6

/* Define if va_list is an array type */
#define VA_LIST_IS_ARRAY 1

/* Version number of package */
#define VERSION "2.9.10"

/* Determine what socket length (socklen_t) data type is */
#undef XML_SOCKLEN_T

/* Define for Solaris 2.5.1 so the uint32_t typedef from <sys/synch.h>,
   <pthread.h>, or <semaphore.h> is not used. If the typedef were allowed, the
   #define below would cause a syntax error. */
/* #undef _UINT32_T */

/* Using the Win32 Socket implementation */
/* #undef _WINSOCKAPI_ */

/* ss_family is not defined here, use __ss_family instead */
/* #undef ss_family */

/* Win32 Std C name mangling work-around */
/* #undef vsnprintf */

/* Define to 1 if you have the <stdio.h> header file. */
#undef HAVE_STDIO_H

/* Define to 1 if you have getenv. If not defined, some features will be
 * disabled */
#undef HAVE_GETENV

/* toupper() wrapping */
#define XML_TOUPPER toupper

/* floor() wrapping */
#define XML_FLOOR floor

/* fabs() wrapping */
#define XML_FABS fabs

/* labs() wrapping */
#define XML_LABS labs

/* malloc() wrapping */
#define XML_MALLOC SbMemoryAllocate

/* realloc() wrapping */
#define XML_REALLOC SbMemoryReallocate

/* free() wrapping */
#define XML_FREE SbMemoryDeallocate

/* memcpy() wrapping */
#define XML_MEMCPY memcpy

/* memchr() wrapping */
#define XML_MEMCHR memchr

/* memcmp() wrapping */
#define XML_MEMCMP memcmp

/* memcpy() wrapping */
#define XML_MEMSET memset

/* memmove() wrapping */
#define XML_MEMMOVE memmove

/* snprintf() wrapping */
#define XML_SNPRINTF snprintf

/* sscanf() wrapping */
#define XML_SSCANF sscanf

/* strlen() wrapping */
#define XML_STRLEN strlen

/* strncpy() wrapping */
#define XML_STRNCPY strncpy

/* strncat() wrapping */
#define XML_STRNCAT strncat

/* strcmp() wrapping */
#define XML_STRCMP strcmp

/* strncmp() wrapping */
#define XML_STRNCMP strncmp

/* strchr() wrapping */
#define XML_STRCHR strchr

/* vsnprintf() wrapping */
#define XML_VSNPRINTF vsnprintf
