#ifndef ERRNO_H
#define ERRNO_H

#include "../../include/errno.h"

// Prior to SB 16, ___errno_location was a weak alias for __errno_location in musl.
// Starting with SB 16, we no longer implement __errno_location in musl. Instead, we retrieve the __errno_location symbols from the system.
#if SB_API_VERSION < 16
#ifdef __GNUC__
__attribute__((const))
#endif
hidden int *___errno_location(void);

#undef errno
#define errno (*___errno_location())
#endif // SB_API_VERSION < 16

#endif