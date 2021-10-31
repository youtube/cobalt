#ifndef _LOCALE_IMPL_H
#define _LOCALE_IMPL_H

#include <locale.h>
#include <stdlib.h>
#include "libc.h"

// Using a NULL value here is valid since this is ignored everywhere it is used.
#define CURRENT_LOCALE ((locale_t)0)

// Starboard supports UTF-16 and UTF-32, both of which have a maximum character
// size of 4 bytes.
#undef MB_CUR_MAX
#define MB_CUR_MAX 4

#endif
