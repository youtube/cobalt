#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#if defined(STARBOARD)
#define syscall DO_NOT_USE_SYSCALL_IN_STARBOARD
#endif

#include <bits/syscall.h.in>

#endif
