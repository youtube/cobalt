#ifndef LANGINFO_H
#define LANGINFO_H

#include "../../include/langinfo.h"

#if defined(STARBOARD)
// This function is implemented in Cobalt libc.
#define __nl_langinfo_l(item, locale) nl_langinfo_l(item, locale)
#else
char *__nl_langinfo_l(nl_item, locale_t);
#endif  // defined(STARBOARD)

#endif
