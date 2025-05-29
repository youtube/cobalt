#ifndef	_ERRNO_H
#define _ERRNO_H

#include "build/build_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

#include <bits/errno.h>

#ifdef __GNUC__
#if !BUILDFLAG(IS_STARBOARD)
__attribute__((const))
#endif
#endif
int *__errno_location(void);
#define errno (*__errno_location())

#ifdef _GNU_SOURCE
extern char *program_invocation_short_name, *program_invocation_name;
#endif

#ifdef __cplusplus
}
#endif

#endif
