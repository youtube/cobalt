#ifndef	_SYS_UTSNAME_H
#define	_SYS_UTSNAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>

struct utsname {
#if defined(STARBOARD)
	char sysname[257];
	char nodename[257];
	char release[257];
	char version[257];
	char machine[257];
	char domainname[257];
#else  // defined(STARBOARD)
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
#ifdef _GNU_SOURCE
	char domainname[65];
#else
	char __domainname[65];
#endif
#endif  // defined(STARBOARD)
};

int uname (struct utsname *);

#ifdef __cplusplus
}
#endif

#endif
