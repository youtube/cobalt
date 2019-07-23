#include <locale.h>
#include <langinfo.h>

#include "libc.h"
#include "locale_impl.h"
#include "starboard/common/log.h"

// Starboard does not provide a mechanism to retrieve information about the
// current locale, other than the locale name.
char *__nl_langinfo_l(nl_item item, locale_t loc) {
  SB_NOTREACHED();
  return "";
}

char *__nl_langinfo(nl_item item) {
  SB_NOTREACHED();
  return "";
}

weak_alias(__nl_langinfo, nl_langinfo);
weak_alias(__nl_langinfo_l, nl_langinfo_l);
