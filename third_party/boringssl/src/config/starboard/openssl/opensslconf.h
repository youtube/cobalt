/* opensslconf.h */
/* WARNING: Edited heavily by hand, based on lbshell config. Meant for all
 * starboard platforms. */

#include "starboard/client_porting/eztime/eztime.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/file.h"
#include "starboard/memory.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "starboard/thread.h"
#include "starboard/time.h"

#ifndef OPENSSL_SYS_STARBOARD
# define OPENSSL_SYS_STARBOARD
#endif

/* NOTE: This file is included multiple times on purpose. Some source files
 * define macros before including it to cause different behavior. */

#undef L_ENDIAN
#undef B_ENDIAN
#if SB_IS(BIG_ENDIAN)
# define B_ENDIAN
#else  // SB_IS(BIG_ENDIAN)
# define L_ENDIAN
#endif  // SB_IS(BIG_ENDIAN)

#ifndef NO_SYS_TYPES_H
# define NO_SYS_TYPES_H
#endif

#ifndef OPENSSL_NO_POSIX_IO
# define OPENSSL_NO_POSIX_IO
#endif

// Benchmarking tool requires FP API to read certs and keys
//#ifndef OPENSSL_NO_FP_API
// # define OPENSSL_NO_FP_API
//#endif

#ifndef OPENSSL_NO_DSO
# define OPENSSL_NO_DSO
#endif

/* OpenSSL was configured with the following options: */
#ifndef OPENSSL_NO_CAMELLIA
# define OPENSSL_NO_CAMELLIA
#endif
#ifndef OPENSSL_NO_CAST
# define OPENSSL_NO_CAST
#endif
#ifndef OPENSSL_NO_CAPIENG
# define OPENSSL_NO_CAPIENG
#endif
#ifndef OPENSSL_NO_CMS
# define OPENSSL_NO_CMS
#endif
#ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
# define OPENSSL_NO_EC_NISTP_64_GCC_128
#endif
#ifndef OPENSSL_NO_FIPS
# define OPENSSL_NO_FIPS
#endif
#ifndef OPENSSL_NO_GMP
# define OPENSSL_NO_GMP
#endif
#ifndef OPENSSL_NO_IDEA
# define OPENSSL_NO_IDEA
#endif
#ifndef OPENSSL_NO_JPAKE
# define OPENSSL_NO_JPAKE
#endif
#ifndef OPENSSL_NO_KRB5
# define OPENSSL_NO_KRB5
#endif
#ifndef OPENSSL_NO_MDC2
# define OPENSSL_NO_MDC2
#endif
#ifndef OPENSSL_NO_RC5
# define OPENSSL_NO_RC5
#endif
#ifndef OPENSSL_NO_RFC3779
# define OPENSSL_NO_RFC3779
#endif
#ifndef OPENSSL_NO_SCTP
# define OPENSSL_NO_SCTP
#endif
#ifndef OPENSSL_NO_SEED
# define OPENSSL_NO_SEED
#endif
#ifndef OPENSSL_NO_SHA0
# define OPENSSL_NO_SHA0
#endif
#ifndef OPENSSL_NO_WHIRLPOOL
# define OPENSSL_NO_WHIRLPOOL
#endif

#ifndef OPENSSL_NO_DYNAMIC_ENGINE
# define OPENSSL_NO_DYNAMIC_ENGINE
#endif

/* The OPENSSL_NO_* macros are also defined as NO_* if the application
   asks for it.  This is a transient feature that is provided for those
   who haven't had the time to do the appropriate changes in their
   applications.  */
/* NOTE: This is actually used in evp.h! */
#ifdef OPENSSL_ALGORITHM_DEFINES
# if defined(OPENSSL_NO_CAMELLIA) && !defined(NO_CAMELLIA)
#  define NO_CAMELLIA
# endif
# if defined(OPENSSL_NO_CAPIENG) && !defined(NO_CAPIENG)
#  define NO_CAPIENG
# endif
# if defined(OPENSSL_NO_CMS) && !defined(NO_CMS)
#  define NO_CMS
# endif
# if defined(OPENSSL_NO_FIPS) && !defined(NO_FIPS)
#  define NO_FIPS
# endif
# if defined(OPENSSL_NO_GMP) && !defined(NO_GMP)
#  define NO_GMP
# endif
# if defined(OPENSSL_NO_IDEA) && !defined(NO_IDEA)
#  define NO_IDEA
# endif
# if defined(OPENSSL_NO_JPAKE) && !defined(NO_JPAKE)
#  define NO_JPAKE
# endif
# if defined(OPENSSL_NO_KRB5) && !defined(NO_KRB5)
#  define NO_KRB5
# endif
# if defined(OPENSSL_NO_MDC2) && !defined(NO_MDC2)
#  define NO_MDC2
# endif
# if defined(OPENSSL_NO_RC5) && !defined(NO_RC5)
#  define NO_RC5
# endif
# if defined(OPENSSL_NO_RFC3779) && !defined(NO_RFC3779)
#  define NO_RFC3779
# endif
# if defined(OPENSSL_NO_SEED) && !defined(NO_SEED)
#  define NO_SEED
# endif
#endif

/* crypto/opensslconf.h.in */

#undef I386_ONLY

#if defined(HEADER_CRYPTLIB_H) && !defined(OPENSSLDIR)
# define ENGINESDIR "/usr/local/ssl/lib/engines"
# define OPENSSLDIR "/usr/local/ssl"
#endif

// We should cause a compile error if we try to include UNISTD
#undef OPENSSL_UNISTD

#undef OPENSSL_EXPORT_VAR_AS_FUNCTION

#if (defined(HEADER_NEW_DES_H) || defined(HEADER_DES_H)) && !defined(DES_LONG)
/* If this is set to 'unsigned int' on a DEC Alpha, this gives about a
 * %20 speed up (longs are 8 bytes, int's are 4). */
# ifndef DES_LONG
#  define DES_LONG unsigned int
# endif
#endif

#if defined(HEADER_BN_H) && !defined(CONFIG_HEADER_BN_H)
# define CONFIG_HEADER_BN_H
# undef BN_LLONG
/* Only one for the following should be defined */
/* The prime number generation stuff may not work when
 * EIGHT_BIT but I don't care since I've only used this mode
 * for debuging the bignum libraries */
# undef SIXTY_FOUR_BIT_LONG
# undef SIXTY_FOUR_BIT
# undef THIRTY_TWO_BIT
# undef SIXTEEN_BIT
# undef EIGHT_BIT
# if SB_IS(64_BIT)
#  if SB_SIZE_OF(LONG) == 8
#   define SIXTY_FOUR_BIT_LONG
#  else  // SB_SIZE_OF(LONG) != 8
#   define SIXTY_FOUR_BIT
#  endif  // SB_SIZE_OF(LONG) == 8
# else  // SB_IS(64_BIT)
#  define BN_LLONG
#  define THIRTY_TWO_BIT
# endif  // SB_IS(64_BIT)
#endif // defined(HEADER_BN_H) && !defined(CONFIG_HEADER_BN_H)

#if defined(HEADER_DES_LOCL_H) && !defined(CONFIG_HEADER_DES_LOCL_H)
# define CONFIG_HEADER_DES_LOCL_H
# ifndef DES_DEFAULT_OPTIONS
/* the following is tweaked from a config script, that is why it is a
 * protected undef/define */
#  ifndef DES_PTR
#   define DES_PTR
#  endif
/* This helps C compiler generate the correct code for multiple functional
 * units.  It reduces register dependancies at the expense of 2 more
 * registers */
#  ifndef DES_RISC1
#   define DES_RISC1
#  endif
#  ifdef DES_RISC2
#   undef DES_RISC2
#  endif
#  if defined(DES_RISC1) && defined(DES_RISC2)
#   error YOU SHOULD NOT HAVE BOTH DES_RISC1 AND DES_RISC2 DEFINED!!!!!
#  endif

/* Unroll the inner loop, this sometimes helps, sometimes hinders.
 * Very mucy CPU dependant */
#  ifndef DES_UNROLL
#   define DES_UNROLL
#  endif
# endif /* DES_DEFAULT_OPTIONS */
#endif /* HEADER_DES_LOCL_H */

#ifndef OPENSSL_OPENSSL_CONFIG_STARBOARD_OPENSSL_OPENSSLCONF_H
#define OPENSSL_OPENSSL_CONFIG_STARBOARD_OPENSSL_OPENSSLCONF_H

// Types that need to be ported.

// Use EzTime simulated POSIX types.
#define OPENSSL_port_tm EzTimeExploded
#define OPENSSL_port_time_t EzTimeT
#define OPENSSL_port_timeval EzTimeValue

// Definitions for system calls that may need to be overridden.
#define OPENSSL_port_abort SbSystemBreakIntoDebugger
#define OPENSSL_port_assert(x) SB_DCHECK(x)
#define OPENSSL_port_free SbMemoryDeallocate
#define OPENSSL_port_getenv(x) NULL
#define OPENSSL_port_gettimeofday EzTimeValueGetNow
#define OPENSSL_port_gmtime_r EzTimeTExplodeUTC
#define OPENSSL_port_malloc SbMemoryAllocate
#define OPENSSL_port_printf SbLogFormatF
#define OPENSSL_port_printferr SbLogFormatF
#define OPENSSL_port_realloc SbMemoryReallocate
#define OPENSSL_port_sscanf SbStringScanF
#define OPENSSL_port_strcasecmp SbStringCompareNoCase
#define OPENSSL_port_strdup SbStringDuplicate
#define OPENSSL_port_strerror(x) ""
#define OPENSSL_port_strncasecmp SbStringCompareNoCaseN
#define OPENSSL_port_time EzTimeTGetNow

// Variables that need to be ported.
#define OPENSSL_port_errno SbSystemGetLastError()

#endif  // OPENSSL_OPENSSL_CONFIG_STARBOARD_OPENSSL_OPENSSLCONF_H
